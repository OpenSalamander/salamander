// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "usermenu.h"
#include "execute.h"
#include "cfgdlg.h"
#include "zip.h"
#include "spl_file.h"

CSalamanderSafeFile SalSafeFile;

//*****************************************************************************
//
// CSalamanderSafeFile
//

BOOL CSalamanderSafeFile::SafeFileOpen(SAFE_FILE* file,
                                       const char* fileName,
                                       DWORD dwDesiredAccess,
                                       DWORD dwShareMode,
                                       DWORD dwCreationDisposition,
                                       DWORD dwFlagsAndAttributes,
                                       HWND hParent,
                                       DWORD flags,
                                       DWORD* pressedButton,
                                       DWORD* silentMask)
{
    CALL_STACK_MESSAGE7("CSalamanderSafeFile::SafeFileOpen(, %s, %u, %u, %u, %u, , %u, ,)",
                        fileName, dwDesiredAccess, dwShareMode, dwCreationDisposition,
                        dwFlagsAndAttributes, flags);

    // for errors like LOW_MEMORY we wish for the operation to end completely
    if (pressedButton != NULL)
        *pressedButton = DIALOG_CANCEL;

    HANDLE hFile;
    int fileNameLen = (int)strlen(fileName);
    do
    {
        hFile = fileNameLen >= MAX_PATH ? INVALID_HANDLE_VALUE : HANDLES_Q(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwFlagsAndAttributes, NULL));
        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD dlgRet;
            if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_OPEN) && ButtonsContainsSkip(flags))
                dlgRet = DIALOG_SKIP;
            else
            {
                DWORD lastError = fileNameLen >= MAX_PATH ? ERROR_FILENAME_EXCED_RANGE : GetLastError();
                dlgRet = DialogError(hParent, (flags & BUTTONS_MASK), fileName,
                                     GetErrorText(lastError), LoadStr(IDS_ERROROPENINGFILE));
            }
            switch (dlgRet)
            {
            case DIALOG_RETRY:
                break;
            case DIALOG_SKIPALL:
                if (silentMask != NULL)
                    *silentMask |= SILENT_SKIP_FILE_OPEN;
            case DIALOG_SKIP:
            default:
            {
                if (pressedButton != NULL)
                    *pressedButton = dlgRet;
                return FALSE;
            }
            }
        }
    } while (hFile == INVALID_HANDLE_VALUE);

    // everything is OK - let's fill the context
    file->FileName = DupStr(fileName);
    if (file->FileName == NULL)
    {
        TRACE_E(LOW_MEMORY);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }
    file->HFile = hFile;
    file->HParentWnd = hParent;
    file->dwDesiredAccess = dwDesiredAccess;
    file->dwShareMode = dwShareMode;
    file->dwCreationDisposition = dwCreationDisposition;
    file->dwFlagsAndAttributes = dwFlagsAndAttributes;
    file->WholeFileAllocated = FALSE;
    return TRUE;
}

