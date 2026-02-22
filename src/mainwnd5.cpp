// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "cfgdlg.h"
#include "dialogs.h"

void GetFileDateAndTimeFromPanel(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData,
                                 const CFileData* f, BOOL isDir, SYSTEMTIME* st, BOOL* validDate,
                                 BOOL* validTime)
{
    *validDate = FALSE;
    *validTime = FALSE;
    FILETIME ft;
    if (validFileData & (VALID_DATA_DATE | VALID_DATA_TIME)) // at least something is in LastWrite
    {
        if (FileTimeToLocalFileTime(&f->LastWrite, &ft) &&
            FileTimeToSystemTime(&ft, st))
        {
            if (validFileData & VALID_DATA_DATE)
                *validDate = TRUE;
            if (validFileData & VALID_DATA_TIME)
                *validTime = TRUE;
        }
    }
    if ((validFileData & VALID_DATA_PL_DATE) &&
        pluginData->NotEmpty() &&
        pluginData->GetLastWriteDate(f, isDir, st))
    {
        *validDate = TRUE;
    }
    if ((validFileData & VALID_DATA_PL_TIME) &&
        pluginData->NotEmpty() &&
        pluginData->GetLastWriteTime(f, isDir, st))
    {
        *validTime = TRUE;
    }
    if (!*validDate) // no date set, reset it...
    {
        st->wYear = 1602;
        st->wMonth = 1;
        st->wDay = 1;
        st->wDayOfWeek = 2;
    }
    if (!*validTime) // no time set, reset it...
    {
        st->wHour = 0;
        st->wMinute = 0;
        st->wSecond = 0;
        st->wMilliseconds = 0;
    }
}

int MyCompareFileTime(FILETIME* lf, FILETIME* rf, int* foundDSTShifts, int* compResNoDSTShiftIgn)
{
    int res = *compResNoDSTShiftIgn = CompareFileTime(lf, rf);
    if (res != 0)
    {
        CQuadWord left(lf->dwLowDateTime, lf->dwHighDateTime);
        CQuadWord right(rf->dwLowDateTime, rf->dwHighDateTime);
        // compute the difference between the times and its rounding
        unsigned __int64 diff = (right.Value > left.Value) ? right.Value - left.Value : left.Value - right.Value;
        if (diff == (unsigned __int64)3600 * 10000000 ||
            diff == (unsigned __int64)2 * 3600 * 10000000)
        {
            (*foundDSTShifts)++;
            if (Configuration.IgnoreDSTShifts)
                res = 0;
        }
    }
    return res;
}

