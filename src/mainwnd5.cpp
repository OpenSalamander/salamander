// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
    if (!*validDate) // We do not have any date set, we are resetting it...
    {
        st->wYear = 1602;
        st->wMonth = 1;
        st->wDay = 1;
        st->wDayOfWeek = 2;
    }
    if (!*validTime) // We don't have any time set, we are resetting it...
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
        // calculate the difference between times and round it
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

// compare two files 'l' and 'r' based on date/time
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

    if (validDateLeft && validDateRight ||                                    // if the time is not known, it is initialized to 0:00:00.000, so we don't deal with time...
        !validDateLeft && !validDateRight && validTimeLeft && validTimeRight) // date is unknown (zeroed), times are both known, so there is no obstacle to comparison
    {
        FILETIME lf, rf;
        if (Configuration.UseTimeResolution)
        {
            // Time cropping with precision to seconds
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
                    // calculate the difference between times and round it
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
                        res = MyCompareFileTime(&lf, &rf, foundDSTShifts, compResNoDSTShiftIgn); // same filesystems -> o.k.
                    else                                                                         // FAT + other filesystem -> adjusting non-FAT time to FAT time (every 2 seconds)
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
                res = -1; // unknown date is older than any date
            else          // neither date is known
            {
                if (validTimeLeft)
                    res = 1; // any time is newer than an unknown time
                else
                {
                    if (validTimeRight)
                        res = -1; // Unknown time is older than any time
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

// compare two files 'l' and 'r' based on their size
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
            return 1; // anything is greater than an unknown size
        else
        {
            if (rightValidSize)
                return -1; // unknown size is smaller than anything
        }
    }
    return 0;
}

// compare files 'file1' and 'file2', specified by full path, based on their content
// if the comparison is successful, the function returns TRUE and sets the variable
// 'different' (TRUE if a difference is found, otherwise FALSE)
// in case of an error or operation interruption, it returns FALSE and sets the variable
// 'canceled' (TRUE if the user canceled the operation, otherwise FALSE)

#define COMPARE_BUFFER_SIZE (2 * 1024 * 1024) // size of the buffer for comparison in bytes (may not necessarily be fully used)
#define COMPARE_BLOCK_SIZE (32 * 1024)        // size of the block continuously read from the file; WARNING: COMPARE_BUFFER_SIZE must be divisible by COMPARE_BLOCK_SIZE
#define COMPARE_BLOCK_GROUP 8                 // How many blocks can be read at once during fast reading (more than 1 MB/s, see COMPARE_BUF_TIME_LIMIT); WARNING: the number of blocks in the buffer (COMPARE_BUFFER_SIZE / COMPARE_BLOCK_SIZE) must be divisible by COMPARE_BLOCK_GROUP
#define COMPARE_BUF_TIME_LIMIT 2000           // limit in milliseconds for reading the entire buffer (COMPARE_BUFFER_SIZE) - if met, reading is done in groups of COMPARE_BLOCK_GROUP blocks, which speeds up reading from the network 2x to 3x on Vista+; otherwise, reading is done one block at a time (COMPARE_BLOCK_SIZE)

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
    char message[2 * MAX_PATH + 200]; // we need 2*MAX_PATH for the path and then reserve for the error message
    BOOL ret = FALSE;
    *canceled = FALSE;

    //  DWORD totalTi = GetTickCount();

    HANDLE hFile1 = HANDLES_Q(CreateFile(file1, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    HANDLE hFile2 = hFile1 != INVALID_HANDLE_VALUE ? HANDLES_Q(CreateFile(file2, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                                          NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL))
                                                   : INVALID_HANDLE_VALUE;
    DWORD err = GetLastError();

    if (!progressDlg->Continue()) // give the opportunity to redraw the dialogue to 100% of the previous file (opening the file is the longest (3ms) when copying small files (0.1ms))
        *canceled = TRUE;

    // set texts in the progress dialog
    progressDlg->SetSource(file1);
    progressDlg->SetTarget(file2);

    // set total ('bothFileSize') and actual (0%) file-progress
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
                BOOL readingIsFast1 = FALSE; // FALSE = we are waiting for the speed to reach the limit and then we will be able to read in groups (COMPARE_BLOCK_GROUP)
                BOOL readingIsFast2 = FALSE;
                while (TRUE)
                {
                    DWORD bufSize = COMPARE_BUFFER_SIZE;
                    int blockCount = bufSize / COMPARE_BLOCK_SIZE; // number of blocks in bufSize

                    // read another part from file 'file1' into 'buffer1'
                    // we read the entire buffer in 32KB blocks from one file (on W2K and XP it is fastest for both files from one physical disk, on Vista it is only slightly slower than continuous reading)
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
                        if (!progressDlg->Continue()) // give us the opportunity to redraw ourselves in dialogue
                        {
                            *canceled = TRUE;
                            break;
                        }

                        read1 += locRead;
                        if (locRead != readBlockSize)
                            break; // EOF
                        if (GetTickCount() - readBegTime > 200)
                        { // Slow reading (probably slow network disk (VPN) or floppy) - reduce buffer size, worst case scenario we will end up comparing block by block
                            blockCount = block + (canReadGroup ? COMPARE_BLOCK_GROUP : 1);
                            bufSize = blockCount * COMPARE_BLOCK_SIZE;
                            break;
                        }
                        block += canReadGroup ? COMPARE_BLOCK_GROUP : 1;
                    }
                    if (readErr || *canceled)
                        break;
                    readingIsFast1 = WindowsVistaAndLater &&                                                                                           // On W2K/XP this should not lead to acceleration, so we won't poke the snake with a bare foot there
                                     GetTickCount() - readBegTime < (DWORD)(((unsigned __int64)read1 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE); // measure the speed to be more than 1 MB/s
                                                                                                                                                       /*            // reading the entire buffer at once from one file (on Vista, when reading from both files on one physical disk and with a large buffer, it is slightly faster than reading in 32KB blocks)
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
          }*/

                    // read another part from the file 'file2' into 'buffer2'
                    // Reading the entire buffer in blocks from one file (on W2K and XP, it is fastest for both files from one physical disk, on Vista it is only slightly slower than continuous reading)
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
                        if (!progressDlg->Continue()) // give us the opportunity to redraw ourselves in dialogue
                        {
                            *canceled = TRUE;
                            break;
                        }

                        read2 += locRead;
                        if (read1 < read2 || // files are of different lengths now => content is different
                            locRead > 0 && memcmp(buffer1 + block * COMPARE_BLOCK_SIZE, buffer2, locRead) != 0)
                        { // the content of the file is different, it doesn't make sense to continue reading
                            *different = TRUE;
                            ret = TRUE;
                            break;
                        }
                        if (locRead != readBlockSize)
                        { // We failed to load the entire buffer (EOF), the files are identical
                            *different = FALSE;
                            ret = TRUE;
                            break;
                        }
                        block += canReadGroup ? COMPARE_BLOCK_GROUP : 1;
                        if (readingIsFast2 &&
                            GetTickCount() - readBegTime > (DWORD)(((unsigned __int64)read2 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE))
                        {
                            readingIsFast2 = FALSE; // slowdown detection, we proceed through individual blocks (for smooth progress and the possibility of cancellation)
                        }
                    }
                    if (readErr || ret || *canceled)
                        break;
                    readingIsFast2 = WindowsVistaAndLater &&                                                                                           // On W2K/XP this should not lead to acceleration, so we won't poke the snake with a bare foot there
                                     GetTickCount() - readBegTime < (DWORD)(((unsigned __int64)read2 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE); // measure the speed to be more than 1 MB/s
                                                                                                                                                       /*            // reading the entire buffer at once from one file (on Vista, when reading from both files on one physical disk and with a large buffer, it is slightly faster than reading in 32KB blocks)
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
          }*/
                                                                                                                                                       /*            // these tests are performed above during block reading
          AddProgressSizeWithLimit(progressDlg, read1 + read2, &fileProgressTotal, bothFileSize);
          if (!progressDlg->Continue()) // give the dialog a chance to repaint itself
          {
            *canceled = TRUE;
            break;
          }

          if (read1 != read2 ||  // files are now of different lengths => content is different
              read1 > 0 && memcmp(buffer1, buffer2, read1) != 0)
          {  // file content is different, no point in continuing reading
            *different = TRUE;
            ret = TRUE;
            break;
          }
          if (read1 != bufSize)
          { // we failed to load the entire buffer, files are identical
            *different = FALSE;
            ret = TRUE;
            break;
          }*/
                                                                                                                                                       /*            measuredSize += bufSize;
          DWORD ti = GetTickCount() - measureStart;
          if (ti >= 500)
          {
            unsigned __int64 speed = (measuredSize * 1000) / ti;
            TRACE_I("speed: " << (DWORD)(speed / 1024));
            measureStart = GetTickCount();
            measuredSize = 0;
          }*/
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
    if (!*canceled) // we need to bring the total progress to the end of the file
        progressDlg->AddSize(bothFileSize - fileProgressTotal);

    //  TRACE_I("CompareFilesByContent(): total time (in secs): " << ((GetTickCount() - totalTi) / 1000));

    return ret;
}

// Load directories and files into arrays 'dirs' and 'files'
// Source path is determined by summing the path in the 'panel' and 'subPath' panel
// 'hWindow' is a window for displaying a messagebox
// 'progressDlg' is used to move the progress after loading X files/directories
// returns TRUE in case of successful loading of both arrays
// returns FALSE in case of an error or cancellation of the operation by the user
// (the variable 'canceled' will be set later)
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
                    *canceled = FALSE; // we are only getting the size, no need to bother, we will skip the error
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
                if (counter++ > 200) // after loading 200 items
                {
                    if (!progressDlg->Continue()) // give us the opportunity to redraw ourselves in dialogue
                    {
                        HANDLES(FindClose(hFind));
                        *canceled = TRUE;
                        return FALSE;
                    }
                    counter = 0;
                }

                CFileData file;
                // Initialization of structure members that we will no longer change
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
                memmove(file.Name, data.cFileName, nameLen + 1); // copy of text
                file.NameLen = nameLen;

                //--- extension
                if (!Configuration.SortDirsByExt && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // it's about ptDisk
                {
                    file.Ext = file.Name + file.NameLen; // directories do not have extensions
                }
                else
                {
                    const char* s = data.cFileName + nameLen;
                    while (--s >= data.cFileName && *s != '.')
                        ;
                    //          if (s > data.cFileName) file.Ext = file.Name + (s - data.cFileName + 1); // ".cvspass" in Windows is the extension ...
                    if (s >= data.cFileName)
                        file.Ext = file.Name + (s - data.cFileName + 1);
                    else
                        file.Ext = file.Name + file.NameLen;
                }

                //--- others
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
                *canceled = FALSE; // we are only getting the size, no need to bother, we will skip the error
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
            return FALSE; // low memory. I'll get out
        }

        files->SetDeleteData(FALSE); // just shallow copies of data
        dirs->SetDeleteData(FALSE);  // just shallow copies of data

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
                    return FALSE; // low memory. I'll get out
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
                    return FALSE; // low memory. I'll get out
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

// Recursive function searching for the difference between directories
// Directories are specified by the path in the left and right panel and the variables 'leftSubDir' and 'rightSubDir'
// leftFAT and rightFAT indicate whether the respective panel is on the FAT system; if it is open
// archive, the corresponding xxxFAT will be set to FALSE
// 'flags' specifies the comparison criteria and belongs to the COMPARE_DIRECTORIES_xxx family
// Function returns TRUE in case of successful comparison and sets the variable 'different' (TRUE
// if the directories are different, otherwise FALSE).
// In case of an error or interruption of the operation by the user, the function returns FALSE and sets
// variable 'canceled' (TRUE in case of user interruption, otherwise FALSE)

// supports ptDisk and ptZIPArchive

BOOL CompareDirsAux(HWND hWindow, CCmpDirProgressDialog* progressDlg,
                    CFilesWindow* leftPanel, const char* leftSubDir, BOOL leftFAT,
                    CFilesWindow* rightPanel, const char* rightSubDir, BOOL rightFAT,
                    DWORD flags, BOOL* different, BOOL* canceled,
                    BOOL getTotal, CQuadWord* total, int* foundDSTShifts)
{
    // left/rightPanel and left/rightSubDir determine the path,
    // whose directories and files will be stored in the following arrays
    CFilesArray leftDirs;
    CFilesArray rightDirs;
    int foundDSTShiftsInThisDir = 0;

    { // Local block due to the limitation of the validity of leftFiles & rightFiles (saving memory before recursion into a subdirectory)
        CFilesArray leftFiles;
        CFilesArray rightFiles;

        // the value set from the previous file comparison remains
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

        // read directories and files for the left
        if (!ReadDirsAndFilesAux(hWindow, flags, progressDlg, leftPanel, leftSubDir, &leftDirs, &leftFiles, canceled, getTotal))
            return FALSE;

        // and right panel
        if (!ReadDirsAndFilesAux(hWindow, flags, progressDlg, rightPanel, rightSubDir, &rightDirs, &rightFiles, canceled, getTotal))
            return FALSE;

        // if the numbers of directories in the left and right panel do not match, the directories are different
        if (leftDirs.Count != rightDirs.Count || leftFiles.Count != rightFiles.Count)
        {
            *different = TRUE;
            return TRUE;
        }

        // now it holds that the number of directories in the file is the same in both panels

        // we will sort the array by name so that we can compare them pairwise
        if (leftDirs.Count > 1)
            SortNameExt(leftDirs, 0, leftDirs.Count - 1, FALSE);
        if (leftFiles.Count > 1)
            SortNameExt(leftFiles, 0, leftFiles.Count - 1, FALSE);
        if (rightDirs.Count > 1)
            SortNameExt(rightDirs, 0, rightDirs.Count - 1, FALSE);
        if (rightFiles.Count > 1)
            SortNameExt(rightFiles, 0, rightFiles.Count - 1, FALSE);

        // first we will compare files by name, time, and attributes
        // We will postpone the comparison by content because it is slow and if
        // We will find the difference already at this level, saving time
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

            // compare files by content -- here we only compare file sizes as a trivial test for content mismatch
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

            // By Time -- we test times as the last resort due to DST time shift (we will not report DST problem if files differ in size or attributes)
            if (flags & COMPARE_DIRECTORIES_BYTIME)
            {
                int isDSTShift = 0; // 1 = the times of the currently compared pair of files differ by exactly one or two hours, 0 = do not differ
                int compResNoDSTShiftIgn;
                if (CompareFilesByTime(leftPanel, leftFile, leftFAT, rightPanel, rightFile, rightFAT,
                                       &isDSTShift, &compResNoDSTShiftIgn) != 0)
                {
                    if (isDSTShift != 0)
                        timeDiffWithDSTShiftExists = TRUE; // Let's try to find another difference (ideally without DST warning)
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

            // We do not compare addresses by time
        }

        if (timeDiffWithDSTShiftExists) // We did not find any other difference, so we will report the unignored DST time shift including a warning
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
                            // construct full paths to both files
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

                            // I found two different files, we're done
                            if (*different)
                                return TRUE;
                        }
                        else
                            *total += leftFile->Size + rightFile->Size;
                    }
                }
                else
                {
                    // files have different lengths, they are different in content
                    *different = TRUE;
                    return TRUE;
                }
            }
        }

        // We did not find any difference
        // We won't need the files anymore, we can free up memory
        // leftFiles.DestroyMembers();  -- Petr: we will directly perform the destruction of the local object, it will release more
        // rightFiles.DestroyMembers(); -- Petr: we will perform direct destruction of the local object, it will release more
    }

    // We did not find any differences, recursively calling ourselves on individual directories
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
                return TRUE; // We found a difference in the subdirectory, we are finishing
            }
            foundDSTShiftsInThisDir += foundDSTShiftsInSubDir;
        }
    }

    // We did not find any difference
    *different = FALSE;
    *foundDSTShifts += foundDSTShiftsInThisDir;
    return TRUE;
}