HANDLE
CSalamanderSafeFile::SafeFileCreate(const char* fileName,
                                    DWORD dwDesiredAccess,
                                    DWORD dwShareMode,
                                    DWORD dwFlagsAndAttributes,
                                    BOOL isDir,
                                    HWND hParent,
                                    const char* srcFileName,
                                    const char* srcFileInfo,
                                    DWORD* silentMask,
                                    BOOL allowSkip,
                                    BOOL* skipped,
                                    char* skipPath,
                                    int skipPathMax,
                                    CQuadWord* allocateWholeFile,
                                    SAFE_FILE* file)
{
    CALL_STACK_MESSAGE7("CSalamanderGeneral::SafeFileCreate(%s, %u, %u, %u, %d, , , , %d)",
                        fileName, dwDesiredAccess, dwShareMode, dwFlagsAndAttributes, isDir, allowSkip);
    dwFlagsAndAttributes &= 0xFFFF0000 | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                            FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY |
                            FILE_ATTRIBUTE_ARCHIVE;
    if (skipped != NULL)
        *skipped = FALSE;
    if (skipPath != NULL && skipPathMax > 0)
        *skipPath = 0;
    BOOL wholeFileAllocated = FALSE;
    BOOL needWholeAllocTest = FALSE; // We need to verify that the pointer is successfully set and the data is not appended to the end of the file
    if (allocateWholeFile != NULL &&
        *allocateWholeFile >= CQuadWord(0, 0x80000000))
    {
        *allocateWholeFile -= CQuadWord(0, 0x80000000);
        needWholeAllocTest = TRUE;
    }

    // check if it already exists
    DWORD attrs;
    HANDLE hFile;
    int fileNameLen = (int)strlen(fileName);
    while (1)
    {
        attrs = fileNameLen < MAX_PATH ? SalGetFileAttributes(fileName) : 0xFFFFFFFF;
        if (attrs == 0xFFFFFFFF)
            break;

        // already exists, let's see if it's just a collision with the DOS name (the full name of an existing file/directory is different)
        if (!isDir)
        {
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(fileName, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                const char* tgtName = SalPathFindFileName(fileName);
                if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // match only for two names
                    StrICmp(tgtName, data.cFileName) != 0)            // (full name is different)
                {
                    // rename ("clean up") the file/directory with a conflicting long name to a temporary 8.3 name (does not require an extra long name)
                    char tmpName[MAX_PATH + 20];
                    char origFullName[MAX_PATH];
                    lstrcpyn(tmpName, fileName, MAX_PATH);
                    CutDirectory(tmpName);
                    SalPathAddBackslash(tmpName, MAX_PATH + 20);
                    char* tmpNamePart = tmpName + strlen(tmpName);
                    if (SalPathAppend(tmpName, data.cFileName, MAX_PATH))
                    {
                        strcpy(origFullName, tmpName);
                        DWORD num = (GetTickCount() / 10) % 0xFFF;
                        while (1)
                        {
                            sprintf(tmpNamePart, "sal%03X", num++);
                            if (::SalMoveFile(origFullName, tmpName))
                                break;
                            DWORD e = GetLastError();
                            if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS)
                            {
                                tmpName[0] = 0;
                                break;
                            }
                        }
                        if (tmpName[0] != 0) // if the conflict file/directory has been successfully "cleaned up", we will try to create the target
                        {                    // file/directory, then we return the "cleaned up" file/directory its original name
                            hFile = INVALID_HANDLE_VALUE;
                            //              if (!isDir)   // file
                            //              {       // add handle to handles only if the SAFE_FILE structure is being fulfilled at the end
                            hFile = NOHANDLES(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL,
                                                         CREATE_NEW, dwFlagsAndAttributes, NULL));
                            //              }
                            //              else   // directory
                            //              {
                            //                if (CreateDirectory(fileName, NULL)) out = (void *)1;  // pri uspechu musime vratit neco jineho nez INVALID_HANDLE_VALUE
                            //              }
                            if (!::SalMoveFile(tmpName, origFullName))
                            { // this can obviously happen, incomprehensible, but Windows will create a 'fileName' (DOS name) file with the name origFullName
                                TRACE_I("Unexpected situation in CSalamanderGeneral::SafeCreateFile(): unable to rename file from tmp-name to original long file name! " << origFullName);

                                if (hFile != INVALID_HANDLE_VALUE)
                                {
                                    //                  if (!isDir)
                                    CloseHandle(hFile);
                                    hFile = INVALID_HANDLE_VALUE;
                                    //                  if (!isDir)
                                    DeleteFile(fileName);
                                    //                  else RemoveDirectory(fileName);
                                    if (!::SalMoveFile(tmpName, origFullName))
                                        TRACE_E("Fatal unexpected situation in CSalamanderGeneral::SafeCreateFile(): unable to rename file from tmp-name to original long file name! " << origFullName);
                                }
                            }
                            if (hFile != INVALID_HANDLE_VALUE)
                                goto SUCCESS; // Return only on success, errors are handled further (we ignore the conflict of the DOS name)
                        }
                    }
                }
            }
        }

        // It already exists, but what is it?
        if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        {
            int ret;
            // it is a directory
            if (isDir)
            {
                // if we wanted a directory, then it's okay
                // and return anything different from INVALID_HANDLE_VALUE
                return (void*)1;
            }
            // otherwise report an error
            if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_NAMEUSED) && allowSkip)
                ret = DIALOG_SKIP;
            else
            {
                // ERROR: filename+error, buttons retry/skip/skip all/cancel
                ret = DialogError(hParent, allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                                  fileName, LoadStr(IDS_NAMEALREADYUSEDFORDIR), LoadStr(IDS_ERRORCREATINGFILE));
            }
            switch (ret)
            {
            case DIALOG_SKIPALL:
                if (silentMask != NULL)
                    *silentMask |= SILENT_SKIP_FILE_NAMEUSED;
                // no break here
            case DIALOG_SKIP:
                if (skipped != NULL)
                    *skipped = TRUE;
                return INVALID_HANDLE_VALUE;
            case DIALOG_CANCEL:
            case DIALOG_FAIL:
                return INVALID_HANDLE_VALUE;
            }
        }
        else
        {
            int ret;
            // it is a file, let's find out if we can overwrite it
            if (isDir)
            {
                // We are trying to create a directory, but there is already a file with the same name at that location -- we will throw an error
                if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_NAMEUSED) && allowSkip)
                    ret = DIALOG_SKIP;
                else
                {
                    // ERROR: filename+error, buttons retry/skip/skip all/cancel
                    ret = DialogError(hParent, allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                                      fileName, LoadStr(IDS_NAMEALREADYUSED), LoadStr(IDS_ERRORCREATINGDIR));
                }
                switch (ret)
                {
                case DIALOG_SKIPALL:
                    if (silentMask != NULL)
                        *silentMask |= SILENT_SKIP_DIR_NAMEUSED;
                    // no break here
                case DIALOG_SKIP:
                    if (skipped != NULL)
                        *skipped = TRUE;
                    if (skipPath != NULL)
                        lstrcpyn(skipPath, fileName, skipPathMax); // user wants to return the skipped path
                    return INVALID_HANDLE_VALUE;
                case DIALOG_CANCEL:
                case DIALOG_FAIL:
                    return INVALID_HANDLE_VALUE;
                    // else retry
                }
            }
            else
            {
                // ask for a transcript
                if ((srcFileName != NULL && !Configuration.CnfrmFileOver) || (silentMask != NULL && (*silentMask & SILENT_OVERWRITE_FILE_EXIST)))
                    ret = DIALOG_YES;
                else if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_EXIST) && allowSkip)
                    ret = DIALOG_SKIP;
                else
                {
                    char fibuffer[500];
                    HANDLE file2 = HANDLES_Q(CreateFile(fileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
                    if (file2 != INVALID_HANDLE_VALUE)
                    {
                        GetFileOverwriteInfo(fibuffer, _countof(fibuffer), file2, fileName);
                        HANDLES(CloseHandle(file2));
                    }
                    else
                        strcpy(fibuffer, LoadStr(IDS_ERR_FILEOPEN));
                    if (srcFileName != NULL)
                    {
                        // CONFIRM FILE OVERWRITE: filename1+filedata1+filename2+filedata2, buttons yes/all/skip/skip all/cancel
                        ret = DialogOverwrite(hParent, allowSkip ? BUTTONS_YESALLSKIPCANCEL : BUTTONS_YESALLCANCEL,
                                              fileName, fibuffer, srcFileName, srcFileInfo);
                    }
                    else
                    {
                        // CONFIRM FILE OVERWRITE: filename1+filedata1+a newly created file, buttons yes/all/skip/skip all/cancel
                        ret = DialogQuestion(hParent, allowSkip ? BUTTONS_YESALLSKIPCANCEL : BUTTONS_YESNOCANCEL,
                                             fileName, LoadStr(IDS_NEWLYCREATEDFILE), LoadStr(IDS_CONFIRMFILEOVERWRITING));
                    }
                }
                switch (ret)
                {
                case DIALOG_SKIPALL:
                    if (silentMask != NULL)
                        *silentMask |= SILENT_SKIP_FILE_EXIST;
                    // no break here
                case DIALOG_SKIP:
                    if (skipped != NULL)
                        *skipped = TRUE;
                    return INVALID_HANDLE_VALUE;
                case DIALOG_CANCEL:
                case DIALOG_NO:
                case DIALOG_FAIL:
                    return INVALID_HANDLE_VALUE;
                case DIALOG_ALL:
                    ret = DIALOG_YES;
                    if (silentMask != NULL)
                        *silentMask |= SILENT_OVERWRITE_FILE_EXIST;
                    break;
                }
                if (ret == DIALOG_YES)
                {
                    // we will rewrite - remove attributes
                    if (attrs & FILE_ATTRIBUTE_HIDDEN ||
                        attrs & FILE_ATTRIBUTE_SYSTEM ||
                        attrs & FILE_ATTRIBUTE_READONLY)
                    {
                        // for files without hidden and system attributes, the second (hidden+system) confirmation is not displayed
                        if (srcFileName == NULL || !Configuration.CnfrmSHFileOver ||
                            (silentMask != NULL && (*silentMask & SILENT_OVERWRITE_FILE_SYSHID)) ||
                            ((attrs & FILE_ATTRIBUTE_HIDDEN) == 0 && (attrs & FILE_ATTRIBUTE_SYSTEM) == 0))
                            ret = DIALOG_YES;
                        else if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_SYSHID) && allowSkip)
                            ret = DIALOG_SKIP;
                        else
                            // QUESTION: filename+question, buttons yes/all/skip/skip all/cancel
                            ret = DialogQuestion(hParent, allowSkip ? BUTTONS_YESALLSKIPCANCEL : BUTTONS_YESALLCANCEL,
                                                 fileName, LoadStr(IDS_WANTOVERWRITESHFILE), LoadStr(IDS_CONFIRMFILEOVERWRITING));
                        switch (ret)
                        {
                        case DIALOG_SKIPALL:
                            if (silentMask != NULL)
                                *silentMask |= SILENT_SKIP_FILE_SYSHID;
                            // no break here
                        case DIALOG_SKIP:
                            if (skipped != NULL)
                                *skipped = TRUE;
                            return INVALID_HANDLE_VALUE;
                        case DIALOG_CANCEL:
                        case DIALOG_FAIL:
                            return INVALID_HANDLE_VALUE;
                        case DIALOG_ALL:
                            ret = DIALOG_YES;
                            if (silentMask != NULL)
                                *silentMask |= SILENT_OVERWRITE_FILE_SYSHID;
                            break;
                        }
                        if (ret == DIALOG_YES)
                        {
                            SetFileAttributes(fileName, FILE_ATTRIBUTE_NORMAL);
                            break;
                        }
                    }
                    else
                        break;
                }
            }
        }
    }

    if (attrs == 0xFFFFFFFF)
    {
        if (fileNameLen > MAX_PATH - 1)
        {
            // Too long name -- offer Skip / Skip All / Cancel
            int ret;
            if (silentMask != NULL && (*silentMask & (isDir ? SILENT_SKIP_DIR_CREATE : SILENT_SKIP_FILE_CREATE)) && allowSkip)
                ret = DIALOG_SKIP;
            else
            {
                // ERROR: filename+error, skip/skip all/cancel
                ret = DialogError(hParent, allowSkip ? BUTTONS_SKIPCANCEL : BUTTONS_OK, fileName, ::GetErrorText(ERROR_FILENAME_EXCED_RANGE),
                                  LoadStr(isDir ? IDS_ERRORCREATINGDIR : IDS_ERRORCREATINGFILE));
            }
            switch (ret)
            {
            case DIALOG_SKIPALL:
                if (silentMask != NULL)
                    *silentMask |= (isDir ? SILENT_SKIP_DIR_CREATE : SILENT_SKIP_FILE_CREATE);
                // no break here
            case DIALOG_SKIP:
            {
                if (skipped != NULL)
                    *skipped = TRUE;
                if (isDir && skipPath != NULL)
                    lstrcpyn(skipPath, fileName, skipPathMax); // user wants to return the skipped path
            }
            }
            return INVALID_HANDLE_VALUE;
        }

        char namecopy[MAX_PATH];
        strcpy(namecopy, fileName);
        // if it is a file, we get the directory name
        if (!isDir)
        {
            char* ptr = strrchr(namecopy, '\\');
            // Is there a way we could create?
            if (ptr == NULL)
                goto CREATE_FILE;
            // if yes, we will leave only the path
            *ptr = '\0';
            // Is there a way already?
            while (1)
            {
                attrs = SalGetFileAttributes(namecopy);
                if (attrs != 0xFFFFFFFF)
                {
                    // yes - let's create a file
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        goto CREATE_FILE;
                    // no - it's a file with the same name - we're throwing an error
                    int ret;
                    if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_NAMEUSED) && allowSkip)
                        ret = DIALOG_SKIP;
                    else
                    {
                        // ERROR: filename+error, buttons retry/skip/skip all/cancel
                        ret = DialogError(hParent, allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL, namecopy,
                                          LoadStr(IDS_NAMEALREADYUSED), LoadStr(IDS_ERRORCREATINGDIR));
                    }
                    switch (ret)
                    {
                    case DIALOG_SKIPALL:
                        if (silentMask != NULL)
                            *silentMask |= SILENT_SKIP_DIR_NAMEUSED;
                        // no break here
                    case DIALOG_SKIP:
                        if (skipped != NULL)
                            *skipped = TRUE;
                        if (skipPath != NULL)
                            lstrcpyn(skipPath, namecopy, skipPathMax); // user wants to return the skipped path
                        return INVALID_HANDLE_VALUE;
                    case DIALOG_CANCEL:
                    case DIALOG_FAIL:
                        return INVALID_HANDLE_VALUE;
                        // else retry
                    }
                }
                else
                    break;
            }
        }
        // create directory path
        char root[MAX_PATH];
        GetRootPath(root, namecopy);
        // if the directory is root, there is some problem
        if (strlen(namecopy) <= strlen(root))
        {
            // root directory -> error
            int ret;
            if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_CREATE) && allowSkip)
                ret = DIALOG_SKIP;
            else
                ret = DialogError(hParent, allowSkip ? BUTTONS_SKIPCANCEL : BUTTONS_OK, namecopy,
                                  LoadStr(IDS_ERRORCREATINGROOTDIR), LoadStr(IDS_ERRORCREATINGDIR));
            switch (ret)
            {
            case DIALOG_SKIPALL:
                if (silentMask != NULL)
                    *silentMask |= SILENT_SKIP_DIR_CREATE;
                // no break here
            case DIALOG_SKIP:
                if (skipped != NULL)
                    *skipped = TRUE;
                if (skipPath != NULL)
                    lstrcpyn(skipPath, namecopy, skipPathMax); // user wants to return the skipped path
            }
            return INVALID_HANDLE_VALUE;
        }
        char* ptr;
        char namecpy2[MAX_PATH];
        strcpy(namecpy2, namecopy);
        // find the first existing directory
        while (1)
        {
            ptr = strrchr(namecpy2, '\\');
            if (ptr == NULL)
            {
                // root directory -> error
                int ret;
                if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_CREATE) && allowSkip)
                    ret = DIALOG_SKIP;
                else
                    ret = DialogError(hParent, allowSkip ? BUTTONS_SKIPCANCEL : BUTTONS_OK, namecpy2,
                                      LoadStr(IDS_ERRORCREATINGROOTDIR), LoadStr(IDS_ERRORCREATINGDIR));
                switch (ret)
                {
                case DIALOG_SKIPALL:
                    if (silentMask != NULL)
                        *silentMask |= SILENT_SKIP_DIR_CREATE;
                    // no break here
                case DIALOG_SKIP:
                    if (skipped != NULL)
                        *skipped = TRUE;
                    if (skipPath != NULL)
                        lstrcpyn(skipPath, namecpy2, skipPathMax); // user wants to return the skipped path
                }
                return INVALID_HANDLE_VALUE;
            }
            *ptr = '\0';
            // Are we already in the root directory?
            if (ptr <= namecpy2 + strlen(root))
                break;
            while (1)
            {
                attrs = SalGetFileAttributes(namecpy2);
                if (attrs != 0xFFFFFFFF)
                {
                    // Do we have a directory or a file?
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        break;
                    else
                    {
                        int ret;
                        if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_NAMEUSED) && allowSkip)
                            ret = DIALOG_SKIP;
                        else
                        {
                            // ERROR: filename+error, buttons retry/skip/skip all/cancel
                            ret = DialogError(hParent, allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                                              namecpy2, LoadStr(IDS_NAMEALREADYUSED), LoadStr(IDS_ERRORCREATINGDIR));
                        }
                        switch (ret)
                        {
                        case DIALOG_SKIPALL:
                            if (silentMask != NULL)
                                *silentMask |= SILENT_SKIP_DIR_NAMEUSED;
                            // no break here
                        case DIALOG_SKIP:
                            if (skipped != NULL)
                                *skipped = TRUE;
                            if (skipPath != NULL)
                                lstrcpyn(skipPath, namecpy2, skipPathMax); // user wants to return the skipped path
                            return INVALID_HANDLE_VALUE;
                        case DIALOG_CANCEL:
                        case DIALOG_FAIL:
                            return INVALID_HANDLE_VALUE;
                            // else retry
                        }
                    }
                }
                else
                    break;
            }
            if (attrs != 0xFFFFFFFF && attrs & FILE_ATTRIBUTE_DIRECTORY)
                break;
        }
        // We have the first functional directory in namecopy
        ptr = namecpy2 + strlen(namecpy2) - 1;
        if (*ptr != '\\')
        {
            *++ptr = '\\';
            *++ptr = '\0';
        }
        // add another
        const char* src = namecopy + strlen(namecpy2);
        while (*src == '\\')
            src++;
        int len = (int)strlen(namecpy2);
        // and we will produce them one after another
        while (*src != 0)
        {
            BOOL invalidPath = FALSE; // *src != 0 && *src <= ' '; // space at the beginning of the directory name is allowed, but we do not allow it when manually creating a directory, it is confusing
            const char* slash = strchr(src, '\\');
            if (slash == NULL)
                slash = src + strlen(src);
            memcpy(namecpy2 + len, src, slash - src);
            namecpy2[len += (int)(slash - src)] = '\0';
            if (namecpy2[len - 1] <= ' ' || namecpy2[len - 1] == '.')
                invalidPath = TRUE; // Spaces and dots at the end of the directory name are undesirable
            while (invalidPath || !CreateDirectory(namecpy2, NULL))
            {
                // Failed to create directory, displaying error
                int ret;
                if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_CREATE) && allowSkip)
                    ret = DIALOG_SKIP;
                else
                {
                    DWORD err = GetLastError();
                    if (invalidPath)
                        err = ERROR_INVALID_NAME;
                    // ERROR: filename+error, buttons retry/skip/skip all/cancel
                    ret = DialogError(hParent, allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                                      namecpy2, ::GetErrorText(err), LoadStr(IDS_ERRORCREATINGDIR));
                }
                switch (ret)
                {
                case DIALOG_SKIPALL:
                    if (silentMask != NULL)
                        *silentMask |= SILENT_SKIP_DIR_CREATE;
                    // no break here
                case DIALOG_SKIP:
                    if (skipped != NULL)
                        *skipped = TRUE;
                    if (skipPath != NULL)
                        lstrcpyn(skipPath, namecpy2, skipPathMax); // user wants to return the skipped path
                    return INVALID_HANDLE_VALUE;

                case DIALOG_CANCEL:
                case DIALOG_FAIL:
                    return INVALID_HANDLE_VALUE;
                    // else retry
                }
            }
            namecpy2[len++] = '\\';
            while (*slash == '\\')
                slash++;
            src = slash;
        }
    }