// compare two files 'l' and 'r' by date/time
int CompareFilesByTime(CFilesWindow* leftPanel, const CFileData* l, BOOL lFAT,
                       CFilesWindow* rightPanel, CFileData* r, BOOL rFAT,
                       int* foundDSTShifts, int* compResNoDSTShiftIgn)
{
    SLOW_CALL_STACK_MESSAGE3("CompareFilesByTime(, , %d, , , %d, ,)", lFAT, rFAT);
    int res = *compResNoDSTShiftIgn = 0;

    SYSTEMTIME stLeft;
    BOOL validDateLeft, validTimeLeft;
    GetFileDateAndTimeFromPanel(leftPanel->ValidFileData, &leftPanel->PluginData, l, FALSE, &stLeft,
                                &validDateLeft, &validTimeLeft);
    SYSTEMTIME stRight;
    BOOL validDateRight, validTimeRight;
    GetFileDateAndTimeFromPanel(rightPanel->ValidFileData, &rightPanel->PluginData, r, FALSE, &stRight,
                                &validDateRight, &validTimeRight);

    if (validDateLeft && validDateRight ||                                    // if the time is unknown it is initialized to 0:00:00.000, so we ignore time...
        !validDateLeft && !validDateRight && validTimeLeft && validTimeRight) // date is unknown (zeroed), times are both known, so nothing prevents comparison
    {
        FILETIME lf, rf;
        if (Configuration.UseTimeResolution)
        {
            // trim time with second precision
            stLeft.wMilliseconds = 0;
            stRight.wMilliseconds = 0;
            // convert time to numbers
            if (!SystemTimeToFileTime(&stLeft, &lf))
            {
                TRACE_E("CompareFilesByTime(): date&time of left file is invalid!");
                *compResNoDSTShiftIgn = res = -1;
            }
            else
            {
                if (!SystemTimeToFileTime(&stRight, &rf))
                {
                    TRACE_E("CompareFilesByTime(): date&time of right file is invalid!");
                    *compResNoDSTShiftIgn = res = 1;
                }
                else
                {
                    CQuadWord left(lf.dwLowDateTime, lf.dwHighDateTime);
                    CQuadWord right(rf.dwLowDateTime, rf.dwHighDateTime);
                    unsigned __int64 ld = left.Value / 10000000;
                    unsigned __int64 rd = right.Value / 10000000;
                    // compute the difference between the times and its rounding
                    unsigned __int64 diff = (rd > ld) ? rd - ld : ld - rd;
                    *compResNoDSTShiftIgn = res = (left < right) ? -1 : 1;
                    if (diff < 100000)
                    {
                        int d = (int)diff;
                        if (d <= Configuration.TimeResolution)
                            *compResNoDSTShiftIgn = res = 0;
                        else
                        {
                            int dIgnOneHour = d < 3600 ? 3600 - d : d - 3600;
                            int dIgnTwoHours = d < 2 * 3600 ? 2 * 3600 - d : d - 2 * 3600;
                            if (dIgnOneHour <= Configuration.TimeResolution || dIgnTwoHours <= Configuration.TimeResolution)
                            {
                                (*foundDSTShifts)++;
                                if (Configuration.IgnoreDSTShifts)
                                    res = 0;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (!SystemTimeToFileTime(&stLeft, &lf))
            {
                TRACE_E("CompareFilesByTime(): date&time of left file is invalid!");
                *compResNoDSTShiftIgn = res = -1;
            }
            else
            {
                if (!SystemTimeToFileTime(&stRight, &rf))
                {
                    TRACE_E("CompareFilesByTime(): date&time of right file is invalid!");
                    *compResNoDSTShiftIgn = res = 1;
                }
                else
                {
                    if (lFAT == rFAT)
                        res = MyCompareFileTime(&lf, &rf, foundDSTShifts, compResNoDSTShiftIgn); // same filesystems -> OK
                    else                                                                         // FAT plus a different filesystem -> adjust the non-FAT time to FAT time (by 2 seconds)
                    {
                        WORD date, time; // FAT values
                        FILETIME t;
                        if (lFAT)
                        {
                            FileTimeToDosDateTime(&rf, &date, &time);
                            DosDateTimeToFileTime(date, time, &t);
                            res = MyCompareFileTime(&lf, &t, foundDSTShifts, compResNoDSTShiftIgn);
                        }
                        else
                        {
                            FileTimeToDosDateTime(&lf, &date, &time);
                            DosDateTimeToFileTime(date, time, &t);
                            res = MyCompareFileTime(&t, &rf, foundDSTShifts, compResNoDSTShiftIgn);
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (validDateLeft)
            res = 1; // any date is newer than an unknown date
        else
        {
            if (validDateRight)
                res = -1; // an unknown date is older than any date
            else          // neither date is known
            {
                if (validTimeLeft)
                    res = 1; // any time is newer than an unknown time
                else
                {
                    if (validTimeRight)
                        res = -1; // an unknown time is older than any time
                }
            }
        }
        *compResNoDSTShiftIgn = res;
    }
    return res;
}

void GetFileSizeFromPanel(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData,
                          const CFileData* f, BOOL isDir, CQuadWord* size, BOOL* validSize)
{
    *validSize = FALSE;
    if (validFileData & VALID_DATA_SIZE)
    {
        *validSize = TRUE;
        *size = f->Size;
    }
    else
    {
        if ((validFileData & VALID_DATA_PL_SIZE) &&
            pluginData->NotEmpty() &&
            pluginData->GetByteSize(f, isDir, size))
        {
            *validSize = TRUE;
        }
    }
}

// it compares two files 'l' and 'r' by size
int CompareFilesBySize(CFilesWindow* leftPanel, CFileData* l, CFilesWindow* rightPanel, CFileData* r)
{
    CALL_STACK_MESSAGE_NONE

    BOOL leftValidSize;
    CQuadWord leftSize;
    GetFileSizeFromPanel(leftPanel->ValidFileData, &leftPanel->PluginData, l, FALSE, &leftSize, &leftValidSize);
    BOOL rightValidSize;
    CQuadWord rightSize;
    GetFileSizeFromPanel(rightPanel->ValidFileData, &rightPanel->PluginData, r, FALSE, &rightSize, &rightValidSize);

    if (leftValidSize && rightValidSize)
    {
        if (leftSize != rightSize)
            return (leftSize > rightSize) ? 1 : -1;
    }
    else
    {
        if (leftValidSize)
            return 1; // anything is larger than an unknown size
        else
        {
            if (rightValidSize)
                return -1; // an unknown size is smaller than anything
        }
    }
    return 0;
}

// compares files 'file1' and 'file2' specified by full path by their content
// on successful comparison, it returns TRUE and sets 'different'
// variable (TRUE if a difference was found, otherwise FALSE)
// on error or user abort, returns FALSE and sets 'canceled' variable
// (TRUE if the user canceled the operation, otherwise FALSE)

#define COMPARE_BUFFER_SIZE (2 * 1024 * 1024) // buffer size for comparison in bytes (does not necessarily need to be fully used)
#define COMPARE_BLOCK_SIZE (32 * 1024)        // size of a block read continuously from the file; NOTE: COMPARE_BUFFER_SIZE must be divisible by COMPARE_BLOCK_SIZE
#define COMPARE_BLOCK_GROUP 8                 // how many blocks can be read at once when reading is fast enough (more than 1 MB/s, see COMPARE_BUF_TIME_LIMIT); NOTE: the number of blocks in the buffer (COMPARE_BUFFER_SIZE / COMPARE_BLOCK_SIZE) must be divisible by COMPARE_BLOCK_GROUP
#define COMPARE_BUF_TIME_LIMIT 2000           // time limit in milliseconds for reading the entire buffer (COMPARE_BUFFER_SIZE) - if met, blocks are read in groups COMPARE_BLOCK_GROUP which speeds up network reading on Vista+ 2-3x; otherwise reading is done block by block (COMPARE_BLOCK_SIZE)

void AddProgressSizeWithLimit(CCmpDirProgressDialog* progressDlg, DWORD read, CQuadWord* fileProgressTotal, const CQuadWord& sizeLimit)
{
    if (*fileProgressTotal + CQuadWord(read, 0) > sizeLimit)
        read = (DWORD)(sizeLimit - *fileProgressTotal).Value;
    if (read > 0)
    {
        progressDlg->AddSize(CQuadWord(read, 0));
        *fileProgressTotal += CQuadWord(read, 0);
    }
}

BOOL CompareFilesByContent(HWND hWindow, CCmpDirProgressDialog* progressDlg,
                           const char* file1, const char* file2, const CQuadWord& bothFileSize,
                           BOOL* different, BOOL* canceled)
{
    char message[2 * MAX_PATH + 200]; // we need 2*MAX_PATH for the path plus room for an error message
    BOOL ret = FALSE;
    *canceled = FALSE;

    //  DWORD totalTi = GetTickCount();

    HANDLE hFile1 = HANDLES_Q(CreateFile(file1, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    HANDLE hFile2 = hFile1 != INVALID_HANDLE_VALUE ? HANDLES_Q(CreateFile(file2, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                                          NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL))
                                                   : INVALID_HANDLE_VALUE;
    DWORD err = GetLastError();

    if (!progressDlg->Continue()) // give the dialog a chance to repaint at 100% of the previous file (opening a file is the longest step (3ms) when copying small files (0.1ms))
        *canceled = TRUE;

    // set texts in the progress dialog
    progressDlg->SetSource(file1);
    progressDlg->SetTarget(file2);

    // set the total ('bothFileSize') and current (0%) file progress
    CQuadWord fileProgressTotal(0, 0);
    progressDlg->SetFileSize(bothFileSize);
    progressDlg->SetActualFileSize(CQuadWord(0, 0));

    if (hFile1 != INVALID_HANDLE_VALUE)
    {
        if (hFile2 != INVALID_HANDLE_VALUE)
        {
            if (!*canceled)
            {
                char* buffer1 = (char*)malloc(COMPARE_BUFFER_SIZE);
                //        char *buffer2 = (char *)malloc(COMPARE_BUFFER_SIZE);
                char buffer2[COMPARE_BLOCK_GROUP * COMPARE_BLOCK_SIZE];

                DWORD read1;
                DWORD read2;

                //        DWORD measureStart = GetTickCount();
                //        unsigned __int64 measuredSize = 0;
                BOOL readErr = FALSE;
                BOOL readingIsFast1 = FALSE; // FALSE = wait until speed reaches the limit and we can read in groups (COMPARE_BLOCK_GROUP)
                BOOL readingIsFast2 = FALSE;
                while (TRUE)
                {
                    DWORD bufSize = COMPARE_BUFFER_SIZE;
                    int blockCount = bufSize / COMPARE_BLOCK_SIZE; // number of blocks in bufSize

                    // load the next part from 'file1' into 'buffer1'
                    // read the entire buffer in 32KB blocks from one file (on W2K and XP this is fastest when both files are on one physical disk; on Vista it's only slightly slower than sequential reading)
                    read1 = 0;
                    DWORD readBegTime = GetTickCount();
                    for (int block = 0; block < blockCount;)
                    {
                        DWORD locRead;
                        BOOL canReadGroup = readingIsFast1 && (block % COMPARE_BLOCK_GROUP) == 0 && block + COMPARE_BLOCK_GROUP <= blockCount;
                        DWORD readBlockSize = canReadGroup ? COMPARE_BLOCK_GROUP * COMPARE_BLOCK_SIZE : COMPARE_BLOCK_SIZE;
                        if (!ReadFile(hFile1, buffer1 + block * COMPARE_BLOCK_SIZE, readBlockSize, &locRead, NULL))
                        {
                            err = GetLastError();
                            readErr = TRUE;
                            _snprintf_s(message, _TRUNCATE, LoadStr(IDS_ERROR_READING_FILE), file1, GetErrorText(err));
                            progressDlg->FlushDataToControls();
                            if (SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                            {
                                *canceled = TRUE;
                            }
                            break;
                        }

                        AddProgressSizeWithLimit(progressDlg, locRead, &fileProgressTotal, bothFileSize);
                        if (!progressDlg->Continue()) // give the dialog a chance to repaint
                        {
                            *canceled = TRUE;
                            break;
                        }

                        read1 += locRead;
                        if (locRead != readBlockSize)
                            break; // EOF
                        if (GetTickCount() - readBegTime > 200)
                        { // slow reading (probably a slow network disk (VPN) or floppy) - reduce the buffer, at worst we'll compare block by block
                            blockCount = block + (canReadGroup ? COMPARE_BLOCK_GROUP : 1);
                            bufSize = blockCount * COMPARE_BLOCK_SIZE;
                            break;
                        }
                        block += canReadGroup ? COMPARE_BLOCK_GROUP : 1;
                    }
                    if (readErr || *canceled)
                        break;
                    readingIsFast1 = WindowsVistaAndLater &&                                                                                           // on W2K/XP this should not speed things up, so we will not tempt fate
                                     GetTickCount() - readBegTime < (DWORD)(((unsigned __int64)read1 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE); // measure the speed so it is over 1 MB/s
                                                                                                                                                       /*
          // read the entire buffer at once from one file (on Vista, when both files are on the same physical disk and the buffer is large, this is slightly faster than reading in 32KB blocks)
          if (!ReadFile(hFile1, buffer1, bufSize, &read1, NULL))
          {
            err = GetLastError();
            sprintf(message, LoadStr(IDS_ERROR_READING_FILE), file1, GetErrorText(err));
            progressDlg->FlushDataToControls();
            if (SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
              *canceled = TRUE;
            }
            break;
          }
*/

                    // load the next part from 'file2' into 'buffer2'
                    // read the whole buffer in blocks from one file (on W2K and XP this is fastest when both files are on one physical disk; on Vista it's only slightly slower than sequential reading)
                    read2 = 0;
                    readBegTime = GetTickCount();
                    for (int block = 0; block < blockCount;)
                    {
                        DWORD locRead;
                        BOOL canReadGroup = readingIsFast2 && (block % COMPARE_BLOCK_GROUP) == 0 && block + COMPARE_BLOCK_GROUP <= blockCount;
                        DWORD readBlockSize = canReadGroup ? COMPARE_BLOCK_GROUP * COMPARE_BLOCK_SIZE : COMPARE_BLOCK_SIZE;
                        if (!ReadFile(hFile2, buffer2, readBlockSize, &locRead, NULL))
                        {
                            err = GetLastError();
                            readErr = TRUE;
                            _snprintf_s(message, _TRUNCATE, LoadStr(IDS_ERROR_READING_FILE), file2, GetErrorText(err));
                            progressDlg->FlushDataToControls();
                            if (SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                            {
                                *canceled = TRUE;
                            }
                            break;
                        }

                        AddProgressSizeWithLimit(progressDlg, locRead, &fileProgressTotal, bothFileSize);
                        if (!progressDlg->Continue()) // give the dialog a chance to repaint
                        {
                            *canceled = TRUE;
                            break;
                        }

                        read2 += locRead;
                        if (read1 < read2 || // files are now of different length => content differs
                            locRead > 0 && memcmp(buffer1 + block * COMPARE_BLOCK_SIZE, buffer2, locRead) != 0)
                        { // file contents differ, no point in continuing reading
                            *different = TRUE;
                            ret = TRUE;
                            break;
                        }
                        if (locRead != readBlockSize)
                        { // unable to read the entire buffer (EOF), files are identical
                            *different = FALSE;
                            ret = TRUE;
                            break;
                        }
                        block += canReadGroup ? COMPARE_BLOCK_GROUP : 1;
                        if (readingIsFast2 &&
                            GetTickCount() - readBegTime > (DWORD)(((unsigned __int64)read2 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE))
                        {
                            // detecting slowdown, continue block by block to keep progress smooth and allow canceling
                            readingIsFast2 = FALSE;
                        }
                    }
                    if (readErr || ret || *canceled)
                        break;
                    readingIsFast2 = WindowsVistaAndLater &&                                                                                           // on W2K/XP this should not speed things up, so we will not tempt fate
                                     GetTickCount() - readBegTime < (DWORD)(((unsigned __int64)read2 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE); // measure the speed so it is over 1 MB/s
                                                                                                                                                       /*
          // read the entire buffer at once from one file (on Vista, when both files are on the same physical disk and the buffer is large, this is slightly faster than reading in 32KB blocks)
          if (!ReadFile(hFile2, buffer2, bufSize, &read2, NULL))
          {
            err = GetLastError();
            sprintf(message, LoadStr(IDS_ERROR_READING_FILE), file2, GetErrorText(err));
            progressDlg->FlushDataToControls();
            if (SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
              *canceled = TRUE;
            }
            break;
          }
*/
                                                                                                                                                       /*
          // these tests are performed above when reading in blocks
          AddProgressSizeWithLimit(progressDlg, read1 + read2, &fileProgressTotal, bothFileSize);
          if (!progressDlg->Continue()) // give the dialog a chance to repaint
          {
            *canceled = TRUE;
            break;
          }

          if (read1 != read2 ||  // files are now of different length => content differs
              read1 > 0 && memcmp(buffer1, buffer2, read1) != 0)
          {  // file contents differ, no point in continuing reading
            *different = TRUE;
            ret = TRUE;
            break;
          }
          if (read1 != bufSize)
          { // unable to read the entire buffer, files are identical
            *different = FALSE;
            ret = TRUE;
            break;
          }
*/
                                                                                                                                                       /*
          measuredSize += bufSize;
          DWORD ti = GetTickCount() - measureStart;
          if (ti >= 500)
          {
            unsigned __int64 speed = (measuredSize * 1000) / ti;
            TRACE_I("speed: " << (DWORD)(speed / 1024));
            measureStart = GetTickCount();
            measuredSize = 0;
          }
*/
                }
                free(buffer1);
                //        free(buffer2);
            }
            HANDLES(CloseHandle(hFile2));
        }
        else
        {
            _snprintf_s(message, _TRUNCATE, LoadStr(IDS_ERROR_OPENING_FILE), file2, GetErrorText(err));
            progressDlg->FlushDataToControls();
            if (SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
                *canceled = TRUE;
            }
        }
        HANDLES(CloseHandle(hFile1));
    }
    else
    {
        _snprintf_s(message, _TRUNCATE, LoadStr(IDS_ERROR_OPENING_FILE), file1, GetErrorText(err));
        progressDlg->FlushDataToControls();
        if (SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                          MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
        {
            *canceled = TRUE;
        }
    }
    if (!*canceled) // we must advance the total progress to the end of the file
        progressDlg->AddSize(bothFileSize - fileProgressTotal);

    //  TRACE_I("CompareFilesByContent(): total time (in secs): " << ((GetTickCount() - totalTi) / 1000));

    return ret;
}

// loads directories and files into the arrays 'dirs' and 'files'
// the source path is determined by combining the path in the 'panel' with 'subPath'
// 'hWindow' is the window for displaying message boxes
// 'progressDlg' is used to update progress after loading X files/directories
// returns TRUE if both arrays were successfully filled
// returns FALSE on error or if the user canceled the operation
// (the variable 'canceled' will then be set)
//
// supports ptDisk and ptZIPArchive

BOOL ReadDirsAndFilesAux(HWND hWindow, DWORD flags, CCmpDirProgressDialog* progressDlg,
                         CFilesWindow* panel, const char* subPath,
                         CFilesArray* dirs, CFilesArray* files, BOOL* canceled, BOOL getTotal)
{
    char message[2 * MAX_PATH + 200];
    char path[MAX_PATH];

    BOOL ignFileNames = (flags & COMPARE_DIRECTORIES_IGNFILENAMES) != 0;
    BOOL ignDirNames = (flags & COMPARE_DIRECTORIES_IGNDIRNAMES) != 0;

    if (panel->Is(ptDisk))
    {
        BOOL pathAppended = TRUE;
        lstrcpyn(path, panel->GetPath(), MAX_PATH);
        pathAppended &= SalPathAppend(path, subPath, MAX_PATH);
        pathAppended &= SalPathAppend(path, "*", MAX_PATH);
        if (!pathAppended)
        {
            SalMessageBox(hWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONEXCLAMATION);
            *canceled = TRUE;
            return FALSE;
        }

        DWORD counter = 0;
        WIN32_FIND_DATA data;
        HANDLE hFind;
        hFind = HANDLES_Q(FindFirstFile(path, &data));
        if (hFind == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
            {
                if (getTotal)
                    *canceled = FALSE; // we're only obtaining the size, no need to bother the user, skip the error
                else
                {
                    _snprintf_s(message, _TRUNCATE, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
                    progressDlg->FlushDataToControls();
                    *canceled = SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL;
                }
                return FALSE;
            }
            return TRUE;
        }
        do
        {
            if (data.cFileName[0] != 0 &&
                (data.cFileName[0] != '.' ||
                 (data.cFileName[1] != 0 && (data.cFileName[1] != '.' || data.cFileName[2] != 0))))
            {
                if (counter++ > 200) // after reading 200 items
                {
                    if (!progressDlg->Continue()) // give the dialog a chance to repaint
                    {
                        HANDLES(FindClose(hFind));
                        *canceled = TRUE;
                        return FALSE;
                    }
                    counter = 0;
                }

                CFileData file;
                // initialize structure members we won't modify further
                file.DosName = NULL;
                file.PluginData = -1;
                file.Association = 0;
                file.Selected = 0;
                file.Shared = 0;
                file.Archive = 0;
                file.SizeValid = 0;
                file.Dirty = 0;
                file.CutToClip = 0;
                file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
                file.IconOverlayDone = 0;
                file.Hidden = 0;
                file.IsLink = 0;
                file.IsOffline = 0;

                int nameLen = (int)strlen(data.cFileName);

                //--- name
                file.Name = (char*)malloc(nameLen + 1); // allocation
                if (file.Name == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    HANDLES(FindClose(hFind));
                    *canceled = TRUE;
                    return FALSE;
                }
                memmove(file.Name, data.cFileName, nameLen + 1); // copy text
                file.NameLen = nameLen;

                //--- extension
                if (!Configuration.SortDirsByExt && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // this is ptDisk
                {
                    file.Ext = file.Name + file.NameLen; // directories have no extensions
                }
                else
                {
                    const char* s = data.cFileName + nameLen;
                    while (--s >= data.cFileName && *s != '.')
                        ;
                    //          if (s > data.cFileName) file.Ext = file.Name + (s - data.cFileName + 1); // ".cvspass" in Windows counts as an extension ...
                    if (s >= data.cFileName)
                        file.Ext = file.Name + (s - data.cFileName + 1);
                    else
                        file.Ext = file.Name + file.NameLen;
                }

                //--- other fields
                file.Size = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh);
                file.Attr = data.dwFileAttributes;
                file.LastWrite = data.ftLastWriteTime;

                if (file.Attr & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!ignDirNames || !Configuration.CompareIgnoreDirsMasks.AgreeMasks(file.Name, NULL))
                    {
                        dirs->Add(file);
                        if (!dirs->IsGood())
                        {
                            TRACE_E(LOW_MEMORY);
                            free(file.Name);
                            HANDLES(FindClose(hFind));
                            *canceled = TRUE;
                            return FALSE;
                        }
                    }
                    else
                        free(file.Name);
                }
                else
                {
                    if (!ignFileNames || !Configuration.CompareIgnoreFilesMasks.AgreeMasks(file.Name, file.Ext))
                    {
                        files->Add(file);
                        if (!files->IsGood())
                        {
                            TRACE_E(LOW_MEMORY);
                            free(file.Name);
                            HANDLES(FindClose(hFind));
                            *canceled = TRUE;
                            return FALSE;
                        }
                    }
                    else
                        free(file.Name);
                }
            }
        } while (FindNextFile(hFind, &data));
        DWORD err = GetLastError();
        if (err != ERROR_NO_MORE_FILES)
        {
            if (getTotal)
                *canceled = FALSE; // we're only obtaining the size, no need to bother the user, skip the error
            else
            {
                _snprintf_s(message, _TRUNCATE, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
                progressDlg->FlushDataToControls();
                *canceled = SalMessageBox(hWindow, message, LoadStr(IDS_ERRORTITLE),
                                          MB_OK | MB_ICONEXCLAMATION) == IDCANCEL;
            }
            HANDLES(FindClose(hFind));
            return FALSE;
        }

        HANDLES(FindClose(hFind));
    }
    else if (panel->Is(ptZIPArchive))
    {
        CFilesArray* zipDirs = panel->GetArchiveDirDirs(subPath);
        CFilesArray* zipFiles = panel->GetArchiveDirFiles(subPath);

        if (zipFiles == NULL || zipDirs == NULL)
        {
            *canceled = TRUE;
            return FALSE; // low memory, bail out
        }

        files->SetDeleteData(FALSE); // shallow data copies only
        dirs->SetDeleteData(FALSE);  // shallow data copies only

        int i;
        for (i = 0; i < zipDirs->Count; i++)
        {
            CFileData* f = &zipDirs->At(i);
            if (!ignDirNames || !Configuration.CompareIgnoreDirsMasks.AgreeMasks(f->Name, NULL))
            {
                dirs->Add(*f);
                if (!dirs->IsGood())
                {
                    TRACE_E(LOW_MEMORY);
                    *canceled = TRUE;
                    return FALSE; // low memory, bail out
                }
            }
        }

        for (i = 0; i < zipFiles->Count; i++)
        {
            CFileData* f = &zipFiles->At(i);
            if (!ignFileNames || !Configuration.CompareIgnoreFilesMasks.AgreeMasks(f->Name, f->Ext))
            {
                files->Add(*f);
                if (!files->IsGood())
                {
                    TRACE_E(LOW_MEMORY);
                    *canceled = TRUE;
                    return FALSE; // low memory, bail out
                }
            }
        }
    }
    else
    {
        TRACE_E("not implemented");
        *canceled = TRUE;
        return FALSE;
    }

    return TRUE;
}

// recursive function searching for differences between directories
// directories are determined by the paths in the left and right panel
// and the variables 'leftSubDir' and 'rightSubDir'
// leftFAT and rightFAT indicate whether the respective panel is on a FAT system; if an archive is opened,
// the corresponding xxxFAT will be set to FALSE
// 'flags' specifies comparison criteria and comes from the COMPARE_DIRECTORIES_xxx family
// the function returns TRUE on successful comparison and sets 'different' (TRUE
// if the directories differ, otherwise FALSE).
// on error or user abort it returns FALSE
// and sets the variable 'canceled' (TRUE if aborted by the user, otherwise FALSE)

// supports ptDisk and ptZIPArchive

BOOL CompareDirsAux(HWND hWindow, CCmpDirProgressDialog* progressDlg,
                    CFilesWindow* leftPanel, const char* leftSubDir, BOOL leftFAT,
                    CFilesWindow* rightPanel, const char* rightSubDir, BOOL rightFAT,
                    DWORD flags, BOOL* different, BOOL* canceled,
                    BOOL getTotal, CQuadWord* total, int* foundDSTShifts)
{
    // left/rightPanel and left/rightSubDir specify the path
    // whose directories and files are stored in the arrays below
    CFilesArray leftDirs;
    CFilesArray rightDirs;
    int foundDSTShiftsInThisDir = 0;

    { // local block to limit the scope of leftFiles & rightFiles (saves memory before recursing into subdirectories)
        CFilesArray leftFiles;
        CFilesArray rightFiles;

        // from the previous file comparison, a value remained set
        if ((flags & COMPARE_DIRECTORIES_BYCONTENT) && (!getTotal))
            progressDlg->SetActualFileSize(CQuadWord(0, 0)); // set to 0%

        // set texts in the progress dialog
        BOOL pathAppended = TRUE;

        char message[MAX_PATH + 200];
        strcpy(message, leftPanel->GetPath());
        pathAppended &= SalPathAppend(message, leftSubDir, MAX_PATH + 200);
        progressDlg->SetSource(message);
        strcpy(message, rightPanel->GetPath());
        pathAppended &= SalPathAppend(message, rightSubDir, MAX_PATH + 200);
        progressDlg->SetTarget(message);

        if (!pathAppended)
        {
            SalMessageBox(hWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONEXCLAMATION);
            *canceled = TRUE;
            return FALSE;
        }

        if (!progressDlg->Continue())
        {
            *canceled = TRUE;
            return FALSE;
        }

        // load directories and files for the left side
        if (!ReadDirsAndFilesAux(hWindow, flags, progressDlg, leftPanel, leftSubDir, &leftDirs, &leftFiles, canceled, getTotal))
            return FALSE;

        // and the right panel
        if (!ReadDirsAndFilesAux(hWindow, flags, progressDlg, rightPanel, rightSubDir, &rightDirs, &rightFiles, canceled, getTotal))
            return FALSE;

        // if the counts differ in the left and right panel, the directories differ
        if (leftDirs.Count != rightDirs.Count || leftFiles.Count != rightFiles.Count)
        {
            *different = TRUE;
            return TRUE;
        }

        // at this point, the number of directories and files is the same in both panels

        // sort the arrays by name so we can compare them
        if (leftDirs.Count > 1)
            SortNameExt(leftDirs, 0, leftDirs.Count - 1, FALSE);
        if (leftFiles.Count > 1)
            SortNameExt(leftFiles, 0, leftFiles.Count - 1, FALSE);
        if (rightDirs.Count > 1)
            SortNameExt(rightDirs, 0, rightDirs.Count - 1, FALSE);
        if (rightFiles.Count > 1)
            SortNameExt(rightFiles, 0, rightFiles.Count - 1, FALSE);

        // first compare files by name, time and attributes
        // postpone comparing by content because it's slow and if
        // we find a difference at this level, we save time
        BOOL timeDiffWithDSTShiftExists = FALSE;
        CFileData *leftFile, *rightFile;
        int i;
        for (i = 0; i < leftFiles.Count; i++)
        {
            leftFile = &leftFiles[i];
            rightFile = &rightFiles[i];

            // By Name
            if (CmpNameExtIgnCase(*leftFile, *rightFile) != 0)
            {
                *different = TRUE;
                return TRUE;
            }

            // By Size
            if (flags & COMPARE_DIRECTORIES_BYSIZE)
            {
                if (CompareFilesBySize(leftPanel, leftFile, rightPanel, rightFile) != 0)
                {
                    *different = TRUE;
                    return TRUE;
                }
            }

            // By Attributes
            if (flags & COMPARE_DIRECTORIES_BYATTR)
            {
                if ((leftPanel->ValidFileData & VALID_DATA_ATTRIBUTES) &&
                    (rightPanel->ValidFileData & VALID_DATA_ATTRIBUTES))
                {
                    if ((leftFile->Attr & DISPLAYED_ATTRIBUTES) != (rightFile->Attr & DISPLAYED_ATTRIBUTES))
                    {
                        *different = TRUE;
                        return TRUE;
                    }
                }
            }

            // compare files by content -- here we only compare file sizes as a trivial test for mismatched content
            if (flags & COMPARE_DIRECTORIES_BYCONTENT)
            {
                if (leftFile->Size != rightFile->Size)
                {
                    if (!leftPanel->Is(ptDisk) || !rightPanel->Is(ptDisk))
                    {
                        TRACE_E("not implemented");
                        *canceled = FALSE;
                        return FALSE;
                    }
                    *different = TRUE;
                    return TRUE;
                }
            }

            // By Time -- because of DST time shifts we test timestamps last (we don't report a DST problem when files also differ in size or attributes)
            if (flags & COMPARE_DIRECTORIES_BYTIME)
            {
                int isDSTShift = 0; // 1 = the timestamps of the currently compared pair of files differ by exactly one or two hours, 0 = they don’t
                int compResNoDSTShiftIgn;
                if (CompareFilesByTime(leftPanel, leftFile, leftFAT, rightPanel, rightFile, rightFAT,
                                       &isDSTShift, &compResNoDSTShiftIgn) != 0)
                {
                    if (isDSTShift != 0)
                        timeDiffWithDSTShiftExists = TRUE; // try to find another difference (ideally without a DST warning)
                    else
                    {
                        *different = TRUE;
                        return TRUE;
                    }
                }
                else
                    foundDSTShiftsInThisDir += isDSTShift;
            }
        }

        // compare directories
        CFileData *leftDir, *rightDir;
        for (i = 0; i < leftDirs.Count; i++)
        {
            leftDir = &leftDirs[i];
            rightDir = &rightDirs[i];

            // By Name
            if (CmpNameExtIgnCase(*leftDir, *rightDir) != 0)
            {
                *different = TRUE;
                return TRUE;
            }

            // By Attributes
            if (flags & COMPARE_DIRECTORIES_SUBDIRS_ATTR)
            {
                if ((leftPanel->ValidFileData & VALID_DATA_ATTRIBUTES) &&
                    (rightPanel->ValidFileData & VALID_DATA_ATTRIBUTES))
                {
                    if ((leftDir->Attr & DISPLAYED_ATTRIBUTES) != (rightDir->Attr & DISPLAYED_ATTRIBUTES))
                    {
                        *different = TRUE;
                        return TRUE;
                    }
                }
            }

            // directories are not compared by time
        }

        if (timeDiffWithDSTShiftExists) // we found no other difference, so we report an unignored DST time shift including a warning
        {
            (*foundDSTShifts)++;
            *different = TRUE;
            return TRUE;
        }

        // compare files by content
        if (flags & COMPARE_DIRECTORIES_BYCONTENT)
        {
            if (!leftPanel->Is(ptDisk) || !rightPanel->Is(ptDisk))
            {
                TRACE_E("not implemented");
                *canceled = FALSE;
                return FALSE;
            }

            for (i = 0; i < leftFiles.Count; i++)
            {
                leftFile = &leftFiles[i];
                rightFile = &rightFiles[i];

                if (leftFile->Size == rightFile->Size)
                {
                    if (leftFile->Size != CQuadWord(0, 0))
                    {
                        if (!getTotal)
                        {
                            // build full paths to both files
                            pathAppended = TRUE;

                            char leftFilePath[2 * MAX_PATH];
                            strcpy(leftFilePath, leftPanel->GetPath());
                            pathAppended &= SalPathAppend(leftFilePath, leftSubDir, 2 * MAX_PATH);
                            pathAppended &= SalPathAppend(leftFilePath, leftFile->Name, 2 * MAX_PATH);

                            char rightFilePath[2 * MAX_PATH];
                            strcpy(rightFilePath, rightPanel->GetPath());
                            pathAppended &= SalPathAppend(rightFilePath, rightSubDir, 2 * MAX_PATH);
                            pathAppended &= SalPathAppend(rightFilePath, rightFile->Name, 2 * MAX_PATH);

                            if (!pathAppended)
                            {
                                SalMessageBox(hWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONEXCLAMATION);
                                *canceled = TRUE;
                                return FALSE;
                            }

                            if (!CompareFilesByContent(hWindow, progressDlg, leftFilePath, rightFilePath,
                                                       leftFile->Size + rightFile->Size, different, canceled))
                            {
                                return FALSE;
                            }

                            // found two different files, stop
                            if (*different)
                                return TRUE;
                        }
                        else
                            *total += leftFile->Size + rightFile->Size;
                    }
                }
                else
                {
                    // files have different length, they differ in content
                    *different = TRUE;
                    return TRUE;
                }
            }
        }

        // no difference found
        // we no longer need the files, so we can free the memory
        // leftFiles.DestroyMembers();  -- Petr: directly destroy the local object, frees more
        // rightFiles.DestroyMembers(); -- Petr: directly destroy the local object, frees more
    }

    // no difference found, recursively call ourselves on individual directories
    int i;
    for (i = 0; i < leftDirs.Count; i++)
    {
        char newLeftSubDir[MAX_PATH];
        char newRightSubDir[MAX_PATH];

        strcpy(newLeftSubDir, leftSubDir);
        BOOL pathAppended = TRUE;
        pathAppended &= SalPathAppend(newLeftSubDir, leftDirs[i].Name, MAX_PATH);
        strcpy(newRightSubDir, rightSubDir);
        pathAppended &= SalPathAppend(newRightSubDir, rightDirs[i].Name, MAX_PATH);
        if (!pathAppended)
        {
            SalMessageBox(hWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONEXCLAMATION);
            *canceled = TRUE;
            return FALSE;
        }

        int foundDSTShiftsInSubDir = 0;
        if (!CompareDirsAux(hWindow, progressDlg,
                            leftPanel, newLeftSubDir, leftFAT,
                            rightPanel, newRightSubDir, rightFAT,
                            flags, different, canceled, getTotal,
                            total, &foundDSTShiftsInSubDir))
        {
            return FALSE;
        }
        else
        {
            if (*different)
            {
                *foundDSTShifts += foundDSTShiftsInSubDir;
                return TRUE; // found a difference in a subdirectory, stop
            }
            foundDSTShiftsInThisDir += foundDSTShiftsInSubDir;
        }
    }

    // no difference found
    *different = FALSE;
    *foundDSTShifts += foundDSTShiftsInThisDir;
    return TRUE;
}

// create a shallow copy and set Selected to FALSE for all items
CFilesArray* GetShallowCopy(CFilesArray* items)
{
    CFilesArray* ret = new CFilesArray();
    if (ret != NULL)
    {
        ret->Add(items->GetData(), items->Count);
        if (ret->IsGood())
        {
            int i;
            for (i = 0; i < items->Count; i++)
                ret->At(i).Selected = 0;
            ret->SetDeleteData(FALSE); // shallow copy of data only, prevent destruction
        }
        else
        {
            TRACE_E(LOW_MEMORY);
            delete ret;
            ret = NULL;
        }
    }
    else
        TRACE_E(LOW_MEMORY);
    return ret;
}

void SkipIgnoredNames(BOOL ignoreNames, CMaskGroup* ignoreNamesMasks, BOOL dirs, int* l, CFileData** leftFile,
                      CFilesArray* left, int* r, CFileData** rightFile, CFilesArray* right)
{
    if (ignoreNames)
    {
        if (*l < left->Count)
        {
            while (ignoreNamesMasks->AgreeMasks((*leftFile)->Name, dirs ? NULL : (*leftFile)->Ext))
            { // skip ignored names in the left panel
                if (++(*l) < left->Count)
                    *leftFile = &left->At(*l);
                else
                    break;
            }
        }
        if (*r < right->Count)
        {
            while (ignoreNamesMasks->AgreeMasks((*rightFile)->Name, dirs ? NULL : (*rightFile)->Ext))
            { // skip ignored names in the right panel
                if (++(*r) < right->Count)
                    *rightFile = &right->At(*r);
                else
                    break;
            }
        }
    }
}

void CMainWindow::CompareDirectories(DWORD flags)
{
    CALL_STACK_MESSAGE2("CMainWindow::CompareDirectories(%u)", flags);
    if (flags & COMPARE_DIRECTORIES_BYCONTENT)
    {
        if (!LeftPanel->Is(ptDisk) || !RightPanel->Is(ptDisk))
        {
            TRACE_E("CMainWindow::CompareDirectories: Comparing by content is supported on ptDisk only.");
            return;
        }
    }
    if (flags & COMPARE_DIRECTORIES_SUBDIRS)
    {
        if (LeftPanel->Is(ptPluginFS) || RightPanel->Is(ptPluginFS))
        {
            TRACE_E("CMainWindow::CompareDirectories: Comparing including subdirectories is not supported on ptPluginFS.");
            return;
        }
    }
    BOOL ignFileNames = (flags & COMPARE_DIRECTORIES_IGNFILENAMES) != 0;
    if (ignFileNames)
    {
        int errPos;
        if (!Configuration.CompareIgnoreFilesMasks.PrepareMasks(errPos))
        {
            TRACE_E("CMainWindow::CompareDirectories: CompareIgnoreFilesMasks is invalid."); // likely only happens when configuration is manually edited in the registry
            return;
        }
    }
    BOOL ignDirNames = (flags & COMPARE_DIRECTORIES_IGNDIRNAMES) != 0;
    if (ignDirNames)
    {
        int errPos;
        if (!Configuration.CompareIgnoreDirsMasks.PrepareMasks(errPos))
        {
            TRACE_E("CMainWindow::CompareDirectories: CompareIgnoreDirsMasks is invalid."); // likely only happens when configuration is manually edited in the registry
            return;
        }
    }

    BeginStopRefresh(); // the snooper takes a break

    // lower the thread priority to "normal" (so the operation doesn't overburden the machine)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    //--- open the progress dialog
    BOOL displayDialogBox = (flags & COMPARE_DIRECTORIES_BYCONTENT) != 0 || // for quick actions there's no point in showing the dialog
                            (flags & COMPARE_DIRECTORIES_SUBDIRS) != 0;
    BOOL displayProgressBar = (flags & COMPARE_DIRECTORIES_BYCONTENT) != 0;
    CCmpDirProgressDialog progressDlg(HWindow, displayProgressBar, &TaskBarList3);

    HWND hFocusedWnd = NULL;
    if (displayDialogBox)
    {
        // set texts in the progress dialog
        char message[2 * MAX_PATH];
        LeftPanel->GetGeneralPath(message, 2 * MAX_PATH);
        progressDlg.SetSource(message);
        RightPanel->GetGeneralPath(message, 2 * MAX_PATH);
        progressDlg.SetTarget(message);

        hFocusedWnd = GetFocus();
        EnableWindow(HWindow, FALSE);
        progressDlg.Create();
    }

    BOOL identical = TRUE;
    BOOL canceled = FALSE;  // user canceled the operation
    int foundDSTShifts = 0; // how many times file timestamps differed by exactly one or two hours, with that being the only difference in the user-selected criteria

    //--- shallow copy of Files and Dirs arrays; (we must work on a copy of data because while the progress dialog is shown,
    //    a panel redraw would reveal that we changed panel sorting or moved the selection)

    CFilesArray* leftFiles = GetShallowCopy(LeftPanel->Files);
    CFilesArray* leftDirs = GetShallowCopy(LeftPanel->Dirs);
    CFilesArray* rightFiles = GetShallowCopy(RightPanel->Files);
    CFilesArray* rightDirs = GetShallowCopy(RightPanel->Dirs);

    if (leftFiles != NULL && leftDirs != NULL && rightFiles != NULL && rightDirs != NULL)
    {
        // shallow copies were created successfully
        //--- determine FAT file system because of lastWrite accuracy
        BOOL leftFAT = FALSE;
        BOOL rightFAT = FALSE;
        if (LeftPanel->Is(ptDisk))
        {
            char fileSystem[20];
            MyGetVolumeInformation(LeftPanel->GetPath(), NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, fileSystem, 20);
            leftFAT = StrNICmp(fileSystem, "FAT", 3) == 0; // FAT and FAT32 use DOS time (precision only 2 seconds)
        }
        if (RightPanel->Is(ptDisk))
        {
            char fileSystem[20];
            MyGetVolumeInformation(RightPanel->GetPath(), NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, fileSystem, 20);
            rightFAT = StrNICmp(fileSystem, "FAT", 3) == 0; // FAT and FAT32 use DOS time (precision only 2 seconds)
        }
        // stop loading icons to avoid competition with the comparison
        if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
            LeftPanel->SleepIconCacheThread();
        if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
            RightPanel->SleepIconCacheThread();
        //--- sort by name
        if (LeftPanel->SortType != stName || LeftPanel->ReverseSort)
            SortFilesAndDirectories(leftFiles, leftDirs, stName, FALSE, Configuration.SortDirsByName);
        if (RightPanel->SortType != stName || RightPanel->ReverseSort)
            SortFilesAndDirectories(rightFiles, rightDirs, stName, FALSE, Configuration.SortDirsByName);
        //--- comparison + marking
        BOOL getTotal;
        if (flags & COMPARE_DIRECTORIES_BYCONTENT)
            getTotal = TRUE; // TRUE: we don't mark differences, instead accumulate the size of files to compare into the 'total' variable
        else
            getTotal = FALSE;  // FALSE: we mark differences, compare files by content
        CQuadWord total(0, 0); // total number of bytes that may need to be compared in the worst case

        // the 'dirSubTotal' array contains sizes of individual subdirectories (files contained in them)
        // the array is filled in the first pass (getTotal == TRUE)
        // it is used in the second pass (getTotal == FALSE): if a subdirectory differs we know how much to skip on the total progress
        TDirectArray<CQuadWord> dirSubTotal(max(1, min(leftDirs->Count, rightDirs->Count)), 1);
        int subTotalIndex; // index into the dirSubTotal array

    ONCE_MORE:
        // first the files from the left and right directory
        CFilesArray *left = leftFiles, *right = rightFiles;

        CFileData *leftFile, *rightFile;
        int l = 0, r = 0, compRes;

        if (l < left->Count)
            leftFile = &left->At(l);
        if (r < right->Count)
            rightFile = &right->At(r);
        while (l < left->Count || r < right->Count)
        {
            SkipIgnoredNames(ignFileNames, &Configuration.CompareIgnoreFilesMasks, FALSE,
                             &l, &leftFile, left, &r, &rightFile, right);
            if (l < left->Count)
            {
                if (r < right->Count) // both panels contain files
                {
                    compRes = CmpNameExtIgnCase(*leftFile, *rightFile);
                    if (compRes == -1) // left < right
                    {
                        if (!getTotal)
                        {
                            leftFile->Selected = 1;
                            identical = FALSE;
                        }
                        if (++l < left->Count)
                            leftFile = &left->At(l);
                    }
                    else
                    {
                        if (compRes == 1) // left > right
                        {
                            if (!getTotal)
                            {
                                rightFile->Selected = 1;
                                identical = FALSE;
                            }
                            if (++r < right->Count)
                                rightFile = &right->At(r);
                        }
                        else // left == right
                        {
                            BOOL selectLeft = FALSE;
                            BOOL selectRight = FALSE;
                            BOOL leftIsNewer = FALSE;
                            BOOL rightIsNewer = FALSE;
                            BOOL leftIsNewerNoDSTShiftIgn = FALSE;
                            BOOL rightIsNewerNoDSTShiftIgn = FALSE;
                            int isDSTShift = 0; // 1 = times of the currently compared file pair differ by exactly one or two hours, 0 = they don't (or times are not compared at all)

                            // By Size
                            if (flags & COMPARE_DIRECTORIES_BYSIZE)
                            {
                                compRes = CompareFilesBySize(LeftPanel, leftFile, RightPanel, rightFile);
                                if (compRes == 1)
                                    selectLeft = TRUE;
                                if (compRes == -1)
                                    selectRight = TRUE;
                            }

                            // By Time
                            if (flags & COMPARE_DIRECTORIES_BYTIME)
                            {
                                int compResNoDSTShiftIgn;
                                compRes = CompareFilesByTime(LeftPanel, leftFile, leftFAT, RightPanel,
                                                             rightFile, rightFAT, &isDSTShift, &compResNoDSTShiftIgn);
                                if (compRes == 1)
                                    leftIsNewer = TRUE;
                                if (compRes == -1)
                                    rightIsNewer = TRUE;
                                if (compResNoDSTShiftIgn == 1)
                                    leftIsNewerNoDSTShiftIgn = TRUE;
                                if (compResNoDSTShiftIgn == -1)
                                    rightIsNewerNoDSTShiftIgn = TRUE;
                            }

                            // By Attributes
                            if (flags & COMPARE_DIRECTORIES_BYATTR)
                            {
                                if ((LeftPanel->ValidFileData & VALID_DATA_ATTRIBUTES) &&
                                    (RightPanel->ValidFileData & VALID_DATA_ATTRIBUTES))
                                {
                                    if ((leftFile->Attr & DISPLAYED_ATTRIBUTES) != (rightFile->Attr & DISPLAYED_ATTRIBUTES))
                                    {
                                        selectLeft = TRUE;
                                        selectRight = TRUE;
                                    }
                                }
                            }

                            // By Content
                            if (flags & COMPARE_DIRECTORIES_BYCONTENT)
                            {
                                if (leftFile->Size == rightFile->Size) // size test is intentionally not covered by the following optimization so both files are marked and unnecessary DST warnings don't pop up
                                {
                                    if (!selectLeft && !leftIsNewer || !selectRight && !rightIsNewer) // if both files are already flagged, there is no point in comparing them by content
                                    {
                                        // if both files are zero length, they must be identical in content
                                        if (leftFile->Size != CQuadWord(0, 0))
                                        {
                                            if (!getTotal)
                                            {
                                                char leftFilePath[MAX_PATH];
                                                char rightFilePath[MAX_PATH];
                                                strcpy(leftFilePath, LeftPanel->GetPath());
                                                strcpy(rightFilePath, RightPanel->GetPath());
                                                BOOL pathAppended = TRUE;
                                                pathAppended &= SalPathAppend(leftFilePath, leftFile->Name, MAX_PATH);
                                                pathAppended &= SalPathAppend(rightFilePath, rightFile->Name, MAX_PATH);
                                                if (!pathAppended)
                                                {
                                                    SalMessageBox(progressDlg.HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONEXCLAMATION);
                                                    canceled = TRUE;
                                                    goto ABORT_COMPARE;
                                                }

                                                BOOL different;

                                                BOOL ret = CompareFilesByContent(progressDlg.HWindow, &progressDlg, leftFilePath,
                                                                                 rightFilePath, leftFile->Size + rightFile->Size,
                                                                                 &different, &canceled);
                                                if (ret)
                                                {
                                                    if (different)
                                                    {
                                                        selectLeft = TRUE;
                                                        selectRight = TRUE;
                                                    }
                                                }
                                                else
                                                {
                                                    if (canceled)
                                                        goto ABORT_COMPARE;
                                                    selectLeft = TRUE; // on read error mark the pair as if it had different content (it might)
                                                    selectRight = TRUE;
                                                }
                                            }
                                            else
                                                total += leftFile->Size + rightFile->Size;
                                        }
                                    }
                                }
                                else
                                {
                                    // files have different length, so they must differ in content
                                    selectLeft = TRUE;
                                    selectRight = TRUE;
                                }
                            }

                            if (!getTotal)
                            {
                                if (leftIsNewerNoDSTShiftIgn && !selectLeft || rightIsNewerNoDSTShiftIgn && !selectRight)
                                    foundDSTShifts += isDSTShift; // count only time differences of files that are not already marked for another reason (e.g., due to a difference by another criterion) -- motivation: if we don't need to show a complex DST warning, don't show it

                                if (selectLeft || leftIsNewer)
                                {
                                    leftFile->Selected = 1;
                                    identical = FALSE;
                                }
                                if (selectRight || rightIsNewer)
                                {
                                    rightFile->Selected = 1;
                                    identical = FALSE;
                                }
                            }
                            if (++l < left->Count)
                                leftFile = &left->At(l);
                            if (++r < right->Count)
                                rightFile = &right->At(r);
                        }
                    }
                }
                else // left only
                {
                    if (!getTotal)
                    {
                        leftFile->Selected = 1;
                        identical = FALSE;
                    }
                    if (++l < left->Count)
                        leftFile = &left->At(l);
                }
            }
            else // right only
            {
                if (r < right->Count)
                {
                    if (!getTotal)
                    {
                        rightFile->Selected = 1;
                        identical = FALSE;
                    }
                    if (++r < right->Count)
                        rightFile = &right->At(r);
                }
            }
        }

        // Sal2.0 and TC compare without directories in such a way that they even ignore their names
        // people kept pointing this out to us, so we'll behave the same (with
        // COMPARE_DIRECTORIES_ONEPANELDIRS disabled)
        if ((flags & COMPARE_DIRECTORIES_SUBDIRS) || (flags & COMPARE_DIRECTORIES_ONEPANELDIRS))
        {
            // now compare subdirectories
            subTotalIndex = 0;

            left = leftDirs;
            right = rightDirs;

            CFileData *leftDir, *rightDir;
            l = 0;
            r = 0;

            if (left->Count > 0 && strcmp(left->At(0).Name, "..") == 0)
                l++;
            if (right->Count > 0 && strcmp(right->At(0).Name, "..") == 0)
                r++;

            if (l < left->Count)
                leftDir = &left->At(l);
            if (r < right->Count)
                rightDir = &right->At(r);
            while (l < left->Count || r < right->Count)
            {
                SkipIgnoredNames(ignDirNames, &Configuration.CompareIgnoreDirsMasks, TRUE,
                                 &l, &leftDir, left, &r, &rightDir, right);
                if (l < left->Count)
                {
                    if (r < right->Count) // both panels contain directories
                    {
                        compRes = CmpNameExtIgnCase(*leftDir, *rightDir);
                        if (compRes == -1) // left < right
                        {
                            leftDir->Selected = 1;
                            identical = FALSE;
                            if (++l < left->Count)
                                leftDir = &left->At(l);
                        }
                        else
                        {
                            if (compRes == 1) // left > right
                            {
                                rightDir->Selected = 1;
                                identical = FALSE;
                                if (++r < right->Count)
                                    rightDir = &right->At(r);
                            }
                            else // left == right
                            {
                                BOOL select = FALSE;

                                // By Attributes
                                if (flags & COMPARE_DIRECTORIES_SUBDIRS_ATTR)
                                {
                                    if ((LeftPanel->ValidFileData & VALID_DATA_ATTRIBUTES) &&
                                        (RightPanel->ValidFileData & VALID_DATA_ATTRIBUTES))
                                    {
                                        if ((leftDir->Attr & DISPLAYED_ATTRIBUTES) != (rightDir->Attr & DISPLAYED_ATTRIBUTES))
                                        {
                                            select = TRUE;
                                        }
                                    }
                                }

                                // same names and attributes -> we'll examine the subdirectory content
                                // if the directory was selected in the previous round we won't enumerate it again (getTotal)
                                if ((flags & COMPARE_DIRECTORIES_SUBDIRS) && (!select) && leftDir->Selected == 0)
                                {
                                    // we insert the subpath into the left/rightSubDir variables, which is
                                    // in the case of ptDisk relative to the panel path and in the case
                                    // of ptZIPArchive relative to the path to the archive
                                    char leftSubDir[MAX_PATH];
                                    char rightSubDir[MAX_PATH];

                                    if (LeftPanel->Is(ptDisk))
                                    {
                                        strcpy(leftSubDir, leftDir->Name);
                                    }
                                    else
                                    {
                                        if (LeftPanel->Is(ptZIPArchive))
                                        {
                                            strcpy(leftSubDir, LeftPanel->GetZIPPath());
                                            BOOL pathAppended = SalPathAppend(leftSubDir, leftDir->Name, MAX_PATH);
                                            if (!pathAppended)
                                            {
                                                SalMessageBox(progressDlg.HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONEXCLAMATION);
                                                canceled = TRUE;
                                                goto ABORT_COMPARE;
                                            }
                                        }
                                        else
                                        {
                                            TRACE_E("not implemented");
                                            leftSubDir[0] = 0;
                                        }
                                    }

                                    if (RightPanel->Is(ptDisk))
                                    {
                                        strcpy(rightSubDir, rightDir->Name);
                                    }
                                    else
                                    {
                                        if (RightPanel->Is(ptZIPArchive))
                                        {
                                            strcpy(rightSubDir, RightPanel->GetZIPPath());
                                            BOOL pathAppended = SalPathAppend(rightSubDir, rightDir->Name, MAX_PATH);
                                            if (!pathAppended)
                                            {
                                                SalMessageBox(progressDlg.HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONEXCLAMATION);
                                                canceled = TRUE;
                                                goto ABORT_COMPARE;
                                            }
                                        }
                                        else
                                        {
                                            TRACE_E("not implemented");
                                            rightSubDir[0] = 0;
                                        }
                                    }

                                    BOOL different;
                                    CQuadWord lastTotal; // if the subdirectory differs, skip its size using 'lastTotal' + 'dirSubTotal'
                                    if (!getTotal)
                                        progressDlg.GetActualTotalSize(lastTotal);
                                    CQuadWord subTotal(0, 0);
                                    int foundDSTShiftsInSubDir = 0;
                                    BOOL ret = CompareDirsAux(progressDlg.HWindow, &progressDlg,
                                                              LeftPanel, leftSubDir, leftFAT,
                                                              RightPanel, rightSubDir, rightFAT,
                                                              flags, &different, &canceled,
                                                              getTotal, &subTotal, &foundDSTShiftsInSubDir);
                                    if (ret)
                                    {
                                        if (different)
                                        {
                                            foundDSTShifts += foundDSTShiftsInSubDir;
                                            select = TRUE; // set select even when getTotal is TRUE; optimization, skips the entire tree in the second pass
                                            if (!getTotal && (flags & COMPARE_DIRECTORIES_BYCONTENT))
                                            {
                                                if (dirSubTotal.IsGood() && dirSubTotal.Count > subTotalIndex)
                                                    progressDlg.SetActualTotalSize(lastTotal + dirSubTotal[subTotalIndex]);
                                                else
                                                    TRACE_E("Error total size adjusting, please report it to Honza.");
                                            }
                                        }
                                        else
                                        {
                                            // add to total only if we compare the entire tree by content
                                            if (getTotal)
                                            {
                                                if (dirSubTotal.IsGood())
                                                    dirSubTotal.Add(subTotal);
                                                total += subTotal;
                                            }
                                            else
                                                foundDSTShifts += foundDSTShiftsInSubDir;
                                        }
                                        subTotalIndex++;
                                    }
                                    else
                                    {
                                        if (canceled)
                                            goto ABORT_COMPARE;
                                        else // if it is not 'canceled', an error message was shown and the user wants to continue
                                        {
                                            if (!getTotal)
                                                select = TRUE;
                                        }
                                    }
                                }

                                if (select) // note: used even when getTotal == TRUE (optimization)
                                {
                                    leftDir->Selected = 1;
                                    rightDir->Selected = 1;
                                    identical = FALSE;
                                }

                                if (++l < left->Count)
                                    leftDir = &left->At(l);
                                if (++r < right->Count)
                                    rightDir = &right->At(r);
                            }
                        }
                    }
                    else // left only
                    {
                        leftDir->Selected = 1;
                        identical = FALSE;
                        if (++l < left->Count)
                            leftDir = &left->At(l);
                    }
                }
                else // right only
                {
                    if (r < right->Count)
                    {
                        rightDir->Selected = 1;
                        identical = FALSE;
                        if (++r < right->Count)
                            rightDir = &right->At(r);
                    }
                }
            }
        }

        if (getTotal)
        {
            progressDlg.SetTotalSize(total);
            getTotal = FALSE;
            goto ONCE_MORE;
        }

    ABORT_COMPARE:

        //--- sort according to settings
        if (LeftPanel->SortType != stName || LeftPanel->ReverseSort)
            SortFilesAndDirectories(leftFiles, leftDirs, LeftPanel->SortType, LeftPanel->ReverseSort, Configuration.SortDirsByName);
        if (RightPanel->SortType != stName || RightPanel->ReverseSort)
            SortFilesAndDirectories(rightFiles, rightDirs, RightPanel->SortType, RightPanel->ReverseSort, Configuration.SortDirsByName);

        //--- close the progress dialog
        if (displayDialogBox)
        {
            EnableWindow(HWindow, TRUE);
            HWND actWnd = GetForegroundWindow();
            BOOL activate = actWnd == progressDlg.HWindow || actWnd == HWindow;
            DestroyWindow(progressDlg.HWindow);
            if (activate && hFocusedWnd != NULL)
                SetFocus(hFocusedWnd);
        }

        // previously the selection was cleared at the start of the compare and continuously set during it
        // if an icon redraw request arrived from the icon reader during that time,
        // the dirty bit for the given file/directory was cleared and as a result, it was not redrawn at the end
        // (it remained unselected even though it should have been marked)

        // now we clear the selection at the end
        // and between changing the selection state and calling
        // RepaintListBox no messages are distributed, so a request to redraw the icon cannot be delivered

        // clear the selection
        LeftPanel->SetSel(FALSE, -1);
        RightPanel->SetSel(FALSE, -1);

        // if not canceled, transfer the selection
        if (!canceled)
        {
            int i;
            for (i = 0; i < leftDirs->Count; i++)
                if (leftDirs->At(i).Selected != 0)
                    LeftPanel->SetSel(TRUE, i);
            for (i = 0; i < leftFiles->Count; i++)
                if (leftFiles->At(i).Selected != 0)
                    LeftPanel->SetSel(TRUE, leftDirs->Count + i);
            for (i = 0; i < rightDirs->Count; i++)
                if (rightDirs->At(i).Selected != 0)
                    RightPanel->SetSel(TRUE, i);
            for (i = 0; i < rightFiles->Count; i++)
                if (rightFiles->At(i).Selected != 0)
                    RightPanel->SetSel(TRUE, rightDirs->Count + i);
        }

    } // if (leftFiles != NULL && leftDirs != NULL && rightFiles != NULL && rightDirs != NULL)
    else
    {
        // out of memory
        canceled = TRUE;
    }

    //--- destroy helper arrays
    if (leftFiles != NULL)
        delete leftFiles;
    if (leftDirs != NULL)
        delete leftDirs;
    if (rightFiles != NULL)
        delete rightFiles;
    if (rightDirs != NULL)
        delete rightDirs;

    //--- mark the appropriate files
    LeftPanel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    RightPanel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    PostMessage(LeftPanel->HWindow, WM_USER_SELCHANGED, 0, 0);  // sel-change notify
    PostMessage(RightPanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify

    //--- allow icon loading again
    if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
        LeftPanel->WakeupIconCacheThread();
    if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
        RightPanel->WakeupIconCacheThread();

    //--- display a confirmation if files differing by exactly one or two hours appear
    //    either warn that not all files have the same time or notify that the detected difference can be ignored
    BOOL resultAlreadyShown = FALSE;
    if (!canceled && foundDSTShifts > 0 &&
        (Configuration.IgnoreDSTShifts ? Configuration.CnfrmDSTShiftsIgnored : Configuration.CnfrmDSTShiftsOccured))
    {
        char buf[550];
        char buf2[400];
        CQuadWord qwShifts[2];
        qwShifts[0].Set(foundDSTShifts, 0);
        qwShifts[1].Set(foundDSTShifts, 0);
        ExpandPluralString(buf2, 400,
                           LoadStr(Configuration.IgnoreDSTShifts ? (!canceled && identical ? IDS_CMPDIRS_IGNDSTDIFFSEXACT : IDS_CMPDIRS_IGNDSTDIFFS) : IDS_CMPDIRS_FOUNDDSTDIFFS),
                           2, qwShifts);
        buf[0] = 0;
        if (!canceled && identical)
        {
            resultAlreadyShown = TRUE;
            int messageID = (flags & COMPARE_DIRECTORIES_BYCONTENT) ? IDS_COMPAREDIR_ARE_IDENTICAL : IDS_COMPAREDIR_SEEMS_IDENTICAL;
            _snprintf_s(buf, _TRUNCATE, "%s\n\n%s ", LoadStr(messageID), LoadStr(IDS_CMPDIRS_NOTE));
        }
        _snprintf_s(buf + strlen(buf), _countof(buf) - strlen(buf), _TRUNCATE, buf2, foundDSTShifts);

        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = HWindow;
        params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_HINT;
        params.Caption = LoadStr(IDS_COMPAREDIRSTITLE);
        params.Text = buf;
        params.CheckBoxText = LoadStr(!canceled && identical ? IDS_CMPDIRS_DONTSHOWNOTEAG : IDS_DONTSHOWAGAIN);
        int dontShow = Configuration.IgnoreDSTShifts ? !Configuration.CnfrmDSTShiftsIgnored : !Configuration.CnfrmDSTShiftsOccured;
        params.CheckBoxValue = &dontShow;
        SalMessageBoxEx(&params);

        if (Configuration.IgnoreDSTShifts)
            Configuration.CnfrmDSTShiftsIgnored = !dontShow;
        else
            Configuration.CnfrmDSTShiftsOccured = !dontShow;
    }

    //--- display the verdict
    if (!resultAlreadyShown && !canceled && identical)
    {
        int messageID = (flags & COMPARE_DIRECTORIES_BYCONTENT) ? IDS_COMPAREDIR_ARE_IDENTICAL : IDS_COMPAREDIR_SEEMS_IDENTICAL;
        SalMessageBox(HWindow, LoadStr(messageID), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONINFORMATION);
    }

    // raise the thread priority again, the operation is finished
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    EndStopRefresh(); // now the snooper starts again
}

BOOL CMainWindow::GetViewersAssoc(int wantedViewerType, CDynString* strViewerMasks)
{
    BOOL ok = TRUE;
    EnterViewerMasksCS();
    BOOL first = TRUE;
    CViewerMasks* masks = ViewerMasks;
    int i;
    for (i = 0; i < masks->Count; i++)
    {
        CViewerMasksItem* item = masks->At(i);
        if (!item->OldType && item->ViewerType == wantedViewerType)
        {
            const char* masksStr = item->Masks->GetMasksString();
            int len = (int)strlen(masksStr);
            if (len > 0 && masksStr[len - 1] == ';')
                len--;
            if (strchr(masksStr, '|') != NULL)
            {
                TRACE_E("CMainWindow::GetViewersAssoc(): unexpected situation: masks contains forbidden char '|'!");
                len = 0;
            }
            if (len > 0)
            {
                if (!first)
                    ok &= strViewerMasks->Append(";", 1);
                else
                    first = FALSE;
                ok &= strViewerMasks->Append(masksStr, len);
            }
        }
    }
    LeaveViewerMasksCS();
    return ok;
}

//
// ****************************************************************************
// CDynString
//

BOOL CDynString::Append(const char* str, int len)
{
    if (len == -1)
        len = (int)strlen(str);
    if (Length + len >= Allocated)
    {
        int size = Length + len + 1 + 256; // +256 characters as reserve so we don't allocate so often
        char* newBuf = (char*)realloc(Buffer, size);
        if (newBuf != NULL)
        {
            Buffer = newBuf;
            Allocated = size;
        }
        else // out of memory, tough luck...
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    memmove(Buffer + Length, str, len);
    Length += len;
    Buffer[Length] = 0;
    return TRUE;
}
