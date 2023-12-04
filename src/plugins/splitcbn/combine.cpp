// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <tchar.h>
#include "splitcbn.h"
#include "splitcbn.rh"
#include "splitcbn.rh2"
#include "lang\lang.rh"
#include "combine.h"
#include "dialogs.h"

// *****************************************************************************
//
//  Combine Files
//

#define BUFSIZE (512 * 1024)

BOOL CombineFiles(TIndirectArray<char>& files, LPTSTR targetName,
                  BOOL bOnlyCrc, BOOL bTestCrc, UINT32& Crc,
                  BOOL bTime, FILETIME* origTime, HWND parent,
                  CSalamanderForOperationsAbstract* salamander)
{
    CALL_STACK_MESSAGE4("CombineFiles( , %s, %ld, %X, , )", targetName, bTestCrc, Crc);

    if (!bOnlyCrc && !files.Count)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ZEROFILES), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
        return FALSE;
    }

    int idTitle = bOnlyCrc ? IDS_CRCTITLE : IDS_COMBINE;

    // nascitani velikosti vsech partial files (soucasne kontrola jejich pristupnosti)
    CQuadWord totalSize = CQuadWord(0, 0);
    char text[MAX_PATH + 50];
    int i;
    for (i = 0; i < files.Count; i++)
    {
        SAFE_FILE file;
        if (!SalamanderSafeFile->SafeFileOpen(&file, files[i], GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                                              0, parent, BUTTONS_RETRYCANCEL, NULL, NULL))
        {
            return FALSE;
        }
        CQuadWord size;
        size.LoDWord = GetFileSize(file.HFile, &size.HiDWord);
        totalSize += size;
        SalamanderSafeFile->SafeFileClose(&file);
    }

    // test volneho mista
    if (!bOnlyCrc)
    {
        char dir[MAX_PATH];
        strncpy_s(dir, targetName, _TRUNCATE);
        SalamanderGeneral->CutDirectory(dir);
        if (!SalamanderGeneral->TestFreeSpace(parent, dir, totalSize, LoadStr(IDS_COMBINE)))
            return FALSE;
    }

    // vytvoreni vystupniho souboru
    SAFE_FILE outfile;
    if (!bOnlyCrc)
    {
        if (SalamanderSafeFile->SafeFileCreate(targetName, GENERIC_WRITE, FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL,
                                               FALSE, parent, NULL, NULL, NULL, FALSE, NULL, NULL, 0, NULL, &outfile) == INVALID_HANDLE_VALUE)
        {
            return FALSE;
        }
    }

    // spojeni
    char* pBuffer = new char[BUFSIZE];
    if (pBuffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_OUTOFMEM), LoadStr(idTitle), MSGBOX_ERROR);
        if (!bOnlyCrc)
            SalamanderSafeFile->SafeFileClose(&outfile);
        return FALSE;
    }

    UINT32 CrcVal = 0;

    // otevreni progress dialogu
    salamander->OpenProgressDialog(LoadStr(idTitle), TRUE, parent, FALSE);
    salamander->ProgressSetTotalSize(CQuadWord(-1, -1), totalSize);
    salamander->ProgressSetSize(CQuadWord(-1, -1), CQuadWord(0, 0), FALSE);
    CQuadWord totalProgress = CQuadWord(0, 0);

    int ret = TRUE;
    int j;
    for (j = 0; j < files.Count; j++)
    {
        sprintf(text, "%s %s...", LoadStr(IDS_PROCESSING), files[j]);
        salamander->ProgressDialogAddText(text, TRUE);

        SAFE_FILE file;
        if (!SalamanderSafeFile->SafeFileOpen(&file, files[j], GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                                              FILE_FLAG_SEQUENTIAL_SCAN, parent, BUTTONS_RETRYCANCEL, NULL, NULL))
        {
            ret = FALSE;
            break;
        }

        DWORD numread, numwr;
        CQuadWord currentProgress = CQuadWord(0, 0), size;
        size.LoDWord = GetFileSize(file.HFile, &size.HiDWord);
        salamander->ProgressSetTotalSize(size, CQuadWord(-1, -1));
        salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);
        do
        {
            if (!SalamanderSafeFile->SafeFileRead(&file, pBuffer, BUFSIZE, &numread, parent, BUTTONS_RETRYCANCEL, NULL, NULL))
            {
                ret = FALSE;
                break;
            }
            if (!bOnlyCrc && numread)
            {
                if (!SalamanderSafeFile->SafeFileWrite(&outfile, pBuffer, numread, &numwr, parent, BUTTONS_RETRYCANCEL, NULL, NULL))
                {
                    ret = FALSE;
                    break;
                }
            }
            CrcVal = SalamanderGeneral->UpdateCrc32(pBuffer, numread, CrcVal);
            currentProgress += CQuadWord(numread, 0);
            if (!salamander->ProgressSetSize(currentProgress, totalProgress + currentProgress, TRUE))
            {
                ret = FALSE;
                break;
            }
        } while (numread == BUFSIZE);

        totalProgress += currentProgress;
        SalamanderSafeFile->SafeFileClose(&file);
        if (ret == FALSE)
            break;
    }

    salamander->CloseProgressDialog();
    delete[] pBuffer;
    if (!bOnlyCrc)
    {
        if (ret)
        {
            if (bTime)
                SetFileTime(outfile.HFile, NULL, NULL, origTime);
            SalamanderSafeFile->SafeFileClose(&outfile);

            char* name = (char*)SalamanderGeneral->SalPathFindFileName(targetName);
            if (name > targetName)
            {
                name[-1] = 0;
                SalamanderGeneral->PostChangeOnPathNotification(targetName, FALSE);
            }
        }
        else
        {
            SalamanderSafeFile->SafeFileClose(&outfile);
            DeleteFile(targetName);
        }
    }

    if (!bOnlyCrc)
    {
        if (ret && bTestCrc && Crc != CrcVal)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CRCERROR), LoadStr(idTitle), MSGBOX_ERROR);
            ret = FALSE;
        }
    }
    else
        Crc = CrcVal;

    return ret;
}