CREATE_FILE:
    // if it is a file, we will create it
    if (!isDir)
    { // add the handle to handles only at the end if the SAFE_FILE structure is fulfilled
        while ((hFile = NOHANDLES(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL,
                                             CREATE_ALWAYS, dwFlagsAndAttributes, NULL))) == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            // handles the situation when it is necessary to rewrite a file on Samba:
            // file has 440+another_owner and is in a directory where the current user has write access
            // (deletion is possible, but direct overwrite is not (cannot be opened for writing) - we bypass:
            //  delete and create the file again
            // (In Samba, it is possible to enable deleting read-only, which allows deleting read-only files,
            //  otherwise it cannot be deleted, because Windows cannot delete a read-only file at the same time
            //  It is not possible to remove the "read-only" attribute from that file because the current user is not the owner.
            if (DeleteFile(fileName)) // if it is read-only, it can only be deleted on Samba with "delete readonly" enabled
            {                         // add the handle to handles only at the end if the SAFE_FILE structure is fulfilled
                hFile = NOHANDLES(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL,
                                             CREATE_ALWAYS, dwFlagsAndAttributes, NULL));
                if (hFile != INVALID_HANDLE_VALUE)
                    break;
                err = GetLastError();
            }

            int ret;
            if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_CREATE) && allowSkip)
                ret = DIALOG_SKIP;
            else
            {
                // ERROR: filename+error, buttons retry/skip/skip all/cancel
                ret = DialogError(hParent, allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL, fileName,
                                  ::GetErrorText(err), LoadStr(IDS_ERRORCREATINGFILE));
            }
            switch (ret)
            {
            case DIALOG_SKIPALL:
                if (silentMask != NULL)
                    *silentMask |= SILENT_SKIP_FILE_CREATE;
                // no break here
            case DIALOG_SKIP:
                if (skipped != NULL)
                    *skipped = TRUE;
                return INVALID_HANDLE_VALUE;
            case DIALOG_CANCEL:
            case DIALOG_FAIL:
                return INVALID_HANDLE_VALUE;
                // else retry
            }
        }

    SUCCESS:
        // *************** Anti-fragmentation code starts here

        // if possible, we will allocate the necessary space for the file (to prevent disk fragmentation + smoother writing to floppy disks)
        if (allocateWholeFile != NULL)
        {
            BOOL fatal = TRUE;
            BOOL ignoreErr = FALSE;
            if (*allocateWholeFile < CQuadWord(2, 0))
                TRACE_E("SafeFileCreate: (WARNING) allocateWholeFile less than 2");

        SET_SIZE_AGAIN:
            CQuadWord off = *allocateWholeFile;
            off.LoDWord = SetFilePointer(hFile, off.LoDWord, (LONG*)&(off.HiDWord), FILE_BEGIN);
            if ((off.LoDWord != INVALID_SET_FILE_POINTER || GetLastError() == NO_ERROR) && off == *allocateWholeFile)
            {
                if (SetEndOfFile(hFile))
                {
                    if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == 0)
                    {
                        if (needWholeAllocTest)
                        {
                            DWORD wr;
                            if (WriteFile(hFile, "x", 1, &wr, NULL) && wr == 1)
                            {
                                if (SetEndOfFile(hFile)) // let's try to shorten the file to one byte
                                {
                                    CQuadWord size;
                                    size.LoDWord = GetFileSize(hFile, &size.HiDWord);
                                    if (size == CQuadWord(1, 0))
                                    { // check if the written byte was not added to the end of the file + if we can truncate the file
                                        needWholeAllocTest = FALSE;
                                        goto SET_SIZE_AGAIN; // We need to reset the complete file size again
                                    }
                                }
                            }
                        }
                        else
                        {
                            fatal = FALSE;
                            wholeFileAllocated = TRUE; // everything is OK, the file is loaded
                        }
                    }
                }
                else
                {
                    if (GetLastError() == ERROR_DISK_FULL)
                        ignoreErr = TRUE; // low disk space
                }
            }
            if (fatal)
            {
                if (!ignoreErr)
                {
                    DWORD err = GetLastError();
                    TRACE_E("SafeFileCreate(): unable to allocate whole file size before copy operation, please report under what conditions this occurs! GetLastError(): " << GetErrorText(err));
                    *allocateWholeFile = CQuadWord(-1, 0); // We will refrain from further attempts on this target disk
                }
                else
                    *allocateWholeFile = CQuadWord(0, 0); // Failed to set the file, but we will try again next time

                // Let's try to truncate the file to zero to avoid any unnecessary writes when closing the file
                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                SetEndOfFile(hFile);

                CloseHandle(hFile);
                ClearReadOnlyAttr(fileName); // If it was created as read-only, we would manage it.
                DeleteFile(fileName);

                allocateWholeFile = NULL; // we will not attempt to inflate in the next round
                goto CREATE_FILE;
            }
        }
        // *************** Anti-fragmentation code ends here
    }
    // return result - if we have reached this point, we return success
    if (isDir)
        return (void*)1; // for the directory simply something else than INVALID_HANDLE_VALUE
    if (file != NULL)    // We are tasked with initializing the SAFE_FILE structure
    {
        file->FileName = DupStr(fileName);
        if (file->FileName == NULL)
        {
            TRACE_E(LOW_MEMORY);
            CloseHandle(hFile);
            return FALSE;
        }
        file->HFile = hFile;
        file->HParentWnd = hParent;
        file->dwDesiredAccess = dwDesiredAccess;
        file->dwShareMode = dwShareMode;
        file->dwCreationDisposition = CREATE_ALWAYS;
        file->dwFlagsAndAttributes = dwFlagsAndAttributes;
        file->WholeFileAllocated = wholeFileAllocated;
        HANDLES_ADD(__htFile, __hoCreateFile, hFile); // add handle hFile to HANDLES
    }
    return hFile;
}

