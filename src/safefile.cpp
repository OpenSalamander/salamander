// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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

    // for errors such as LOW_MEMORY we want the operation to abort entirely
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

    // everything is OK - populate the context structure
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
    BOOL needWholeAllocTest = FALSE; // we must verify that the pointer can be set and the data are not appended to the end of the file
    if (allocateWholeFile != NULL &&
        *allocateWholeFile >= CQuadWord(0, 0x80000000))
    {
        *allocateWholeFile -= CQuadWord(0, 0x80000000);
        needWholeAllocTest = TRUE;
    }

    // check whether the target already exists
    DWORD attrs;
    HANDLE hFile;
    int fileNameLen = (int)strlen(fileName);
    while (1)
    {
        attrs = fileNameLen < MAX_PATH ? SalGetFileAttributes(fileName) : 0xFFFFFFFF;
        if (attrs == 0xFFFFFFFF)
            break;

        // it already exists; we’ll check whether it’s just a collision with a DOS-style name (the full name of the existing file/directory is different)
        if (!isDir)
        {
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(fileName, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                const char* tgtName = SalPathFindFileName(fileName);
                if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // match only for the DOS name
                    StrICmp(tgtName, data.cFileName) != 0)            // (the full name is different)
                {
                    // rename ("clean up") the file/directory with the conflicting DOS name to a temporary 8.3 name (which doesn’t require an extra DOS name)
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
                        if (tmpName[0] != 0) // if we managed to "clean up" the conflicting file/directory, try creating the target
                        {                    // file/directory and then restore the original name to the "cleaned" file/directory
                            hFile = INVALID_HANDLE_VALUE;
                            //              if (!isDir)   // file
                            //              {       // add the handle to HANDLES at the end only if the SAFE_FILE structure is being filled
                            hFile = NOHANDLES(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL,
                                                         CREATE_NEW, dwFlagsAndAttributes, NULL));
                            //              }
                            //              else   // directory
                            //              {
                            //                if (CreateDirectory(fileName, NULL)) out = (void *)1;  // on success we must return something other than INVALID_HANDLE_VALUE
                            //              }
                            if (!::SalMoveFile(tmpName, origFullName))
                            { // this can apparently happen; inexplicably, Windows creates a file named origFullName instead of 'fileName' (the DOS name)
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
                                goto SUCCESS; // return only on success; errors are handled later (ignore the DOS-name conflict)
                        }
                    }
                }
            }
        }

        // it already exists, but what is it?
        if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        {
            int ret;
            // it is a directory
            if (isDir)
            {
                // if we wanted a directory, that is fine
                // and return anything other than INVALID_HANDLE_VALUE
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
            // it is a file, check whether it can be overwritten
            if (isDir)
            {
                // we are trying to create a directory, but there is already a file with the same name in the place -- report an error
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
                        lstrcpyn(skipPath, fileName, skipPathMax); // the user wants to return the skipped path
                    return INVALID_HANDLE_VALUE;
                case DIALOG_CANCEL:
                case DIALOG_FAIL:
                    return INVALID_HANDLE_VALUE;
                    // else retry
                }
            }
            else
            {
                // ask whether to overwrite
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
                    // we will overwrite - clear the attributes
                    if (attrs & FILE_ATTRIBUTE_HIDDEN ||
                        attrs & FILE_ATTRIBUTE_SYSTEM ||
                        attrs & FILE_ATTRIBUTE_READONLY)
                    {
                        // for files without hidden and system attributes, the second (hidden+system) confirmation is not shown
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
            // Name too long -- offer Skip / Skip All / Cancel
            int ret;
            if (silentMask != NULL && (*silentMask & (isDir ? SILENT_SKIP_DIR_CREATE : SILENT_SKIP_FILE_CREATE)) && allowSkip)
                ret = DIALOG_SKIP;
            else
            {
                // ERROR: filename+error, buttons skip/skip all/cancel
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
                    lstrcpyn(skipPath, fileName, skipPathMax); // the user wants to retrieve the skipped path
            }
            }
            return INVALID_HANDLE_VALUE;
        }

        char namecopy[MAX_PATH];
        strcpy(namecopy, fileName);
        // if it is a file, obtain the directory name
        if (!isDir)
        {
            char* ptr = strrchr(namecopy, '\\');
            // does a path exist that we could create?
            if (ptr == NULL)
                goto CREATE_FILE;
            // if so, keep only the path
            *ptr = '\0';
            // does the path already exist?
            while (1)
            {
                attrs = SalGetFileAttributes(namecopy);
                if (attrs != 0xFFFFFFFF)
                {
                    // yes - proceed to create the file
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        goto CREATE_FILE;
                    // no - there is a file with the same name - report an error
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
                            lstrcpyn(skipPath, namecopy, skipPathMax); // the user wants to return the skipped path
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
        // create the directory path
        char root[MAX_PATH];
        GetRootPath(root, namecopy);
        // if the directory is the root directory, there is a problem
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
                    lstrcpyn(skipPath, namecopy, skipPathMax); // the user wants to return the skipped path
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
                        lstrcpyn(skipPath, namecpy2, skipPathMax); // the user wants to retrieve the skipped path
                }
                return INVALID_HANDLE_VALUE;
            }
            *ptr = '\0';
            // are we already at the root directory?
            if (ptr <= namecpy2 + strlen(root))
                break;
            while (1)
            {
                attrs = SalGetFileAttributes(namecpy2);
                if (attrs != 0xFFFFFFFF)
                {
                    // do we have a directory or a file?
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
                                lstrcpyn(skipPath, namecpy2, skipPathMax); // the user wants to return the skipped path
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
        // we have the first working directory in namecopy
        ptr = namecpy2 + strlen(namecpy2) - 1;
        if (*ptr != '\\')
        {
            *++ptr = '\\';
            *++ptr = '\0';
        }
        // add another one
        const char* src = namecopy + strlen(namecpy2);
        while (*src == '\\')
            src++;
        int len = (int)strlen(namecpy2);
        // and now create them one after another
        while (*src != 0)
        {
            BOOL invalidPath = FALSE; // *src != 0 && *src <= ' '; // a leading space in a directory name is allowed, but when creating directories manually, we do not allow it because it is confusing
            const char* slash = strchr(src, '\\');
            if (slash == NULL)
                slash = src + strlen(src);
            memcpy(namecpy2 + len, src, slash - src);
            namecpy2[len += (int)(slash - src)] = '\0';
            if (namecpy2[len - 1] <= ' ' || namecpy2[len - 1] == '.')
                invalidPath = TRUE; // spaces and dots at the end of the directory name being created are undesirable
            while (invalidPath || !CreateDirectory(namecpy2, NULL))
            {
                // failed to create the directory, display an error
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
                        lstrcpyn(skipPath, namecpy2, skipPathMax); // the user wants to return the skipped path
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
    // if it is a file, create it
    if (!isDir)
    { // add the handle to HANDLES at the end only if the SAFE_FILE structure is being filled
        while ((hFile = NOHANDLES(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL,
                                             CREATE_ALWAYS, dwFlagsAndAttributes, NULL))) == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            // handles the situation when a file needs to be overwritten on Samba:
            // the file has permissions 440+different_owner and is in a directory where the current user can write to
            // (it can be deleted, but not overwritten directly (cannot be opened for writing) - we work around it:
            //  delete and create the file again)
            // (on Samba it is possible to allow deleting read-only files, which allows deleting a read-only file,
            //  otherwise it cannot be deleted because Windows cannot delete a read-only file and at the same time
            //  the "read-only" attribute cannot be cleared on that file because the current user is not the owner)
            if (DeleteFile(fileName)) // if it is read-only, it can be deleted only on Samba with "delete readonly" allowed
            {                         // add the handle to HANDLES at the end only if the SAFE_FILE structure is being filled
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
        // *************** Anti-fragmentation code begins here

        // if possible, allocate the necessary space for the file (prevents disk fragmentation + smoother writes to floppies)
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
                                if (SetEndOfFile(hFile)) // try truncating the file to one byte
                                {
                                    CQuadWord size;
                                    size.LoDWord = GetFileSize(hFile, &size.HiDWord);
                                    if (size == CQuadWord(1, 0))
                                    { // check whether the written byte was appended to the end of the file and whether we can truncate the file
                                        needWholeAllocTest = FALSE;
                                        goto SET_SIZE_AGAIN; // we have to set the full file size again
                                    }
                                }
                            }
                        }
                        else
                        {
                            fatal = FALSE;
                            wholeFileAllocated = TRUE; // everything is OK, the file is stretched
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
                    *allocateWholeFile = CQuadWord(-1, 0); // skip further attempts on this target disk
                }
                else
                    *allocateWholeFile = CQuadWord(0, 0); // the file could not be prepared, but we will try again next time

                // also try truncating the file to zero to avoid unnecessary writing when closing the file
                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                SetEndOfFile(hFile);

                CloseHandle(hFile);
                ClearReadOnlyAttr(fileName); // in case it ended up read-only so we can handle it
                DeleteFile(fileName);

                allocateWholeFile = NULL; // next time we will no longer try to preallocate
                goto CREATE_FILE;
            }
        }
        // *************** Anti-fragmentation code ends here
    }
    // return the result - if we got this far, we return success
    if (isDir)
        return (void*)1; // for a directory, just return anything other than INVALID_HANDLE_VALUE
    if (file != NULL)    // our task is to initialize the SAFE_FILE structure
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
        if (silentMask != NULL && (*silentMask & skip) && ButtonsContainsSkip(flags)) // if we are not supposed to ignore the message, show it
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
            goto SEEK_AGAIN; // try again
        case DIALOG_SKIPALL:
            if (silentMask != NULL)
                *silentMask |= skip;
        default:
        {
            if (pressedButton != NULL)
                *pressedButton = dlgRet; // return the button the user clicked
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
    // obtain the current seek position in the file
    long currentSeekHi = 0;
    DWORD currentSeekLo = SetFilePointer(file->HFile, 0, &currentSeekHi, FILE_CURRENT);
    if (currentSeekLo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
        goto READ_ERROR; // cannot set the offset, try again

    while (TRUE)
    {
        if (ReadFile(file->HFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL))
        {
            if ((flags & SAFE_FILE_CHECK_SIZE) && nNumberOfBytesToRead != *lpNumberOfBytesRead)
            {
                // the caller requires reading exactly as many bytes as requested
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
                        *pressedButton = dlgRet; // return the button the user clicked
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
                    HANDLES(CloseHandle(file->HFile)); // close the invalid handle because we could not read from it anyway
                }

                file->HFile = HANDLES_Q(CreateFile(file->FileName, file->dwDesiredAccess, file->dwShareMode, NULL,
                                                   file->dwCreationDisposition, file->dwFlagsAndAttributes, NULL));
                if (file->HFile != INVALID_HANDLE_VALUE) // opened; now set the offset
                {
                SEEK:
                    LONG lo = currentSeekLo;
                    LONG hi = currentSeekHi;
                    lo = SetFilePointer(file->HFile, lo, &hi, FILE_BEGIN);
                    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
                        goto READ_ERROR; // cannot set the offset, try again
                    if (lo != (long)currentSeekLo || hi != currentSeekHi)
                    {
                        SetLastError(ERROR_SEEK_ON_DEVICE);
                        goto READ_ERROR; // cannot set the offset (the file may already be smaller), try again
                    }
                }
                else // cannot open it, the problem persists...
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
                    *pressedButton = dlgRet; // return the button the user clicked
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
    // obtain the current seek position in the file
    long currentSeekHi = 0;
    DWORD currentSeekLo = SetFilePointer(file->HFile, 0, &currentSeekHi, FILE_CURRENT);
    if (currentSeekLo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
        goto WRITE_ERROR; // cannot set the offset, try again

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
                    HANDLES(CloseHandle(file->HFile)); // close the invalid handle because we could not read from it anyway
                }

                file->HFile = HANDLES_Q(CreateFile(file->FileName, file->dwDesiredAccess, file->dwShareMode, NULL,
                                                   file->dwCreationDisposition, file->dwFlagsAndAttributes, NULL));
                if (file->HFile != INVALID_HANDLE_VALUE) // opened; now set the offset
                {
                    //SEEK:
                    LONG lo = currentSeekLo;
                    LONG hi = currentSeekHi;
                    lo = SetFilePointer(file->HFile, lo, &hi, FILE_BEGIN);
                    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
                        goto WRITE_ERROR; // cannot set the offset, try again
                    if (lo != (long)currentSeekLo || hi != currentSeekHi)
                    {
                        SetLastError(ERROR_SEEK_ON_DEVICE);
                        goto WRITE_ERROR; // cannot set the offset (the file may already be smaller), try again
                    }
                }
                else // cannot open it, the problem persists...
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
                    *pressedButton = dlgRet; // return the button the user clicked
                return FALSE;
            }
            }
        }
    }
}
