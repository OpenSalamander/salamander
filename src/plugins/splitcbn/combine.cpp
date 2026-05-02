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

    // sum sizes of all partial files (while simultaneously checking their accessibility)
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

    // check available free space
    if (!bOnlyCrc)
    {
        char dir[MAX_PATH];
        strncpy_s(dir, targetName, _TRUNCATE);
        SalamanderGeneral->CutDirectory(dir);
        if (!SalamanderGeneral->TestFreeSpace(parent, dir, totalSize, LoadStr(IDS_COMBINE)))
            return FALSE;
    }

    // create the output file
    SAFE_FILE outfile;
    if (!bOnlyCrc)
    {
        if (SalamanderSafeFile->SafeFileCreate(targetName, GENERIC_WRITE, FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL,
                                               FALSE, parent, NULL, NULL, NULL, FALSE, NULL, NULL, 0, NULL, &outfile) == INVALID_HANDLE_VALUE)
        {
            return FALSE;
        }
    }

    // merge the files
    char* pBuffer = new char[BUFSIZE];
    if (pBuffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_OUTOFMEM), LoadStr(idTitle), MSGBOX_ERROR);
        if (!bOnlyCrc)
            SalamanderSafeFile->SafeFileClose(&outfile);
        return FALSE;
    }

    UINT32 CrcVal = 0;

    // open the progress dialog
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
    // load the file into a buffer
    HANDLE hFile;
    if ((hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, NULL)) == INVALID_HANDLE_VALUE)
        return;
    DWORD size = GetFileSize(hFile, NULL), numread;
    // j.r. 21.1.2003: when splitting to 999 parts I received a batch of 30KB
    //  if (size > 10000) { CloseHandle(hFile); return; } // skip such large files
    if (size > 200000)
    {
        CloseHandle(hFile);
        return;
    } // skip such large files
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
    // try to find "crc32" or "crc"
    if (!bCrcAcquired)
    {
        bCrcAcquired = FindCrc(text_locase, "crc32", origCrc);
        if (!bCrcAcquired)
            bCrcAcquired = FindCrc(text_locase, "crc", origCrc);
    }
    // "filename" or "name"
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
    { // files are selected
        int index = 0;
        BOOL bAllSameNames = TRUE;
        BOOL bFirst = TRUE;

        // load selected items into the array (except directories)
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

        // if all names were identical, there is a chance we will find a companion .BAT or .CRC
        bTestCompanionFile = bAllSameNames;
        if (!bAllSameNames)
            strcpy(name1, "combinedfile");
        strcpy(companionFile, sourceDir);
        if (!SalamanderGeneral->SalPathAppend(companionFile, name1, MAX_PATH) ||
            strlen(companionFile) + 1 >= MAX_PATH) // safety check for the strcat() below
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
            return FALSE;
        }
        strcat(companionFile, ".");
    }
    else // only the focus is on a file - try to extend the selection with "higher" files
    {
        pfd = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
        if (pfd == NULL || isDir)
        {
            TRACE_E("CombineCommand(): No focus on a file?!?");
            return FALSE;
        }
        BOOL bJustOneFile = FALSE;
        // first analyze the extension
        char* ext = _tcsrchr(pfd->Name, '.');
        if (ext != NULL) // ".cvspass" is an extension in Windows
        {
            BOOL bZeroPadded, bAddThisFile = TRUE;
            int nextIndex;
            if (!lstrcmpi(ext, ".tns"))
            { // tns = Turbo Navigator Split - consider it as "000"
                bZeroPadded = FALSE;
                nextIndex = 2;
            }
            else if (!lstrcmpi(ext, ".bat") || !lstrcmpi(ext, ".crc"))
            { // Salamander's BAT or WinCommander CRC; this will not work with TN files here
                bZeroPadded = TRUE;
                nextIndex = 1;
                bAddThisFile = FALSE;
            }
            else
            { // is the extension composed of digits?
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
                    bJustOneFile = TRUE; // strange extension - we will not extend anything
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
        { // missing extension - skip it, we will not extend anything
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
    { // inspect a potential BAT or CRC
        size_t ext = strlen(companionFile);
        if (ext + 3 >= MAX_PATH) // safety check for the strcat() below
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
    { // set a default name
        strcpy(name1, companionFile);
        name1[strlen(name1) - 1] = 0;
        char* dot = _tcsrchr(name1, '.');
        if (dot == NULL) // ".cvspass" is an extension in Windows
        {
            if (strlen(name1) + 4 >= MAX_PATH) // safety check for the strcat() below
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE), MSGBOX_ERROR);
                return FALSE;
            }
            strcat(name1, ".EXT");
        }
    }

    if (configCombineToOther)
    { // JC - February 2002 - adjustment for combining to the other panel, sorry, a bit of a hack
        // but the previous code did not anticipate it, so I would have had to rewrite it all...
        // This code replaces the path in "name1", which points to the source panel, with the target panel path
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