// Creates a shallow copy and sets Selected to FALSE for all items
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
            ret->SetDeleteData(FALSE); // only shallow copy of data, prevent destruction
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
            TRACE_E("CMainWindow::CompareDirectories: CompareIgnoreFilesMasks is invalid."); // probably only threatens manual editing of the configuration in the registry
            return;
        }
    }
    BOOL ignDirNames = (flags & COMPARE_DIRECTORIES_IGNDIRNAMES) != 0;
    if (ignDirNames)
    {
        int errPos;
        if (!Configuration.CompareIgnoreDirsMasks.PrepareMasks(errPos))
        {
            TRACE_E("CMainWindow::CompareDirectories: CompareIgnoreDirsMasks is invalid."); // probably only threatens manual editing of the configuration in the registry
            return;
        }
    }

    BeginStopRefresh(); // He was snoring in his sleep

    // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    //--- open progress dialog
    BOOL displayDialogBox = (flags & COMPARE_DIRECTORIES_BYCONTENT) != 0 || // It doesn't make sense to display a dialog for quick actions
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
    int foundDSTShifts = 0; // How many times the file times differed exactly by one or two hours, with this being the only difference in user-selected criteria

    //--- shallow copy of the Files and Dirs arrays; (we need to work on a copy of the data because during the displayed progress dialog
    //    to redraw part of the panel would reveal that we changed the order of the panels or moved the selection)

    CFilesArray* leftFiles = GetShallowCopy(LeftPanel->Files);
    CFilesArray* leftDirs = GetShallowCopy(LeftPanel->Dirs);
    CFilesArray* rightFiles = GetShallowCopy(RightPanel->Files);
    CFilesArray* rightDirs = GetShallowCopy(RightPanel->Dirs);

    if (leftFiles != NULL && leftDirs != NULL && rightFiles != NULL && rightDirs != NULL)
    {
        // managed to create shallow copies
        //--- detection of the FAT system for accurate comparison of lastWrite
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
        // Stop loading icons to avoid competition with comparison
        if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
            LeftPanel->SleepIconCacheThread();
        if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
            RightPanel->SleepIconCacheThread();
        //--- sorting by name
        if (LeftPanel->SortType != stName || LeftPanel->ReverseSort)
            SortFilesAndDirectories(leftFiles, leftDirs, stName, FALSE, Configuration.SortDirsByName);
        if (RightPanel->SortType != stName || RightPanel->ReverseSort)
            SortFilesAndDirectories(rightFiles, rightDirs, stName, FALSE, Configuration.SortDirsByName);
        //--- comparison + marking
        BOOL getTotal;
        if (flags & COMPARE_DIRECTORIES_BYCONTENT)
            getTotal = TRUE; // TRUE: we do not mark differences, we add the file size to the 'total' variable for comparison
        else
            getTotal = FALSE;  // FALSE: marking differences, comparing files by content
        CQuadWord total(0, 0); // total number of bytes that will need to be compared (in the worst case)

        // array 'dirSubTotal' contains sizes of individual subdirectories (files contained in them)
        // array is filled in the first pass (getTotal == TRUE)
        // Used in the second pass (getTotal == FALSE): if the subdirectory is different, we know how much to skip on the total progress
        TDirectArray<CQuadWord> dirSubTotal(max(1, min(leftDirs->Count, rightDirs->Count)), 1);
        int subTotalIndex; // index into the array dirSubTotal

    ONCE_MORE:
        // first files from left and right directory
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
                            int isDSTShift = 0; // 1 = the times of the currently compared pair of files differ by exactly one or two hours, 0 = do not differ (or the times are not compared at all)

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
                                if (leftFile->Size == rightFile->Size) // the size test is intentionally not covered by the following optimization condition in order to mark both files and avoid unnecessary DST warnings
                                {
                                    if (!selectLeft && !leftIsNewer || !selectRight && !rightIsNewer) // If both files are dirty, there is no point in comparing them by content
                                    {
                                        // if both files have zero length, they must be content-identical
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
                                                    selectLeft = TRUE; // in case of a file reading error, it is better to mark the pair as having different content (it may have)
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
                                    // files have different lengths, their content must differ
                                    selectLeft = TRUE;
                                    selectRight = TRUE;
                                }
                            }

                            if (!getTotal)
                            {
                                if (leftIsNewerNoDSTShiftIgn && !selectLeft || rightIsNewerNoDSTShiftIgn && !selectRight)
                                    foundDSTShifts += isDSTShift; // we only calculate the time differences of files that are not already marked for another reason (e.g. due to a difference according to another criterion) -- motivation: if a complex warning about DST does not need to be shown, we do not show it

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
                else // just left
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
            else // just right
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

        // Sal2.0 and TC compare without directories by also ignoring their names
        // People kept throwing it in our faces, so we will act the same (situation)
        // without COMPARE_DIRECTORIES_ONEPANELDIRS)
        if ((flags & COMPARE_DIRECTORIES_SUBDIRS) || (flags & COMPARE_DIRECTORIES_ONEPANELDIRS))
        {
            // now we will compare subdirectories
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

                                // same names, same attributes -> we will examine the contents of subdirectories
                                // if the directory is selected from the previous round, we will not enumerate it again (getTotal)
                                if ((flags & COMPARE_DIRECTORIES_SUBDIRS) && (!select) && leftDir->Selected == 0)
                                {
                                    // we insert a subdirectory into the left/rightSubDir variables, which is
                                    // in case of ptDisk related to the path in the panel and in case
                                    // ptZIPArchive to the path to the archive
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
                                    CQuadWord lastTotal; // if the subdirectory is different, we will skip its size using 'lastTotal' + 'dirSubTotal'
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
                                            select = TRUE; // select also in case of getTotal; optimization, in the second round we will skip the entire tree
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
                                            // We will only add up the total if we compare the entire tree based on its content
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
                                        else // if 'canceled' is not, an error message was displayed and the user wants to continue
                                        {
                                            if (!getTotal)
                                                select = TRUE;
                                        }
                                    }
                                }

                                if (select) // note that it is also used in case getTotal == TRUE (optimization)
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
                    else // just left
                    {
                        leftDir->Selected = 1;
                        identical = FALSE;
                        if (++l < left->Count)
                            leftDir = &left->At(l);
                    }
                }
                else // just right
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

        //--- sorting according to settings
        if (LeftPanel->SortType != stName || LeftPanel->ReverseSort)
            SortFilesAndDirectories(leftFiles, leftDirs, LeftPanel->SortType, LeftPanel->ReverseSort, Configuration.SortDirsByName);
        if (RightPanel->SortType != stName || RightPanel->ReverseSort)
            SortFilesAndDirectories(rightFiles, rightDirs, RightPanel->SortType, RightPanel->ReverseSort, Configuration.SortDirsByName);

        //--- close progress dialog
        if (displayDialogBox)
        {
            EnableWindow(HWindow, TRUE);
            HWND actWnd = GetForegroundWindow();
            BOOL activate = actWnd == progressDlg.HWindow || actWnd == HWindow;
            DestroyWindow(progressDlg.HWindow);
            if (activate && hFocusedWnd != NULL)
                SetFocus(hFocusedWnd);
        }

        // drive the selection fell at the beginning of the compare and adjusted it continuously during it
        // if a message about the icon being redrawn was received from the icon reader at that time, drop
        // set the dirty bit to the given file/directory and then did not redraw at the end
        // (remained unselected, even though it should have been marked)

        // now we will defer the selection until the end and between changing the selection state and calling
        // RepaintListBox does not distribute messages, so delivery cannot occur
        // Request to redraw the icon

        // remove selection
        LeftPanel->SetSel(FALSE, -1);
        RightPanel->SetSel(FALSE, -1);

        // if cancellation did not occur, we will transfer the label
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

    //--- destruction of auxiliary arrays
    if (leftFiles != NULL)
        delete leftFiles;
    if (leftDirs != NULL)
        delete leftDirs;
    if (rightFiles != NULL)
        delete rightFiles;
    if (rightDirs != NULL)
        delete rightDirs;

    //--- marking of relevant files
    LeftPanel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    RightPanel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    PostMessage(LeftPanel->HWindow, WM_USER_SELCHANGED, 0, 0);  // sel-change notify
    PostMessage(RightPanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify

    //--- allow loading icons
    if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
        LeftPanel->WakeupIconCacheThread();
    if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
        RightPanel->WakeupIconCacheThread();

    //--- display confirmation if files differing by exactly an hour or two have appeared (either warn,
    //    not all files have the same time or we warn that the found difference can be ignored
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

    // increase the thread priority again, operation completed
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    EndStopRefresh(); // now he's sniffling again, he'll start up
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
        int size = Length + len + 1 + 256; // Allocate +256 characters for the buffer, so it doesn't have to be reallocated so often
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