void CSalamanderSafeFile::SafeFileClose(SAFE_FILE* file)
{
    if (file->HFile != NULL && file->HFile != INVALID_HANDLE_VALUE)
    {
        if (file->WholeFileAllocated)
            SetEndOfFile(file->HFile); // otherwise the rest of the file would be written
        HANDLES(CloseHandle(file->HFile));
    }
    if (file->FileName != NULL)
        free(file->FileName);
    ZeroMemory(file, sizeof(SAFE_FILE));
}

BOOL CSalamanderSafeFile::SafeFileSeek(SAFE_FILE* file, CQuadWord* distance, DWORD moveMethod, DWORD* error)
{
    if (error != NULL)
        *error = NO_ERROR;
    if (file->HFile == NULL)
    {
        TRACE_E("CSalamanderSafeFile::SafeFileSeek() HFile==NULL");
        return FALSE;
    }

    LARGE_INTEGER li;
    li.QuadPart = distance->Value;

    LONG lo = li.LowPart;
    LONG hi = li.HighPart;

    lo = SetFilePointer(file->HFile, lo, &hi, moveMethod);

    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
    {
        if (error != NULL)
            *error = GetLastError();
        return FALSE;
    }

    li.LowPart = lo;
    li.HighPart = hi;
    distance->Value = li.QuadPart;
    return TRUE;
}

