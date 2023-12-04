// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "splitcbn.h"
#include "splitcbn.rh"
#include "splitcbn.rh2"
#include "lang\lang.rh"
#include "split.h"
#include "dialogs.h"

// *****************************************************************************
//
//  SplitFile
//

#define MAX_CMDLINE 127 // max. delka prikazove radky
#define MAX_FILENAME 34 // max. delka jmena souboru, aby se i minimalni prikaz copy jeste vesel do radky
#define MAX_BAT 100000

#define BUFSIZE1 (128 * 1024) // velikost bufferu pro removable drive (je maly kvuli kontrole stisku ESC)
#define BUFSIZE2 (256 * 1024) // velikost pro ostatni

static BOOL EnsureDiskInsertedEtc(LPTSTR targetDir, CQuadWord& qwPartSize, CQuadWord* freeSpace,
                                  UINT driveType, CQuadWord& bytesRemaining, CQuadWord* thisPartSize, HWND parent)
{
    CALL_STACK_MESSAGE1("EnsureDiskInserted()");
    *freeSpace = CQuadWord(1, 0);
    if (driveType == DRIVE_REMOVABLE)
    {
        while (1)
        {
            DWORD err;
            while ((err = SalamanderGeneral->SalCheckPath(FALSE, targetDir, ERROR_SUCCESS, parent)) == ERROR_USER_TERMINATED)
            {
                if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_CANCEL), LoadStr(IDS_SPLIT),
                                                     MB_YESNO | MB_ICONQUESTION) == IDYES)
                    return FALSE;
            }
            if (err == ERROR_SUCCESS && qwPartSize == SIZE_AUTODETECT)
            {
                SalamanderGeneral->GetDiskFreeSpace(freeSpace, targetDir, NULL);
                if (*freeSpace == CQuadWord(-1, -1))
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_OUTOFSPACE), LoadStr(IDS_SPLIT), MSGBOX_ERROR);
                    return FALSE;
                }
            }

            if (err != ERROR_SUCCESS || *freeSpace == CQuadWord(0, 0))
            {
                if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_INSERTDISK),
                                                     LoadStr(IDS_SPLIT), MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
                    return FALSE;
            }
            else
                break;
        }
    }

    if (driveType != DRIVE_REMOVABLE || qwPartSize != SIZE_AUTODETECT)
        SalamanderGeneral->GetDiskFreeSpace(freeSpace, targetDir, NULL);

    if (qwPartSize == SIZE_AUTODETECT)
    {
        *thisPartSize = (*freeSpace < bytesRemaining) ? *freeSpace : bytesRemaining;
    }
    else
    {
        *thisPartSize = (qwPartSize < bytesRemaining) ? qwPartSize : bytesRemaining;
        while (*freeSpace < *thisPartSize)
        {
            if (driveType != DRIVE_REMOVABLE)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_OUTOFSPACE), LoadStr(IDS_SPLIT), MSGBOX_ERROR);
                return FALSE;
            }
            else
            {
                if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_INSERTDISK), LoadStr(IDS_SPLIT),
                                                      MSGBOX_EX_ERROR) == IDCANCEL)
                    return FALSE;
                SalamanderGeneral->GetDiskFreeSpace(freeSpace, targetDir, NULL);
            }
        }
    }
    return TRUE;
}

static void AddEchoEscapeCharacters(char* buf, const char* name)
{
    char* d = buf;
    const char* s = name;
    while (*s != 0)
    {
        if (*s == '&' || *s == '^' || *s == '|' || *s == '<' || *s == '>')
            *d++ = '^';
        *d++ = *s++;
    }
    *d = 0;
}

