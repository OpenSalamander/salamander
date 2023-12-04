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

    // pro chyby jako LOW_MEMORY si prejeme uplny konec operace
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

    // vse je OK - naplnime kontext
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
    BOOL needWholeAllocTest = FALSE; // mame overit, ze se dari nastavit ukazovatko a data se nepripoji na konec souboru
    if (allocateWholeFile != NULL &&
        *allocateWholeFile >= CQuadWord(0, 0x80000000))
    {
        *allocateWholeFile -= CQuadWord(0, 0x80000000);
        needWholeAllocTest = TRUE;
    }

    // zjistime, zda jiz existuje
    DWORD attrs;
    HANDLE hFile;
    int fileNameLen = (int)strlen(fileName);
    while (1)
    {
        attrs = fileNameLen < MAX_PATH ? SalGetFileAttributes(fileName) : 0xFFFFFFFF;
        if (attrs == 0xFFFFFFFF)
            break;

        // uz existuje, zkusime jestli to neni jen kolize s dosovym nazvem (plne jmeno existujiciho souboru/adresare je jine)
        if (!isDir)
        {
            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(fileName, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                const char* tgtName = SalPathFindFileName(fileName);
                if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // shoda jen pro dos name
                    StrICmp(tgtName, data.cFileName) != 0)            // (plne jmeno je jine)
                {
                    // prejmenujeme ("uklidime") soubor/adresar s konfliktnim dos name do docasneho nazvu 8.3 (nepotrebuje extra dos name)
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
                        if (tmpName[0] != 0) // pokud se podarilo "uklidit" konfliktni soubor/adresar, zkusime vytvoreni ciloveho
                        {                    // souboru/adresare, pak vratime "uklizenemu" souboru/adresari jeho puvodni jmeno
                            hFile = INVALID_HANDLE_VALUE;
                            //              if (!isDir)   // soubor
                            //              {       // do handles pridame handle na zaver pouze pokud se plni struktura SAFE_FILE
                            hFile = NOHANDLES(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL,
                                                         CREATE_NEW, dwFlagsAndAttributes, NULL));
                            //              }
                            //              else   // adresar
                            //              {
                            //                if (CreateDirectory(fileName, NULL)) out = (void *)1;  // pri uspechu musime vratit neco jineho nez INVALID_HANDLE_VALUE
                            //              }
                            if (!::SalMoveFile(tmpName, origFullName))
                            { // toto se zjevne muze stat, nepochopitelne, ale Windows vytvori misto 'fileName' (dos name) soubor se jmenem origFullName
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
                                goto SUCCESS; // navrat jen pri uspechu, chyby se resi dale (ignorujeme konflikt dos-jmena)
                        }
                    }
                }
            }
        }

        // uz existuje, ale co to je ?
        if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        {
            int ret;
            // je to adresar
            if (isDir)
            {
                // pokud jsme chteli adresar, pak je to v poradku
                // a vratime cokoli ruzneho od INVALID_HANDLE_VALUE
                return (void*)1;
            }
            // jinak hlasime chybu
            if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_NAMEUSED) && allowSkip)
                ret = DIALOG_SKIP;
            else
            {
                // ERROR: filename+error, tlacitka retry/skip/skip all/cancel
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
            // je to soubor, zjistime, zda ho muzeme prepsat
            if (isDir)
            {
                // snazime se vytvorit adresar, ale v miste uz je soubor se stejnym nazvem -- vyhlasime chybu
                if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_NAMEUSED) && allowSkip)
                    ret = DIALOG_SKIP;
                else
                {
                    // ERROR: filename+error, tlacitka retry/skip/skip all/cancel
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
                        lstrcpyn(skipPath, fileName, skipPathMax); // uzivatel chce vratit skipnutou cestu
                    return INVALID_HANDLE_VALUE;
                case DIALOG_CANCEL:
                case DIALOG_FAIL:
                    return INVALID_HANDLE_VALUE;
                    // else retry
                }
            }
            else
            {
                // doptame se na prepis
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
                        // CONFIRM FILE OVERWRITE: filename1+filedata1+filename2+filedata2, tlacitka yes/all/skip/skip all/cancel
                        ret = DialogOverwrite(hParent, allowSkip ? BUTTONS_YESALLSKIPCANCEL : BUTTONS_YESALLCANCEL,
                                              fileName, fibuffer, srcFileName, srcFileInfo);
                    }
                    else
                    {
                        // CONFIRM FILE OVERWRITE: filename1+filedata1+a newly created file, tlacitka yes/all/skip/skip all/cancel
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
                    // budeme prepisovat - zrusime atributy
                    if (attrs & FILE_ATTRIBUTE_HIDDEN ||
                        attrs & FILE_ATTRIBUTE_SYSTEM ||
                        attrs & FILE_ATTRIBUTE_READONLY)
                    {
                        // pro soubory bez hidden a system attr. se druha (hidden+system) confirmation nevypisuje
                        if (srcFileName == NULL || !Configuration.CnfrmSHFileOver ||
                            (silentMask != NULL && (*silentMask & SILENT_OVERWRITE_FILE_SYSHID)) ||
                            ((attrs & FILE_ATTRIBUTE_HIDDEN) == 0 && (attrs & FILE_ATTRIBUTE_SYSTEM) == 0))
                            ret = DIALOG_YES;
                        else if (silentMask != NULL && (*silentMask & SILENT_SKIP_FILE_SYSHID) && allowSkip)
                            ret = DIALOG_SKIP;
                        else
                            // QUESTION: filename+question, tlacitka yes/all/skip/skip all/cancel
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
            // Prilis dlouhe jmeno -- nabidneme Skip / Skip All / Cancel
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
                    lstrcpyn(skipPath, fileName, skipPathMax); // uzivatel chce vratit skipnutou cestu
            }
            }
            return INVALID_HANDLE_VALUE;
        }

        char namecopy[MAX_PATH];
        strcpy(namecopy, fileName);
        // pokud je to soubor, ziskame jmeno adresare
        if (!isDir)
        {
            char* ptr = strrchr(namecopy, '\\');
            // existuje cesta, kterou bychom mohli tvorit ?
            if (ptr == NULL)
                goto CREATE_FILE;
            // pokud ano, nechame jen cestu
            *ptr = '\0';
            // existuje uz cesta ?
            while (1)
            {
                attrs = SalGetFileAttributes(namecopy);
                if (attrs != 0xFFFFFFFF)
                {
                    // ano - jdeme delat soubor
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        goto CREATE_FILE;
                    // ne - je to soubor se stejnym nazvem - hazime chybu
                    int ret;
                    if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_NAMEUSED) && allowSkip)
                        ret = DIALOG_SKIP;
                    else
                    {
                        // ERROR: filename+error, tlacitka retry/skip/skip all/cancel
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
                            lstrcpyn(skipPath, namecopy, skipPathMax); // uzivatel chce vratit skipnutou cestu
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
        // vytvorime adresarovou cestu
        char root[MAX_PATH];
        GetRootPath(root, namecopy);
        // pokud je dir root adresar, je tu nejaky problem
        if (strlen(namecopy) <= strlen(root))
        {
            // root adresar -> chyba
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
                    lstrcpyn(skipPath, namecopy, skipPathMax); // uzivatel chce vratit skipnutou cestu
            }
            return INVALID_HANDLE_VALUE;
        }
        char* ptr;
        char namecpy2[MAX_PATH];
        strcpy(namecpy2, namecopy);
        // najdeme prvni existujici adresar
        while (1)
        {
            ptr = strrchr(namecpy2, '\\');
            if (ptr == NULL)
            {
                // root adresar -> chyba
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
                        lstrcpyn(skipPath, namecpy2, skipPathMax); // uzivatel chce vratit skipnutou cestu
                }
                return INVALID_HANDLE_VALUE;
            }
            *ptr = '\0';
            // jsme uz na root-adresari ?
            if (ptr <= namecpy2 + strlen(root))
                break;
            while (1)
            {
                attrs = SalGetFileAttributes(namecpy2);
                if (attrs != 0xFFFFFFFF)
                {
                    // mame adresar nebo soubor ?
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        break;
                    else
                    {
                        int ret;
                        if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_NAMEUSED) && allowSkip)
                            ret = DIALOG_SKIP;
                        else
                        {
                            // ERROR: filename+error, tlacitka retry/skip/skip all/cancel
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
                                lstrcpyn(skipPath, namecpy2, skipPathMax); // uzivatel chce vratit skipnutou cestu
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
        // mame prvni funkcni adresar v namecopy
        ptr = namecpy2 + strlen(namecpy2) - 1;
        if (*ptr != '\\')
        {
            *++ptr = '\\';
            *++ptr = '\0';
        }
        // pridame dalsi
        const char* src = namecopy + strlen(namecpy2);
        while (*src == '\\')
            src++;
        int len = (int)strlen(namecpy2);
        // a uz je vyrabime jeden za druhym
        while (*src != 0)
        {
            BOOL invalidPath = FALSE; // *src != 0 && *src <= ' '; // mezera na zacatku jmena adresare je povolena, pri rucni vyrobe adresare ji ale nedovolujeme, je to matouci
            const char* slash = strchr(src, '\\');
            if (slash == NULL)
                slash = src + strlen(src);
            memcpy(namecpy2 + len, src, slash - src);
            namecpy2[len += (int)(slash - src)] = '\0';
            if (namecpy2[len - 1] <= ' ' || namecpy2[len - 1] == '.')
                invalidPath = TRUE; // mezery a tecky na konci jmena vytvareneho adresare jsou nezadouci
            while (invalidPath || !CreateDirectory(namecpy2, NULL))
            {
                // nepodarilo se vytvorit adresar, zobrazime chybu
                int ret;
                if (silentMask != NULL && (*silentMask & SILENT_SKIP_DIR_CREATE) && allowSkip)
                    ret = DIALOG_SKIP;
                else
                {
                    DWORD err = GetLastError();
                    if (invalidPath)
                        err = ERROR_INVALID_NAME;
                    // ERROR: filename+error, tlacitka retry/skip/skip all/cancel
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
                        lstrcpyn(skipPath, namecpy2, skipPathMax); // uzivatel chce vratit skipnutou cestu
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
    // je-li to soubor, vytvorime ho
    if (!isDir)
    { // do handles pridame handle na zaver pouze pokud se plni struktura SAFE_FILE
        while ((hFile = NOHANDLES(CreateFile(fileName, dwDesiredAccess, dwShareMode, NULL,
                                             CREATE_ALWAYS, dwFlagsAndAttributes, NULL))) == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            // resi situaci, kdy je potreba prepsat soubor na Sambe:
            // soubor ma 440+jinyho_vlastnika a je v adresari, kam ma akt. user zapis
            // (smazat lze, ale primo prepsat ne (nelze otevrit pro zapis) - obchazime:
            //  smazeme+vytvorime soubor znovu)
            // (na Sambe lze povolit mazani read-only, coz umozni delete read-only souboru,
            //  jinak nelze smazat, protoze Windows neumi smazat read-only soubor a zaroven
            //  u toho souboru nelze shodit "read-only" atribut, protoze akt. user neni vlastnik)
            if (DeleteFile(fileName)) // je-li read-only, pujde smazat jedine na Sambe s povolenym "delete readonly"
            {                         // do handles pridame handle na zaver pouze pokud se plni struktura SAFE_FILE
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
                // ERROR: filename+error, tlacitka retry/skip/skip all/cancel
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
        // *************** Zde zacina anti-fragmentovaci kod

        // pokud je to mozne, provedeme alokaci potrebneho mista pro soubor (nedochazi pak k fragmentaci disku + hladsi zapis na diskety)
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
                                if (SetEndOfFile(hFile)) // zkusime zkraceni souboru na jeden byte
                                {
                                    CQuadWord size;
                                    size.LoDWord = GetFileSize(hFile, &size.HiDWord);
                                    if (size == CQuadWord(1, 0))
                                    { // kontrola, jestli nedoslo k pridani zapisovaneho bytu na konec souboru + jestli umime soubor zkratit
                                        needWholeAllocTest = FALSE;
                                        goto SET_SIZE_AGAIN; // musime znovu nastavit kompletni velikost souboru
                                    }
                                }
                            }
                        }
                        else
                        {
                            fatal = FALSE;
                            wholeFileAllocated = TRUE; // vse je OK, soubor je natazeny
                        }
                    }
                }
                else
                {
                    if (GetLastError() == ERROR_DISK_FULL)
                        ignoreErr = TRUE; // malo mista na disku
                }
            }
            if (fatal)
            {
                if (!ignoreErr)
                {
                    DWORD err = GetLastError();
                    TRACE_E("SafeFileCreate(): unable to allocate whole file size before copy operation, please report under what conditions this occurs! GetLastError(): " << GetErrorText(err));
                    *allocateWholeFile = CQuadWord(-1, 0); // dalsi pokusy na tomhle cilovem disku si odpustime
                }
                else
                    *allocateWholeFile = CQuadWord(0, 0); // soubor se nezdarilo nastavit, ale priste to zkusime znovu

                // zkusime jeste soubor zkratit na nulu, aby nedoslo pri zavreni souboru k nejakemu zbytecnemu zapisu
                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                SetEndOfFile(hFile);

                CloseHandle(hFile);
                ClearReadOnlyAttr(fileName); // kdyby vznikl jako read-only, tak abychom si s nim poradili
                DeleteFile(fileName);

                allocateWholeFile = NULL; // v pristi kole uz se o nafukovani nebudeme pokouset
                goto CREATE_FILE;
            }
        }
        // *************** Zde konci anti-fragmentovaci kod
    }
    // vratime result - pokud jsme dosli az sem, vracime uspech
    if (isDir)
        return (void*)1; // pro adresar proste neco jineho, nez INVALID_HANDLE_VALUE
    if (file != NULL)    // mame za ukol inicializovat strukturu SAFE_FILE
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
        HANDLES_ADD(__htFile, __hoCreateFile, hFile); // handle hFile pridame do HANDLES
    }
    return hFile;
}