BOOL CSalamanderSafeFile::SafeFileSeekMsg(SAFE_FILE* file, CQuadWord* distance, DWORD moveMethod,
                                          HWND hParent, DWORD flags, DWORD* pressedButton,
                                          DWORD* silentMask, BOOL seekForRead)
{
    if (file->HFile == NULL)
    {
        TRACE_E("CSalamanderSafeFile::SafeFileSeekMsg() HFile==NULL");
        return FALSE;
    }
SEEK_AGAIN:
    DWORD lastError;
    BOOL ret = SafeFileSeek(file, distance, moveMethod, &lastError);
    if (!ret)
    {
        DWORD dlgRet;
        DWORD skip = seekForRead ? SILENT_SKIP_FILE_READ : SILENT_SKIP_FILE_WRITE;
        if (silentMask != NULL && (*silentMask & skip) && ButtonsContainsSkip(flags)) // if we do not have a character to ignore, we display it
            dlgRet = DIALOG_SKIP;
        else
        {
            dlgRet = DialogError((hParent == HWND_STORED) ? file->HParentWnd : hParent, (flags & BUTTONS_MASK),
                                 file->FileName, GetErrorText(lastError),
                                 LoadStr(seekForRead ? IDS_ERRORREADINGFILE : IDS_ERRORWRITINGFILE));
        }
        switch (dlgRet)
        {
        case DIALOG_RETRY:
            goto SEEK_AGAIN; // Let's try again
        case DIALOG_SKIPALL:
            if (silentMask != NULL)
                *silentMask |= skip;
        default:
        {
            if (pressedButton != NULL)
                *pressedButton = dlgRet; // return the button that the user clicked
            return FALSE;
        }
        }
    }
    return ret;
}