static BOOL SplitFile(LPTSTR fileName, LPTSTR targetDir, CQuadWord& qwPartSize,
                      HWND parent, CSalamanderForOperationsAbstract* salamander)
{
    CALL_STACK_MESSAGE4("SplitFile(%s, %s, %I64u, , )", fileName, targetDir, qwPartSize.Value);

    // vytvoreni target cesty
    DWORD silent = 0;
    BOOL bSkip;
    if (SalamanderSafeFile->SafeFileCreate(targetDir, 0, 0, 0, TRUE, parent, NULL, NULL, &silent,
                                           TRUE, &bSkip, NULL, 0, NULL, NULL) == INVALID_HANDLE_VALUE)
        return FALSE;

    // otevreni souboru
    SAFE_FILE file;
    if (!SalamanderSafeFile->SafeFileOpen(&file, fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                                          FILE_FLAG_SEQUENTIAL_SCAN, parent, BUTTONS_RETRYCANCEL, NULL, NULL))
    {
        return FALSE;
    }
    // ziskani velikosti souboru
    CQuadWord bytesRemaining;
    bytesRemaining.LoDWord = GetFileSize(file.HFile, &bytesRemaining.HiDWord);

    // ziskani casu souboru
    FILETIME ft;
    GetFileTime(file.HFile, NULL, NULL, &ft);

    // ziskani zakladu jmen souboru
    char name[MAX_PATH];
    strcpy(name, SalamanderGeneral->SalPathFindFileName(fileName));
    if (!configIncludeFileExt)
        StripExtension(name);

    // zjisteni typu ciloveho media
    char text[MAX_PATH + 50]; // musi byt delsi nez MAX_PATH kvuli "too long name" (niz nize)
    SalamanderGeneral->GetRootPath(text, targetDir);
    UINT driveType = GetDriveType(text);

    BOOL abort = FALSE;

    // test jestli user nezvolil Autodetect na fixed drive
    if (driveType != DRIVE_REMOVABLE && qwPartSize == SIZE_AUTODETECT)
    {
        if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_FIXEDNOSENSE),
                                             LoadStr(IDS_SPLIT), MB_YESNO | MB_ICONQUESTION) == IDNO)
            abort = TRUE;
    }

    // test na vice jak 100 casti
    if (!qwPartSize.Value || (bytesRemaining - CQuadWord(1, 0)) / qwPartSize + CQuadWord(1, 0) > CQuadWord(100, 0))
    {
        if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_TOOMANYPARTS),
                                             LoadStr(IDS_SPLIT), MB_YESNO | MB_ICONQUESTION) == IDNO)
            abort = TRUE;
    }

    // pokud splitujeme na fixed disk, overime ze je tam misto (na batch file kaslu)
    if (!abort && driveType != DRIVE_REMOVABLE &&
        !SalamanderGeneral->TestFreeSpace(parent, targetDir, bytesRemaining, LoadStr(IDS_SPLIT)))
        abort = TRUE;

    // TODO! presnejsi test
    // kontrola jestli neni jmeno prilis dlouhe (batak by nemusel fungovat)
    if (!abort && configCreateBatchFile && strlen(name) > MAX_FILENAME)
    {
        if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_TOOLONGNAME),
                                             LoadStr(IDS_SPLIT), MB_YESNO | MB_ICONQUESTION) == IDNO)
            abort = TRUE;
    }

    if (abort)
    {
        SalamanderSafeFile->SafeFileClose(&file);
        return TRUE;
    }

    // alokace bufferu
    DWORD dwBufSize = (driveType == DRIVE_REMOVABLE) ? BUFSIZE1 : BUFSIZE2;
    char* pBuffer = new char[dwBufSize];
    if (pBuffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_OUTOFMEM), LoadStr(IDS_SPLIT), MSGBOX_ERROR);
        SalamanderSafeFile->SafeFileClose(&file);
        return FALSE;
    }

    // init CRC
    UINT32 Crc = 0;

    // progress dialog
    salamander->OpenProgressDialog(LoadStr(IDS_SPLIT), TRUE, NULL, FALSE);
    salamander->ProgressSetTotalSize(CQuadWord(-1, -1), bytesRemaining);
    BOOL delayed = (driveType != DRIVE_REMOVABLE); // u splitovani na diskety se udajne nestihal prekreslovat dialog
    salamander->ProgressSetSize(CQuadWord(-1, -1), CQuadWord(0, 0), delayed);
    CQuadWord totalProgress = CQuadWord(0, 0), fileProgress;

    // parent uz je od ted progress dialog
    parent = SalamanderGeneral->GetMsgBoxParent();

    BOOL ret = TRUE;
    int partNum = 1;
    char name2[MAX_PATH];
    char buf[50];
    CQuadWord thisPartSize;
    CQuadWord freeSpace;

    while (bytesRemaining.Value)
    {
        if (!EnsureDiskInsertedEtc(targetDir, qwPartSize, &freeSpace, driveType, bytesRemaining, &thisPartSize, parent))
        {
            ret = FALSE;
            break;
        }

        // vyrobime jmeno ciloveho souboru
        sprintf(name2, "%s.%#03ld", name, partNum++);
        strncpy_s(text, MAX_PATH, targetDir, _TRUNCATE);
        if (!SalamanderGeneral->SalPathAppend(text, name2, MAX_PATH))
        { // too long name - ohlasi se v SalamanderSafeFile->SafeFileCreate
            char* end = text + strlen(text);
            if (end > text && *(end - 1) != '\\')
                *end++ = '\\';
            strncpy_s(end, _countof(text) - (end - text), name2, _TRUNCATE);
        }

        // vytvoreni souboru
        GetInfo(buf, thisPartSize);
        SAFE_FILE outfile;
        if (SalamanderSafeFile->SafeFileCreate(text, GENERIC_WRITE, FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL,
                                               FALSE, parent, name2, buf, &silent, TRUE, &bSkip, NULL, 0, NULL, &outfile) == INVALID_HANDLE_VALUE &&
            !bSkip)
        {
            ret = FALSE;
            break;
        }

        // konecne: vykopirovani thisPartSize bajtu vstupniho souboru do vystupniho
        salamander->ProgressSetTotalSize(thisPartSize, CQuadWord(-1, -1));
        salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), delayed);
        fileProgress = CQuadWord(0, 0);
        if (!bSkip)
        {
            char text2[MAX_PATH + 50];
            sprintf(text2, "%s %s...", LoadStr(IDS_WRITING), name2);
            salamander->ProgressDialogAddText(text2, delayed);

            CQuadWord numBytes = thisPartSize;
            while (numBytes.Value)
            {
                DWORD toread = (numBytes > CQuadWord(dwBufSize, 0)) ? dwBufSize : numBytes.LoDWord;
                DWORD numread, numwr;
                if (!SalamanderSafeFile->SafeFileRead(&file, pBuffer, toread, &numread, parent, BUTTONS_RETRYCANCEL, NULL, NULL) ||
                    !SalamanderSafeFile->SafeFileWrite(&outfile, pBuffer, numread, &numwr, parent, BUTTONS_RETRYCANCEL, NULL, NULL))
                {
                    ret = FALSE;
                    break;
                }
                Crc = SalamanderGeneral->UpdateCrc32(pBuffer, numread, Crc);
                CQuadWord qwnr(numread, 0);
                numBytes -= qwnr;
                fileProgress += qwnr;
                if (!salamander->ProgressSetSize(fileProgress, totalProgress + fileProgress, delayed))
                {
                    ret = FALSE;
                    break;
                }
            }
            SalamanderSafeFile->SafeFileClose(&outfile);
            if (ret == FALSE)
            {
                DeleteFile(text);
                break;
            }
        }
        else
        {
            CQuadWord distance = thisPartSize; // 'thisPartSize' se nesmi zmenit (zmeni se misto ni 'distance')
            SalamanderSafeFile->SafeFileSeekMsg(&file, &distance, FILE_CURRENT, parent,
                                                BUTTONS_RETRYCANCEL, NULL, NULL, TRUE);
        }

        bytesRemaining -= thisPartSize;
        totalProgress += thisPartSize;

        // upozorneni na vlozeni dalsiho disku
        if (bytesRemaining.Value && driveType == DRIVE_REMOVABLE &&
            (qwPartSize == SIZE_AUTODETECT || freeSpace - qwPartSize < qwPartSize))
        {
            if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_INSERTNEXT),
                                                 LoadStr(IDS_SPLIT), MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
            {
                ret = FALSE;
                break;
            }
        }
    }

    delete[] pBuffer;
    SalamanderSafeFile->SafeFileClose(&file);

    // vytvoreni batch file

    if (ret != FALSE && configCreateBatchFile)
    {
        char* batfile = new char[MAX_BAT];
        char* line = new char[4 * MAX_PATH];
        int nparts = partNum - 1;
        int linenum = 1, partnum = 1;
        const char* origName = SalamanderGeneral->SalPathFindFileName(fileName);

        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        AddEchoEscapeCharacters(batfile, origName);
        sprintf(line, LoadStr(IDS_BATFILE_DESCR), batfile);
        sprintf(batfile,
                "@echo off\r\n"
                "rem %s, https://www.altap.cz\r\n"
                "rem name=%s\r\n"
                "rem crc32=%X\r\n"
                "rem time=%d-%d-%d %d:%02d:%02d\r\n"
                "echo %s\r\n"
                "echo %s\r\n"
                "pause\r\n",
                LoadStr(IDS_BATFILE_GENBYSAL), origName, Crc,
                (int)st.wYear, (int)st.wMonth, (int)st.wDay, (int)st.wHour, (int)st.wMinute, (int)st.wSecond,
                line, LoadStr(IDS_BATFILE_CTRL_C_TO_QUIT));

        while (partnum <= nparts)
        {
            strcpy(line, "copy /b ");
            if (linenum == 1)
            { // 1. prikaz copy, zacatek = "xxx.001"+"xxx.002"
                strcat(line, "\"");
                strcat(line, name);
                sprintf(buf, ".%#03ld", partnum++);
                strcat(line, buf);
                if (nparts > 1)
                {
                    strcat(line, "\"+\"");
                    strcat(line, name);
                    sprintf(buf, ".%#03ld", partnum++);
                    strcat(line, buf);
                }
            }
            else
            { // dalsi prikazy copy - zacatek = "xxx"+"xxx.yyy"
                strcat(line, "\"");
                strcat(line, origName);
                strcat(line, "\"+\"");
                strcat(line, name);
                sprintf(buf, ".%#03ld", partnum++);
                strcat(line, buf);
            }
            strcat(line, "\"");

            while ((partnum < nparts) &&
                   (strlen(line) + strlen(origName) + strlen(name) + 10 <= MAX_CMDLINE))
            { // zbytek radku az do vycerpani vsech casti nebo max. delky radku
                strcat(line, "+\"");
                strcat(line, name);
                sprintf(buf, ".%#03ld", partnum++);
                strcat(line, buf);
                strcat(line, "\"");
            }

            // konec radku
            strcat(line, " \"");
            strcat(line, origName);
            strcat(line, "\"\r\n");
            linenum++;

            if (strlen(batfile) + strlen(line) < MAX_BAT)
            {
                strcat(batfile, line);
            }
            else
            {
                ret = FALSE;
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_BATTOOLONG), LoadStr(IDS_SPLIT), MSGBOX_ERROR);
                break;
            }
        }

        if (ret)
        {
            CQuadWord batSize((DWORD)strlen(batfile), 0);
            bytesRemaining = batSize;
            thisPartSize = batSize;
            CharToOem(batfile, batfile);

            SalamanderGeneral->GetDiskFreeSpace(&freeSpace, targetDir, NULL);
            if (driveType == DRIVE_REMOVABLE && freeSpace < batSize)
            { // "vloz dalsi disk"
                if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_INSERTNEXT),
                                                     LoadStr(IDS_SPLIT), MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
                    ret = FALSE;
            }

            if (ret)
                if (EnsureDiskInsertedEtc(targetDir, batSize, &freeSpace, driveType, bytesRemaining,
                                          &thisPartSize, parent))
                {
                    strcpy(name2, name);
                    strcat(name2, ".bat");
                    strncpy_s(text, MAX_PATH, targetDir, _TRUNCATE);
                    if (!SalamanderGeneral->SalPathAppend(text, name2, MAX_PATH))
                    { // too long name - ohlasi se v SalamanderSafeFile->SafeFileCreate
                        char* end = text + strlen(text);
                        if (end > text && *(end - 1) != '\\')
                            *end++ = '\\';
                        strncpy_s(end, _countof(text) - (end - text), name2, _TRUNCATE);
                    }

                    GetInfo(buf, batSize);
                    SAFE_FILE bf;
                    if (SalamanderSafeFile->SafeFileCreate(text, GENERIC_WRITE, FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL,
                                                           FALSE, parent, name2, buf, &silent, TRUE, &bSkip, NULL, 0, NULL, &bf) != INVALID_HANDLE_VALUE &&
                        !bSkip)
                    {
                        sprintf(text, "%s %s", LoadStr(IDS_WRITING), name2);
                        salamander->ProgressDialogAddText(text, TRUE);
                        DWORD numw;
                        SalamanderSafeFile->SafeFileWrite(&bf, batfile, batSize.LoDWord, &numw, parent, BUTTONS_RETRYCANCEL, NULL, NULL);
                        SalamanderSafeFile->SafeFileClose(&bf);
                    }
                }
        }

        delete[] line;
        delete[] batfile;
    }

    salamander->CloseProgressDialog();
    SalamanderGeneral->PostChangeOnPathNotification(targetDir, FALSE);
    return ret;
}