// *****************************************************************************
//
//  CalculateFileCRC
//

/*BOOL CalculateFileCRC(UINT32& Crc, HWND parent, CSalamanderForOperationsAbstract* salamander)
{      
  CALL_STACK_MESSAGE1("CalculateFileCRC()");
  HANDLE hFile;
  const CFileData* pfd = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, NULL);
  char path[MAX_PATH];
  SalamanderGeneral->GetPanelPath(PANEL_SOURCE, path, MAX_PATH, NULL, NULL);
  if (!SalamanderGeneral->SalPathAppend(path, pfd->Name, MAX_PATH))
  {
    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_CALCCRC), MSGBOX_ERROR);
    return FALSE;
  }
  if ((hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
    FILE_FLAG_SEQUENTIAL_SCAN, NULL)) == INVALID_HANDLE_VALUE)
  {
    return Error(IDS_CALCCRC, IDS_OPENERROR);
  }

  char* pBuffer = new char[BUFSIZE];
  if (pBuffer == NULL)
  {
    CloseHandle(hFile);
    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_OUTOFMEM), LoadStr(IDS_CALCCRC), MSGBOX_ERROR);
    return FALSE;
  }

  Crc = 0;

  salamander->OpenProgressDialog(LoadStr(IDS_CALCCRC), FALSE, NULL, FALSE);
  salamander->ProgressSetTotalSize(CQuadWord(GetFileSize(hFile, NULL), 0), CQuadWord(-1, -1));

  DWORD numread;
  int ret = TRUE;
  CQuadWord progress = CQuadWord(0, 0);
  do
  {
    if (!SafeReadFile(hFile, pBuffer, BUFSIZE, &numread, path))
    {
      ret = FALSE;
      break;
    }
    Crc = SalamanderGeneral->UpdateCrc32(pBuffer, numread, Crc);
    progress += CQuadWord(numread, 0);
    if (!salamander->ProgressSetSize(progress, CQuadWord(-1, -1), TRUE)) 
    {
      ret = FALSE;
      break;
    }
  }
  while (numread == BUFSIZE);

  salamander->CloseProgressDialog();
  delete [] pBuffer;
  CloseHandle(hFile);
  return ret;
}*/

// *****************************************************************************
//
//  CombineCommand
//

static BOOL AddFile(TIndirectArray<char>& files, LPTSTR sourceDir, LPTSTR name, BOOL bReverse)
{
    CALL_STACK_MESSAGE1("AllocName( , , )");
    char* str = (char*)malloc(strlen(sourceDir) + 2 + strlen(name));
    if (str == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_OUTOFMEM), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
        return FALSE;
    }
    strcpy(str, sourceDir);
    if (!SalamanderGeneral->SalPathAppend(str, name, MAX_PATH))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
        return FALSE;
    }
    if (bReverse)
        files.Insert(0, str);
    else
        files.Add(str);
    return TRUE;
}