BOOL CSalamanderSafeFile::SafeFileGetSize(SAFE_FILE* file, CQuadWord* fileSize, DWORD* error)
{
    if (error != NULL)
        *error = NO_ERROR;
    DWORD err;
    BOOL ret = SalGetFileSize(file->HFile, *fileSize, err);
    if (!ret && error != NULL)
        *error = err;
    return ret;
}

BOOL CSalamanderSafeFile::SafeFileRead(SAFE_FILE* file, LPVOID lpBuffer,
                                       DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
                                       HWND hParent, DWORD flags, DWORD* pressedButton,
                                       DWORD* silentMask)
{
    if (file->HFile == NULL)
    {
        TRACE_E("CSalamanderSafeFile::SafeFileRead() HFile==NULL");
        return FALSE;
    }
    // get the current seek in the file
    long currentSeekHi = 0;
    DWORD currentSeekLo = SetFilePointer(file->HFile, 0, &currentSeekHi, FILE_CURRENT);
    if (currentSeekLo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
        goto READ_ERROR; // cannot set offset, let's try again

    while (TRUE)
    {
        if (ReadFile(file->HFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL))
        {
            if ((flags & SAFE_FILE_CHECK_SIZE) && nNumberOfBytesToRead != *lpNumberOfBytesRead)
            {
                // The caller requests to read exactly as many bytes as they have asked for
                DWORD dlgRet;
                if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_READ) && ButtonsContainsSkip(flags))
                    dlgRet = DIALOG_SKIP;
                else
                {
                    dlgRet = DialogError((hParent == HWND_STORED) ? file->HParentWnd : hParent, (flags & BUTTONS_MASK),
                                         file->FileName, GetErrorText(ERROR_HANDLE_EOF), LoadStr(IDS_ERRORREADINGFILE));
                }
                switch (dlgRet)
                {
                case DIALOG_RETRY:
                    goto SEEK;
                case DIALOG_SKIPALL:
                    if (silentMask != NULL)
                        *silentMask |= SILENT_SKIP_FILE_READ;
                default:
                {
                    if (pressedButton != NULL)
                        *pressedButton = dlgRet; // return the button that the user clicked
                    return FALSE;
                }
                }
            }
            return TRUE;
        }
        else
        {
        READ_ERROR:
            DWORD lastError;
            DWORD dlgRet;
            if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_READ) && ButtonsContainsSkip(flags))
                dlgRet |= DIALOG_SKIP;
            else
            {
                lastError = GetLastError();
                dlgRet = DialogError((hParent == HWND_STORED) ? file->HParentWnd : hParent, (flags & BUTTONS_MASK),
                                     file->FileName, GetErrorText(lastError), LoadStr(IDS_ERRORREADINGFILE));
            }
            switch (dlgRet)
            {
            case DIALOG_RETRY:
            {
                if (file->HFile != NULL)
                {
                    if (file->WholeFileAllocated)
                        SetEndOfFile(file->HFile);     // otherwise the rest of the file would be written
                    HANDLES(CloseHandle(file->HFile)); // Close the invalid handle, because it cannot be read from anymore.
                }

                file->HFile = HANDLES_Q(CreateFile(file->FileName, file->dwDesiredAccess, file->dwShareMode, NULL,
                                                   file->dwCreationDisposition, file->dwFlagsAndAttributes, NULL));
                if (file->HFile != INVALID_HANDLE_VALUE) // open, let's set the offset
                {
                SEEK:
                    LONG lo = currentSeekLo;
                    LONG hi = currentSeekHi;
                    lo = SetFilePointer(file->HFile, lo, &hi, FILE_BEGIN);
                    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
                        goto READ_ERROR; // cannot set offset, let's try again
                    if (lo != (long)currentSeekLo || hi != currentSeekHi)
                    {
                        SetLastError(ERROR_SEEK_ON_DEVICE);
                        goto READ_ERROR; // Cannot set offset (file may already be smaller), let's try again
                    }
                }
                else // cannot open, problem persists ...
                {
                    file->HFile = NULL;
                    goto READ_ERROR;
                }
                break;
            }

            case DIALOG_SKIPALL:
                if (silentMask != NULL)
                    *silentMask |= SILENT_SKIP_FILE_READ;
            default:
            {
                if (pressedButton != NULL)
                    *pressedButton = dlgRet; // return the button that the user clicked
                return FALSE;
            }
            }
        }
    }
}