void CSalamanderSafeFile::SafeFileClose(SAFE_FILE* file)
{
    if (file->HFile != NULL && file->HFile != INVALID_HANDLE_VALUE)
    {
        if (file->WholeFileAllocated)
            SetEndOfFile(file->HFile); // jinak by se zapisoval zbytek souboru
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
        if (silentMask != NULL && (*silentMask & skip) && ButtonsContainsSkip(flags)) // pokud nemame hlasku ignorovat, zobrazime ji
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
            goto SEEK_AGAIN; // zkusime znovu
        case DIALOG_SKIPALL:
            if (silentMask != NULL)
                *silentMask |= skip;
        default:
        {
            if (pressedButton != NULL)
                *pressedButton = dlgRet; // vratim tlacitko, na ktere user kliknul
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
    // ziskame aktualni seek v souboru
    long currentSeekHi = 0;
    DWORD currentSeekLo = SetFilePointer(file->HFile, 0, &currentSeekHi, FILE_CURRENT);
    if (currentSeekLo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
        goto READ_ERROR; // nelze nastavit offset, zkusime to znovu

    while (TRUE)
    {
        if (ReadFile(file->HFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL))
        {
            if ((flags & SAFE_FILE_CHECK_SIZE) && nNumberOfBytesToRead != *lpNumberOfBytesRead)
            {
                // volajici vyzaduje nacteni presne tolika bajtu, o kolik si pozadal
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
                        *pressedButton = dlgRet; // vratim tlacitko, na ktere user kliknul
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
                        SetEndOfFile(file->HFile);     // jinak by se zapisoval zbytek souboru
                    HANDLES(CloseHandle(file->HFile)); // zavreme invalidni handle, protoze uz by z nej stejne neslo cist
                }

                file->HFile = HANDLES_Q(CreateFile(file->FileName, file->dwDesiredAccess, file->dwShareMode, NULL,
                                                   file->dwCreationDisposition, file->dwFlagsAndAttributes, NULL));
                if (file->HFile != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
                {
                SEEK:
                    LONG lo = currentSeekLo;
                    LONG hi = currentSeekHi;
                    lo = SetFilePointer(file->HFile, lo, &hi, FILE_BEGIN);
                    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
                        goto READ_ERROR; // nelze nastavit offset, zkusime to znovu
                    if (lo != (long)currentSeekLo || hi != currentSeekHi)
                    {
                        SetLastError(ERROR_SEEK_ON_DEVICE);
                        goto READ_ERROR; // nelze nastavit offset (soubor uz muze byt mensi), zkusime to znovu
                    }
                }
                else // nejde otevrit, problem trva ...
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
                    *pressedButton = dlgRet; // vratim tlacitko, na ktere user kliknul
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
    // ziskame aktualni seek v souboru
    long currentSeekHi = 0;
    DWORD currentSeekLo = SetFilePointer(file->HFile, 0, &currentSeekHi, FILE_CURRENT);
    if (currentSeekLo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
        goto WRITE_ERROR; // nelze nastavit offset, zkusime to znovu

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
                        SetEndOfFile(file->HFile);     // jinak by se zapisoval zbytek souboru
                    HANDLES(CloseHandle(file->HFile)); // zavreme invalidni handle, protoze uz by z nej stejne neslo cist
                }

                file->HFile = HANDLES_Q(CreateFile(file->FileName, file->dwDesiredAccess, file->dwShareMode, NULL,
                                                   file->dwCreationDisposition, file->dwFlagsAndAttributes, NULL));
                if (file->HFile != INVALID_HANDLE_VALUE) // otevreno, jeste nastavime offset
                {
                    //SEEK:
                    LONG lo = currentSeekLo;
                    LONG hi = currentSeekHi;
                    lo = SetFilePointer(file->HFile, lo, &hi, FILE_BEGIN);
                    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR)
                        goto WRITE_ERROR; // nelze nastavit offset, zkusime to znovu
                    if (lo != (long)currentSeekLo || hi != currentSeekHi)
                    {
                        SetLastError(ERROR_SEEK_ON_DEVICE);
                        goto WRITE_ERROR; // nelze nastavit offset (soubor uz muze byt mensi), zkusime to znovu
                    }
                }
                else // nejde otevrit, problem trva ...
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
                    *pressedButton = dlgRet; // vratim tlacitko, na ktere user kliknul
                return FALSE;
            }
            }
        }
    }
}