// *****************************************************************************
//
//  SplitCommand
//

BOOL SplitCommand(HWND parent, CSalamanderForOperationsAbstract* salamander)
{
    CALL_STACK_MESSAGE1("SplitCommand( , )");
    // ziskani informaci o souboru
    char targetdir[MAX_PATH];
    const CFileData* pfd;
    BOOL isDir;
    pfd = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
    GetTargetDir(targetdir, pfd->Name, TRUE);

    // zjisteni velikosti souboru
    char path[MAX_PATH];
    WIN32_FIND_DATA wfd;
    HANDLE hFind;
    CQuadWord qwFileSize;
    SalamanderGeneral->GetPanelPath(PANEL_SOURCE, path, MAX_PATH, NULL, NULL);
    BOOL tooLong = !SalamanderGeneral->SalPathAppend(path, pfd->Name, MAX_PATH);
    if (!tooLong && (hFind = FindFirstFile(path, &wfd)) != INVALID_HANDLE_VALUE)
    {
        FindClose(hFind);
        qwFileSize.Set(wfd.nFileSizeLow, wfd.nFileSizeHigh);
        if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            (wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            if (!SalamanderGeneral->SalGetFileSize2(path, qwFileSize, NULL))
                qwFileSize.Set(wfd.nFileSizeLow, wfd.nFileSizeHigh);
        }
    }
    else
    {
        if (tooLong)
            SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return Error(IDS_SPLIT, IDS_OPENERROR);
    }

    // soubory mensi nez 2 byty nejdou rozdelit
    if (qwFileSize.Value < 2)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ZEROSIZE), LoadStr(IDS_SPLIT), MSGBOX_WARNING);
        return TRUE;
    }

    // split dialog
    CQuadWord qwPartialSize;
    if (!SplitDialog(pfd->Name, qwFileSize, targetdir, &qwPartialSize, parent))
        return FALSE;

    // kontrola
    char panelpath[MAX_PATH];
    GetTargetDir(panelpath, NULL, TRUE);
    if (!MakePathAbsolute(targetdir, TRUE, panelpath, !configSplitToOther, IDS_SPLIT))
        return FALSE;

    // rozdeleni souboru
    return SplitFile(path, targetdir, qwPartialSize, parent, salamander);
}