static BOOL IsInPanel(LPTSTR fileName)
{
    CALL_STACK_MESSAGE1("IsInPanel()");
    int index = 0;
    const CFileData* pfd;
    BOOL isDir;
    while ((pfd = SalamanderGeneral->GetPanelItem(PANEL_SOURCE, &index, &isDir)) != NULL)
        if (!isDir)
            if (!lstrcmpi(fileName, pfd->Name))
                return TRUE;
    return FALSE;
}

static BOOL FindValue(const char*& p)
{
    CALL_STACK_MESSAGE1("FindValue()");
    while (*p && *p != '\r' && *p != '\n' && (*p == ' ' || *p == '\t'))
        p++;
    if (*p != '=' && *p != ':')
        return FALSE;
    p++;
    while (*p && *p != '\r' && *p != '\n' && (*p == ' ' || *p == '\t'))
        p++;
    return *p && *p != '\r' && *p != '\n';
}

static BOOL FindCrc(const char* text, LPCTSTR searchstring, UINT32& crc)
{
    CALL_STACK_MESSAGE2("FindCrc( , %s, )", searchstring);
    const char* p = strstr(text, searchstring);
    if (p != NULL)
    {
        p += strlen(searchstring);
        if (FindValue(p))
            return sscanf(p, "%x", &crc) == 1;
        else
            return FALSE;
    }
    else
        return FALSE;
}

static BOOL FindName(const char* text, const char* text_locase, LPCTSTR searchstring, LPTSTR name)
{
    CALL_STACK_MESSAGE2("FindName( , %s, )", searchstring);
    const char* p = strstr(text_locase, searchstring);
    if (p != NULL)
    {
        p += strlen(searchstring);
        if (FindValue(p))
        {
            p += text - text_locase;
            if (*p == '\"')
                p++;
            char* q = name;
            while (*p && *p != '\r' && *p != '\n' && *p != '\"' && (q - name < MAX_PATH))
                *q++ = *p++;
            *q = 0;
            return TRUE;
        }
        else
            return FALSE;
    }
    else
        return FALSE;
}

static BOOL FindTime(const char* text, LPCTSTR searchstring, FILETIME* ft)
{
    CALL_STACK_MESSAGE2("FindTime( , %s, )", searchstring);
    const char* p = strstr(text, searchstring);
    if (p != NULL)
    {
        p += strlen(searchstring);
        if (FindValue(p))
        {
            SYSTEMTIME st;
            st.wMilliseconds = 0;
            if (sscanf(p, "%hu-%hu-%hu %hu:%hu:%hu", &st.wYear, &st.wMonth, &st.wDay, &st.wHour, &st.wMinute, &st.wSecond) != 6)
                return FALSE;
            return SystemTimeToFileTime(&st, ft);
        }
        else
            return FALSE;
    }
    else
        return FALSE;
}

static void AnalyzeFile(LPTSTR fileName, LPTSTR origName, UINT32& origCrc, FILETIME* origTime,
                        BOOL& bNameAcquired, BOOL& bCrcAcquired, BOOL& bTimeAcquired)
{
    CALL_STACK_MESSAGE2("AnalyzeFile(%s, , , , )", fileName);
    bNameAcquired = bCrcAcquired = FALSE;
    // nacteni souboru do bufferu
    HANDLE hFile;
    if ((hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, NULL)) == INVALID_HANDLE_VALUE)
        return;
    DWORD size = GetFileSize(hFile, NULL), numread;
    // j.r. 21.1.2003: pri rozdeleni na 999 jsem dostal davku o velikosti 30KB
    //  if (size > 10000) { CloseHandle(hFile); return; } // na tak velky soubory kaslem
    if (size > 200000)
    {
        CloseHandle(hFile);
        return;
    } // na tak velky soubory kaslem
    char* text = new char[size + 1];
    char* text_locase = new char[size + 1];
    if (text == NULL || text_locase == NULL)
    {
        CloseHandle(hFile);
        return;
    }
    BOOL ok = ReadFile(hFile, text, size, &numread, NULL);
    CloseHandle(hFile);
    if (!ok || size != numread)
    {
        delete[] text;
        delete[] text_locase;
        return;
    }
    text[size] = 0;

    strcpy(text_locase, text);
    CharLower(text_locase);
    // zkusime najit "crc32" nebo "crc"
    if (!bCrcAcquired)
    {
        bCrcAcquired = FindCrc(text_locase, "crc32", origCrc);
        if (!bCrcAcquired)
            bCrcAcquired = FindCrc(text_locase, "crc", origCrc);
    }
    // "filename" nebo "name"
    if (!bNameAcquired)
    {
        bNameAcquired = FindName(text, text_locase, "filename", origName);
        if (!bNameAcquired)
            bNameAcquired = FindName(text, text_locase, "name", origName);
    }
    // "time"
    if (!bTimeAcquired)
    {
        bTimeAcquired = FindTime(text_locase, "time", origTime);
    }

    delete[] text;
    delete[] text_locase;
}