BOOL CSalamanderSafeFile::SafeFileWrite(SAFE_FILE* file, LPVOID lpBuffer,
                                        DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
                                        HWND hParent, DWORD flags, DWORD* pressedButton,
                                        DWORD* silentMask)
{
    if (file->HFile == NULL)
    {
        TRACE_E("CSalamanderSafeFile::SafeFileWrite() HFile==NULL");
        return FALSE;
    }
    // get the current seek in the file
    long currentSeekHi = 0;
    DWORD currentSeekLo = SetFilePointer(file->HFile, 0, &currentSeekHi, FILE_CURRENT);
    if (currentSeekLo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
        goto WRITE_ERROR; // cannot set offset, let's try again

    while (TRUE)
    {
        if (WriteFile(file->HFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, NULL) &&
            nNumberOfBytesToWrite == *lpNumberOfBytesWritten)
        {
            return TRUE;
        }
        else
        {
        WRITE_ERROR:
            DWORD lastError = GetLastError();
            DWORD dlgRet;
            if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_WRITE) && ButtonsContainsSkip(flags))
                dlgRet |= DIALOG_SKIP;
            else
            {
                dlgRet = DialogError((hParent == HWND_STORED) ? file->HParentWnd : hParent, (flags & BUTTONS_MASK),
                                     file->FileName, GetErrorText(lastError), LoadStr(IDS_ERRORWRITINGFILE));
            }
            switch (dlgRet)
            {
            case DIALOG_RETRY:
            {
                if (file->HFile != NULL)
                {
                    if (file->WholeFileAllocated)
                        SetEndOfFile(file->HFile);     // otherwise the rest of the file would be written
                    HANDLES(CloseHandle(file->HFile)); // Close the invalid handle, because it cannot be read from anymore.
                }

                file->HFile = HANDLES_Q(CreateFile(file->FileName, file->dwDesiredAccess, file->dwShareMode, NULL,
                                                   file->dwCreationDisposition, file->dwFlagsAndAttributes, NULL));
                if (file->HFile != INVALID_HANDLE_VALUE) // open, let's set the offset
                {
                    //SEEK:
                    LONG lo = currentSeekLo;
                    LONG hi = currentSeekHi;
                    lo = SetFilePointer(file->HFile, lo, &hi, FILE_BEGIN);
                    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
                        goto WRITE_ERROR; // cannot set offset, let's try again
                    if (lo != (long)currentSeekLo || hi != currentSeekHi)
                    {
                        SetLastError(ERROR_SEEK_ON_DEVICE);
                        goto WRITE_ERROR; // Cannot set offset (file may already be smaller), let's try again
                    }
                }
                else // cannot open, problem persists ...
                {
                    file->HFile = NULL;
                    goto WRITE_ERROR;
                }
                break;
            }

            case DIALOG_SKIPALL:
                if (silentMask != NULL)
                    *silentMask |= SILENT_SKIP_FILE_WRITE;
            default:
            {
                if (pressedButton != NULL)
                    *pressedButton = dlgRet; // return the button that the user clicked
                return FALSE;
            }
            }
        }
    }
}