BOOL CombineCommand(DWORD eventMask, HWND parent, CSalamanderForOperationsAbstract* salamander)
{
    CALL_STACK_MESSAGE2("CombineCommand(%X, , )", eventMask);

    TIndirectArray<char> files(100, 100, dtDelete);

    char sourceDir[MAX_PATH];
    SalamanderGeneral->GetPanelPath(PANEL_SOURCE, sourceDir, MAX_PATH, NULL, NULL);

    BOOL bTestCompanionFile = FALSE;
    char companionFile[MAX_PATH];
    char name1[MAX_PATH], name2[MAX_PATH];
    const CFileData* pfd;
    BOOL isDir;

    if (eventMask & MENU_EVENT_FILES_SELECTED)
    { // jsou vybrany soubory
        int index = 0;
        BOOL bAllSameNames = TRUE;
        BOOL bFirst = TRUE;

        // nacteme do pole vybrane polozky (krome adresaru)
        while ((pfd = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir)) != NULL)
        {
            if (!isDir)
            {
                if (bFirst)
                {
                    strcpy(name1, pfd->Name);
                    StripExtension(name1);
                    bFirst = FALSE;
                }
                else if (bAllSameNames)
                {
                    strcpy(name2, pfd->Name);
                    StripExtension(name2);
                    if (lstrcmpi(name1, name2))
                        bAllSameNames = FALSE;
                }
                if (!AddFile(files, sourceDir, pfd->Name, FALSE))
                    return FALSE;
            }
        }

        // jestlize byla vsechna jmena stejna, je sance ze najdeme i doprovodny .BAT nebo .CRC
        bTestCompanionFile = bAllSameNames;
        if (!bAllSameNames)
            strcpy(name1, "combinedfile");
        strcpy(companionFile, sourceDir);
        if (!SalamanderGeneral->SalPathAppend(companionFile, name1, MAX_PATH) ||
            strlen(companionFile) + 1 >= MAX_PATH) // test pro strcat() nize
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
            return FALSE;
        }
        strcat(companionFile, ".");
    }
    else // pouze fokus na souboru - snazime se rozsirit vyber o "vyssi" soubory
    {
        pfd = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
        if (pfd == NULL || isDir)
        {
            TRACE_E("CombineCommand(): Neni fokus na souboru ?!?");
            return FALSE;
        }
        BOOL bJustOneFile = FALSE;
        // nejprve analyza pripony
        char* ext = _tcsrchr(pfd->Name, '.');
        if (ext != NULL) // ".cvspass" ve Windows je pripona
        {
            BOOL bZeroPadded, bAddThisFile = TRUE;
            int nextIndex;
            if (!lstrcmpi(ext, ".tns"))
            { // tns = Turbo Navigator Split - povazujeme za "000"
                bZeroPadded = FALSE;
                nextIndex = 2;
            }
            else if (!lstrcmpi(ext, ".bat") || !lstrcmpi(ext, ".crc"))
            { // bat Salamandera nebo crc WinCommandera; s TN soubory to tady fungovat nebude
                bZeroPadded = TRUE;
                nextIndex = 1;
                bAddThisFile = FALSE;
            }
            else
            { // je pripona slozena z cislic?
                BOOL bNumbers = TRUE;
                int numberCount = 0, i = 1;
                while (ext[i])
                    if (ext[i] < '0' || ext[i] > '9')
                    {
                        bNumbers = FALSE;
                        break;
                    }
                    else
                    {
                        numberCount++;
                        i++;
                    }
                if (!bNumbers || numberCount > 3)
                    bJustOneFile = TRUE; // divna pripona - nic rozsirovat nebudem
                else
                {
                    bZeroPadded = (ext[1] == '0');
                    nextIndex = atol(ext + 1) + 1;
                }
            }

            lstrcpyn(name1, pfd->Name, (int)(ext - pfd->Name + 2));
            strcpy(companionFile, sourceDir);
            if (!SalamanderGeneral->SalPathAppend(companionFile, name1, MAX_PATH))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
                return FALSE;
            }

            if (!bJustOneFile)
            {
                if (nextIndex > 1)
                {
                    int prevIndex = nextIndex - 2;
                    while (1)
                    {
                        sprintf(name2, bZeroPadded ? "%s%#03ld" : "%s%ld", name1, prevIndex--);
                        if (!IsInPanel(name2))
                            break;
                        if (!AddFile(files, sourceDir, name2, TRUE))
                            return FALSE;
                    }
                }

                strcpy(name2, pfd->Name);
                bTestCompanionFile = TRUE;
                do
                {
                    if (bAddThisFile)
                        if (!AddFile(files, sourceDir, name2, FALSE))
                            return FALSE;
                    sprintf(name2, bZeroPadded ? "%s%#03ld" : "%s%ld", name1, nextIndex++);
                    bAddThisFile = TRUE;
                } while (IsInPanel(name2));
            }
        }
        else
        { // chybi pripona - kaslem na to, nic rozsirovat nebudem
            bJustOneFile = TRUE;
            strcpy(companionFile, sourceDir);
            if (!SalamanderGeneral->SalPathAppend(companionFile, "combinedfile", MAX_PATH))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
                return FALSE;
            }
        }

        if (bJustOneFile)
            if (!AddFile(files, sourceDir, pfd->Name, FALSE))
                return FALSE;
    }

    BOOL bName = FALSE, bCrc = FALSE, bTime = FALSE;
    UINT32 origCrc;
    FILETIME origTime;

    if (bTestCompanionFile)
    { // prohlidneme pripadny BAT nebo CRC
        size_t ext = strlen(companionFile);
        if (ext + 3 >= MAX_PATH) // test pro strcat() nize
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
            return FALSE;
        }
        strcat(companionFile, "bat");
        AnalyzeFile(companionFile, name2, origCrc, &origTime, bName, bCrc, bTime);
        if (!bName || !bCrc)
        {
            companionFile[ext] = 0;
            strcat(companionFile, "crc");
            AnalyzeFile(companionFile, name2, origCrc, &origTime, bName, bCrc, bTime);
        }
        companionFile[ext] = 0;
        if (bName)
        {
            strcpy(name1, sourceDir);
            if (!SalamanderGeneral->SalPathAppend(name1, name2, MAX_PATH))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
                return FALSE;
            }
        }
    }

    if (!bName)
    { // nastavime defaultni jmeno
        strcpy(name1, companionFile);
        name1[strlen(name1) - 1] = 0;
        char* dot = _tcsrchr(name1, '.');
        if (dot == NULL) // ".cvspass" ve Windows je pripona
        {
            if (strlen(name1) + 4 >= MAX_PATH) // test pro strcat() nize
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
                return FALSE;
            }
            strcat(name1, ".EXT");
        }
    }

    if (configCombineToOther)
    { // JC - unor 2002 - uprava pro kombinovani do druhyho panelu, sorry, trochu prasarna
        // ale predchozi kod s tim nepocital, takze bych ho musel cely prepsat...
        // Tento kod nahradi cestu v "name1", ktera je do zdrojoveho panelu, cestou ciloveho panelu
        SalamanderGeneral->SalPathStripPath(name1);
        GetTargetDir(name2, NULL, FALSE);
        if (!SalamanderGeneral->SalPathAppend(name2, name1, MAX_PATH))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
            return FALSE;
        }
        strcpy(name1, name2);
    }

    if (!CombineDialog(files, name1, bCrc, origCrc, parent, salamander))
        return FALSE;

    GetTargetDir(sourceDir, NULL, FALSE);
    if (!MakePathAbsolute(name1, FALSE, sourceDir, !configCombineToOther, IDS_COMBINE))
        return FALSE;

    return CombineFiles(files, name1, FALSE, bCrc, origCrc, bTime, &origTime, parent, salamander);
}
