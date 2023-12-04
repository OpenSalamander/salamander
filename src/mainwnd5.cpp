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
    if (validFileData & (VALID_DATA_DATE | VALID_DATA_TIME)) // aspon neco je v LastWrite
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
    if (!*validDate) // nemame nastaveny zadny datum, vynulujeme ho...
    {
        st->wYear = 1602;
        st->wMonth = 1;
        st->wDay = 1;
        st->wDayOfWeek = 2;
    }
    if (!*validTime) // nemame nastaveny zadny cas, vynulujeme ho...
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
        // vypocet rozdilu mezi casy a jeho zaokrouhleni
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

// porovna dva soubory 'l' a 'r' podle datumu/casu
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

    if (validDateLeft && validDateRight ||                                    // pokud neni znamy cas, je inicializovan na 0:00:00.000, takze cas neresime...
        !validDateLeft && !validDateRight && validTimeLeft && validTimeRight) // datum je neznamy (nulovany), casy jsou oba zname, takze porovnani nic nebrani
    {
        FILETIME lf, rf;
        if (Configuration.UseTimeResolution)
        {
            // oriznuti casu s presnosti na sekundy
            stLeft.wMilliseconds = 0;
            stRight.wMilliseconds = 0;
            // prevod casu na cisla
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
                    // vypocet rozdilu mezi casy a jeho zaokrouhleni
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
                        res = MyCompareFileTime(&lf, &rf, foundDSTShifts, compResNoDSTShiftIgn); // stejne filesystemy -> o.k.
                    else                                                                         // FAT + jiny filesystem -> uprava neFAT casu na FAT cas (po 2 sekundach)
                    {
                        WORD date, time; // FAT hodnoty
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
            res = 1; // jakykoliv datum je novejsi nez neznamy datum
        else
        {
            if (validDateRight)
                res = -1; // neznamy datum je starsi nez jakykoliv datum
            else          // ani jeden datum neni znamy
            {
                if (validTimeLeft)
                    res = 1; // jakykoliv cas je novejsi nez neznamy cas
                else
                {
                    if (validTimeRight)
                        res = -1; // neznamy cas je starsi nez jakykoliv cas
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

// porovna dva soubory 'l' a 'r' podle velikosti
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
            return 1; // cokoliv je vetsi nez neznama velikost
        else
        {
            if (rightValidSize)
                return -1; // neznama velikost je mensi nez cokoliv
        }
    }
    return 0;
}

// porovna soubory 'file1' a 'file2', urcene plnou cestou, podle obsahu
// v pripade uspesneho porovnani vraci funkce TRUE a nastavuje promennou
// 'different' (TRUE v pripade nalezeni rozdilu, jinak FALSE)
// v pripade chyby nebo preruseni operace vraci FALSE a nastavuje promennou
// 'canceled' (TRUE pokud uzivatel stornoval operaci, jinak FALSE)

#define COMPARE_BUFFER_SIZE (2 * 1024 * 1024) // velikost bufferu pro porovnani v bytech (nemusi se nutne pouzivat cely)
#define COMPARE_BLOCK_SIZE (32 * 1024)        // velikost bloku souvisle cteneho ze souboru; POZOR: COMPARE_BUFFER_SIZE musi byt delitelne COMPARE_BLOCK_SIZE
#define COMPARE_BLOCK_GROUP 8                 // po kolika blocich najednou se muze cist pri dost rychlem cteni (vic nez 1 MB/s, viz COMPARE_BUF_TIME_LIMIT); POZOR: pocet bloku v bufferu (COMPARE_BUFFER_SIZE / COMPARE_BLOCK_SIZE) musi byt delitelny COMPARE_BLOCK_GROUP
#define COMPARE_BUF_TIME_LIMIT 2000           // limit v milisekundach pro nacteni celeho bufferu (COMPARE_BUFFER_SIZE) - je-li splneny, cte se po skupinach bloku COMPARE_BLOCK_GROUP, coz na Vista+ 2x az 3x zrychluje cteni ze site; jinak se cte po jednom bloku (COMPARE_BLOCK_SIZE)

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
    char message[2 * MAX_PATH + 200]; // potrebujeme 2*MAX_PATH pro cestu a potom rezervu pro chybovou hlasku
    BOOL ret = FALSE;
    *canceled = FALSE;

    //  DWORD totalTi = GetTickCount();

    HANDLE hFile1 = HANDLES_Q(CreateFile(file1, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    HANDLE hFile2 = hFile1 != INVALID_HANDLE_VALUE ? HANDLES_Q(CreateFile(file2, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                                          NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL))
                                                   : INVALID_HANDLE_VALUE;
    DWORD err = GetLastError();

    if (!progressDlg->Continue()) // dame prilezitost dialogu prekreslit se na 100% predchoziho souboru (otevirani souboru je nejdelsi (3ms) pri kopirovani malych souboru (0,1ms))
        *canceled = TRUE;

    // nastavime texty v progress dialogu
    progressDlg->SetSource(file1);
    progressDlg->SetTarget(file2);

    // nastavime total ('bothFileSize') a actual (0%) file-progress
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
                BOOL readingIsFast1 = FALSE; // FALSE = cekame az rychlost dosahne limitu a budeme moct cist po skupinach (COMPARE_BLOCK_GROUP)
                BOOL readingIsFast2 = FALSE;
                while (TRUE)
                {
                    DWORD bufSize = COMPARE_BUFFER_SIZE;
                    int blockCount = bufSize / COMPARE_BLOCK_SIZE; // pocet bloku v bufSize

                    // nacteme do 'buffer1' dalsi cast ze souboru 'file1'
                    // cteme po 32KB blocich cely buffer z jednoho souboru (na W2K a XP je to pri obou souborech z jednoho fyzickeho disku nejrychlejsi, na Viste je to jen mirne pomalejsi nez souvisle cteni)
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
                        if (!progressDlg->Continue()) // dame prilezitost dialogu prekreslit se
                        {
                            *canceled = TRUE;
                            break;
                        }

                        read1 += locRead;
                        if (locRead != readBlockSize)
                            break; // EOF
                        if (GetTickCount() - readBegTime > 200)
                        { // pomale cteni (asi pomaly sitovy disk (VPN) nebo floppy) - zmensime buffer, v nejhorsim se dostaneme na porovnavani po jednom bloku
                            blockCount = block + (canReadGroup ? COMPARE_BLOCK_GROUP : 1);
                            bufSize = blockCount * COMPARE_BLOCK_SIZE;
                            break;
                        }
                        block += canReadGroup ? COMPARE_BLOCK_GROUP : 1;
                    }
                    if (readErr || *canceled)
                        break;
                    readingIsFast1 = WindowsVistaAndLater &&                                                                                           // na W2K/XP by tohle nemelo vest k urychleni, takze tam nebudeme drazdit hada bosou nohou
                                     GetTickCount() - readBegTime < (DWORD)(((unsigned __int64)read1 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE); // pomerime rychlost, aby byla vic nez 1 MB/s
                                                                                                                                                       /*
          // cteme cely buffer najednou z jednoho souboru (na Viste je to pri obou souborech z jednoho fyzickeho disku a velkem bufferu mirne rychlejsi nez cteni po 32KB blocich)
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

                    // nacteme do 'buffer2' dalsi cast ze souboru 'file2'
                    // cteme po blocich cely buffer z jednoho souboru (na W2K a XP je to pri obou souborech z jednoho fyzickeho disku nejrychlejsi, na Viste je to jen mirne pomalejsi nez souvisle cteni)
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
                        if (!progressDlg->Continue()) // dame prilezitost dialogu prekreslit se
                        {
                            *canceled = TRUE;
                            break;
                        }

                        read2 += locRead;
                        if (read1 < read2 || // soubory jsou ted ruzne dlouhe => obsah je ruzny
                            locRead > 0 && memcmp(buffer1 + block * COMPARE_BLOCK_SIZE, buffer2, locRead) != 0)
                        { // obsah souboru je ruzny, nema smysl pokracovat ve cteni
                            *different = TRUE;
                            ret = TRUE;
                            break;
                        }
                        if (locRead != readBlockSize)
                        { // uz se nam nepodarilo nacist cely buffer (EOF), soubory jsou shodne
                            *different = FALSE;
                            ret = TRUE;
                            break;
                        }
                        block += canReadGroup ? COMPARE_BLOCK_GROUP : 1;
                        if (readingIsFast2 &&
                            GetTickCount() - readBegTime > (DWORD)(((unsigned __int64)read2 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE))
                        {
                            readingIsFast2 = FALSE; // detekce zpomaleni, dale jedeme po jednotlivych blocich (kvuli plynulosti porgresu a moznosti zcancelovani)
                        }
                    }
                    if (readErr || ret || *canceled)
                        break;
                    readingIsFast2 = WindowsVistaAndLater &&                                                                                           // na W2K/XP by tohle nemelo vest k urychleni, takze tam nebudeme drazdit hada bosou nohou
                                     GetTickCount() - readBegTime < (DWORD)(((unsigned __int64)read2 * COMPARE_BUF_TIME_LIMIT) / COMPARE_BUFFER_SIZE); // pomerime rychlost, aby byla vic nez 1 MB/s
                                                                                                                                                       /*
          // cteme cely buffer najednou z jednoho souboru (na Viste je to pri obou souborech z jednoho fyzickeho disku a velkem bufferu mirne rychlejsi nez cteni po 32KB blocich)
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
          // tyto testy se pri blokovem cteni provadi vyse
          AddProgressSizeWithLimit(progressDlg, read1 + read2, &fileProgressTotal, bothFileSize);
          if (!progressDlg->Continue()) // dame prilezitost dialogu prekreslit se
          {
            *canceled = TRUE;
            break;
          }

          if (read1 != read2 ||  // soubory jsou ted ruzne dlouhe => obsah je ruzny
              read1 > 0 && memcmp(buffer1, buffer2, read1) != 0)
          {  // obsah souboru je ruzny, nema smysl pokracovat ve cteni
            *different = TRUE;
            ret = TRUE;
            break;
          }
          if (read1 != bufSize)
          { // uz se nam nepodarilo nacist cely buffer, soubory jsou shodne
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
    if (!*canceled) // musime dotahnout total progress na konec souboru
        progressDlg->AddSize(bothFileSize - fileProgressTotal);

    //  TRACE_I("CompareFilesByContent(): total time (in secs): " << ((GetTickCount() - totalTi) / 1000));

    return ret;
}

// nacte adresare a soubory do poli 'dirs' a 'files'
// zdrojova cesta je urcena souctem cesty v panelu 'panel' a 'subPath'
// 'hWindow' je okno pro zobrazovani messageboxu
// 'progressDlg' slouzi k posouvani progresu po nacteni X souboru/adresaru
// vraci TRUE v pripade uspesneho nacteni obou poli
// vraci FALSE v pripade chyby nebo stornovani operace uzivatelem
// (bude pak nastavena promenna 'canceled')
//
// podporuje ptDisk a ptZIPArchive

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
                    *canceled = FALSE; // pouze ziskavame velikost, nema smysl prudit, skipneme chybu
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
                if (counter++ > 200) // po nacteni 200 polozek
                {
                    if (!progressDlg->Continue()) // dame prilezitost dialogu prekreslit se
                    {
                        HANDLES(FindClose(hFind));
                        *canceled = TRUE;
                        return FALSE;
                    }
                    counter = 0;
                }

                CFileData file;
                // inicializace clenu struktury, ktere uz dale nebudeme menit
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

                //--- jmeno
                file.Name = (char*)malloc(nameLen + 1); // alokace
                if (file.Name == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    HANDLES(FindClose(hFind));
                    *canceled = TRUE;
                    return FALSE;
                }
                memmove(file.Name, data.cFileName, nameLen + 1); // kopie textu
                file.NameLen = nameLen;

                //--- pripona
                if (!Configuration.SortDirsByExt && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // jde o ptDisk
                {
                    file.Ext = file.Name + file.NameLen; // adresare nemaji pripony
                }
                else
                {
                    const char* s = data.cFileName + nameLen;
                    while (--s >= data.cFileName && *s != '.')
                        ;
                    //          if (s > data.cFileName) file.Ext = file.Name + (s - data.cFileName + 1); // ".cvspass" ve Windows je pripona ...
                    if (s >= data.cFileName)
                        file.Ext = file.Name + (s - data.cFileName + 1);
                    else
                        file.Ext = file.Name + file.NameLen;
                }

                //--- ostatni
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
                *canceled = FALSE; // pouze ziskavame velikost, nema smysl prudit, skipneme chybu
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
            return FALSE; // low memory. vypadnem
        }

        files->SetDeleteData(FALSE); // jen melke kopie dat
        dirs->SetDeleteData(FALSE);  // jen melke kopie dat

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
                    return FALSE; // low memory. vypadnem
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
                    return FALSE; // low memory. vypadnem
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

// rekurzivni funkce hledajici rozdil mezi adresari
// adresare jsou urcene cestou v levem a pravem panelu a promennou 'leftSubDir' a 'rightSubDir'
// leftFAT a rightFAT udavaji, zda je prislusny panel na FAT systemu; pokud je otevreny
// arhiv, bude prislusna xxxFAT nastavena na FALSE
// 'flags' specifikuje kriteria porovnani a je z rodiny COMPARE_DIRECTORIES_xxx
// funkce vraci TRUE v pripade uspesneho porovnani a nastavuje promennou 'different' (TRUE
// pokud se adresare lisi, jinak FALSE).
// v pripade chyby nebo preruseni operace uzivatelem vraci funkce FALSE a nastavuje
// promennou 'canceled' (TRUE v pripade preruseni uzivatelem, jinak FALSE)

// podporuje ptDisk a ptZIPArchive

BOOL CompareDirsAux(HWND hWindow, CCmpDirProgressDialog* progressDlg,
                    CFilesWindow* leftPanel, const char* leftSubDir, BOOL leftFAT,
                    CFilesWindow* rightPanel, const char* rightSubDir, BOOL rightFAT,
                    DWORD flags, BOOL* different, BOOL* canceled,
                    BOOL getTotal, CQuadWord* total, int* foundDSTShifts)
{
    // left/rightPanel a left/rightSubDir urcuji cestu,
    // jejiz adresare a soubory budou ulozeny do nasledujicich poli
    CFilesArray leftDirs;
    CFilesArray rightDirs;
    int foundDSTShiftsInThisDir = 0;

    { // lokalni blok kvuli omezeni platnosti leftFiles & rightFiles (setrime pamet pred zarekurzenim do podadresaru)
        CFilesArray leftFiles;
        CFilesArray rightFiles;

        // z predesleho porovnani souboru zustala nastavena hodnota
        if ((flags & COMPARE_DIRECTORIES_BYCONTENT) && (!getTotal))
            progressDlg->SetActualFileSize(CQuadWord(0, 0)); // nastavime 0%

        // nastavime texty v progress dialogu
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

        // nacteme adresare a soubory pro levy
        if (!ReadDirsAndFilesAux(hWindow, flags, progressDlg, leftPanel, leftSubDir, &leftDirs, &leftFiles, canceled, getTotal))
            return FALSE;

        // a pravy panel
        if (!ReadDirsAndFilesAux(hWindow, flags, progressDlg, rightPanel, rightSubDir, &rightDirs, &rightFiles, canceled, getTotal))
            return FALSE;

        // pokud se nerovnaji pocty v levem a pravem panelu, jsou adresare ruzne
        if (leftDirs.Count != rightDirs.Count || leftFiles.Count != rightFiles.Count)
        {
            *different = TRUE;
            return TRUE;
        }

        // ted uz plati, ze pocet adresaru soubor je stejny v obou panelech

        // seradime pole podle jmena, abychom je mohli vzajeme porovnat
        if (leftDirs.Count > 1)
            SortNameExt(leftDirs, 0, leftDirs.Count - 1, FALSE);
        if (leftFiles.Count > 1)
            SortNameExt(leftFiles, 0, leftFiles.Count - 1, FALSE);
        if (rightDirs.Count > 1)
            SortNameExt(rightDirs, 0, rightDirs.Count - 1, FALSE);
        if (rightFiles.Count > 1)
            SortNameExt(rightFiles, 0, rightFiles.Count - 1, FALSE);

        // napred porovname soubory podle jmena, casu a atributu
        // porovnani podle obsahu odlozime, protoze je pomale a pokud
        // nalezneme rozdil uz na teto urovni, usetrime cas
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

            // porovname soubory podle obsahu -- tady jen porovname velikosti souboru jako trivialni test neshody obsahu souboru
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

            // By Time -- kvuli DST time shiftu testujeme casy az jako posledni (nebudeme hlasit DST problem, kdyz se soubory lisi treba jeste ve velikosti nebo atributech)
            if (flags & COMPARE_DIRECTORIES_BYTIME)
            {
                int isDSTShift = 0; // 1 = casy prave porovnavane dvojice souboru se lisi presne o jednu nebo dve hodiny, 0 = nelisi
                int compResNoDSTShiftIgn;
                if (CompareFilesByTime(leftPanel, leftFile, leftFAT, rightPanel, rightFile, rightFAT,
                                       &isDSTShift, &compResNoDSTShiftIgn) != 0)
                {
                    if (isDSTShift != 0)
                        timeDiffWithDSTShiftExists = TRUE; // zkusime najit jeste jinou diferenci (idealne bez DST warningu)
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

        // porovname adresare
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

            // adresare podle casu neporovnavame
        }

        if (timeDiffWithDSTShiftExists) // nenasli jsme jiny rozdil, tak ohlasime neignorovany DST time shift vcetne warningu
        {
            (*foundDSTShifts)++;
            *different = TRUE;
            return TRUE;
        }

        // porovname soubory podle obsahu
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
                            // slozime plne cesty k oboum souborum
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

                            // nasel jsem dva ruzne soubory, koncime
                            if (*different)
                                return TRUE;
                        }
                        else
                            *total += leftFile->Size + rightFile->Size;
                    }
                }
                else
                {
                    // soubory maji ruznou delku, jsou obsahove ruzne
                    *different = TRUE;
                    return TRUE;
                }
            }
        }

        // nenasli jsme zadny rozdil
        // soubory uz dale nebudeme potrebovat, muzeme uvolnit pamet
        // leftFiles.DestroyMembers();  -- Petr: provedeme primo destrukci lokalniho objektu, uvolni toho vic
        // rightFiles.DestroyMembers(); -- Petr: provedeme primo destrukci lokalniho objektu, uvolni toho vic
    }

    // nenasli jsme zadny rozdil, rekurzivne volame sami sebe na jednotlive adresare
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
                return TRUE; // nasli jsme rozdil v podadresari, koncime
            }
            foundDSTShiftsInThisDir += foundDSTShiftsInSubDir;
        }
    }

    // nenasli jsme zadny rozdil
    *different = FALSE;
    *foundDSTShifts += foundDSTShiftsInThisDir;
    return TRUE;
}

// vytvori melkou kopii a nastavi Selected pro vsechny polozky na FALSE
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
            ret->SetDeleteData(FALSE); // jen melka kopie dat, zamezime destrukci
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
            { // preskocime ignorovana jmena v levem panelu
                if (++(*l) < left->Count)
                    *leftFile = &left->At(*l);
                else
                    break;
            }
        }
        if (*r < right->Count)
        {
            while (ignoreNamesMasks->AgreeMasks((*rightFile)->Name, dirs ? NULL : (*rightFile)->Ext))
            { // preskocime ignorovana jmena v pravem panelu
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
            TRACE_E("CMainWindow::CompareDirectories: CompareIgnoreFilesMasks is invalid."); // hrozi asi jen pri rucni editaci konfigurace v registry
            return;
        }
    }
    BOOL ignDirNames = (flags & COMPARE_DIRECTORIES_IGNDIRNAMES) != 0;
    if (ignDirNames)
    {
        int errPos;
        if (!Configuration.CompareIgnoreDirsMasks.PrepareMasks(errPos))
        {
            TRACE_E("CMainWindow::CompareDirectories: CompareIgnoreDirsMasks is invalid."); // hrozi asi jen pri rucni editaci konfigurace v registry
            return;
        }
    }

    BeginStopRefresh(); // cmuchal si da pohov

    // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    //--- otevreme progress dialog
    BOOL displayDialogBox = (flags & COMPARE_DIRECTORIES_BYCONTENT) != 0 || // pro rychle akce nema smysl dialog zobrazovat
                            (flags & COMPARE_DIRECTORIES_SUBDIRS) != 0;
    BOOL displayProgressBar = (flags & COMPARE_DIRECTORIES_BYCONTENT) != 0;
    CCmpDirProgressDialog progressDlg(HWindow, displayProgressBar, &TaskBarList3);

    HWND hFocusedWnd = NULL;
    if (displayDialogBox)
    {
        // nastavime texty v progress dialogu
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
    BOOL canceled = FALSE;  // uzivatel stornoval operaci
    int foundDSTShifts = 0; // kolikrat se casy souboru lisily presne o jednu nebo dve hodiny, pricemz to byl jediny rozdil v uzivatelem vybranych kriteriich

    //--- melka kopie poli Files a Dirs; (musime pracovat nad kopii dat, protoze behem zobrazeneho progress dialogu
    //    by prekresleni casti panelu odhalilo, ze jsme zmenili razeni panelu nebo hejbali se selection)

    CFilesArray* leftFiles = GetShallowCopy(LeftPanel->Files);
    CFilesArray* leftDirs = GetShallowCopy(LeftPanel->Dirs);
    CFilesArray* rightFiles = GetShallowCopy(RightPanel->Files);
    CFilesArray* rightDirs = GetShallowCopy(RightPanel->Dirs);

    if (leftFiles != NULL && leftDirs != NULL && rightFiles != NULL && rightDirs != NULL)
    {
        // podarilo se vytvorit melke kopie
        //--- zjisteni FAT systemu kvuli presnosti porovnani lastWrite
        BOOL leftFAT = FALSE;
        BOOL rightFAT = FALSE;
        if (LeftPanel->Is(ptDisk))
        {
            char fileSystem[20];
            MyGetVolumeInformation(LeftPanel->GetPath(), NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, fileSystem, 20);
            leftFAT = StrNICmp(fileSystem, "FAT", 3) == 0; // FAT i FAT32 pouzivaji DOS cas (presnost jen 2 sekundy)
        }
        if (RightPanel->Is(ptDisk))
        {
            char fileSystem[20];
            MyGetVolumeInformation(RightPanel->GetPath(), NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, fileSystem, 20);
            rightFAT = StrNICmp(fileSystem, "FAT", 3) == 0; // FAT i FAT32 pouzivaji DOS cas (presnost jen 2 sekundy)
        }
        // zastavime nacitani ikon, aby nedochazelo ke konkurenci s porovnanim
        if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
            LeftPanel->SleepIconCacheThread();
        if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
            RightPanel->SleepIconCacheThread();
        //--- serazeni podle jmena
        if (LeftPanel->SortType != stName || LeftPanel->ReverseSort)
            SortFilesAndDirectories(leftFiles, leftDirs, stName, FALSE, Configuration.SortDirsByName);
        if (RightPanel->SortType != stName || RightPanel->ReverseSort)
            SortFilesAndDirectories(rightFiles, rightDirs, stName, FALSE, Configuration.SortDirsByName);
        //--- komparace + oznaceni
        BOOL getTotal;
        if (flags & COMPARE_DIRECTORIES_BYCONTENT)
            getTotal = TRUE; // TRUE: neoznacujeme rozdily, do promenne 'total' nascitavame velikost souboru k porovnani
        else
            getTotal = FALSE;  // FALSE: oznacujeme rozdily, porovnavame soubory podle obsahu
        CQuadWord total(0, 0); // celkovy pocet bajtu, ktery bude (v nejhorsim pripade) treba porovnat

        // pole 'dirSubTotal' obsahuje velikosti jednotlivych podadresaru (souboru obsazenych v nich)
        // pole se plni v prvnim pruchodu (getTotal == TRUE)
        // pouziva se ve druhem pruchodu (getTotal == FALSE): pokud je podadresar ruzny, vime kolik preskocit na total progressu
        TDirectArray<CQuadWord> dirSubTotal(max(1, min(leftDirs->Count, rightDirs->Count)), 1);
        int subTotalIndex; // index do pole dirSubTotal

    ONCE_MORE:
        // napred soubory z leveho a praveho adresare
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
                if (r < right->Count) // oba panely obsahuji soubory
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
                            int isDSTShift = 0; // 1 = casy prave porovnavane dvojice souboru se lisi presne o jednu nebo dve hodiny, 0 = nelisi (nebo se casy vubec neporovnavaji)

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
                                if (leftFile->Size == rightFile->Size) // test na velikost neni kryty nasledujici optimalizacni podminkou zamerne, aby doslo k oznaceni obou souboru a nevyskakovaly zbytecne DST warningy
                                {
                                    if (!selectLeft && !leftIsNewer || !selectRight && !rightIsNewer) // pokud jsou oba soubory spinave, nema smysl porovnavat je podle obsahu
                                    {
                                        // pokud maji oba soubory nulovou delku, musi byt obsahove shodne
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
                                                    selectLeft = TRUE; // pri chybe cteni souboru radsi dvojici oznacime jako by mela ruzny obsah (muze mit)
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
                                    // soubory maji ruznou delku, musi se lisit obsahem
                                    selectLeft = TRUE;
                                    selectRight = TRUE;
                                }
                            }

                            if (!getTotal)
                            {
                                if (leftIsNewerNoDSTShiftIgn && !selectLeft || rightIsNewerNoDSTShiftIgn && !selectRight)
                                    foundDSTShifts += isDSTShift; // pocitame jen casove rozdily souboru, ktere nejsou jiz oznacene z jineho duvodu (napr. kvuli rozdilu podle jineho kriteria) -- motivace: pokud se nemusi ukazat slozity warning ohledne DST, neukazujeme ho

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
                else // jen left
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
            else // jen right
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

        // Sal2.0 a TC porovnavaji bez adresaru tak, ze ignoruji i jejich jmena
        // lide nam to porad predhazovali, takze se budeme chovat stejne (situace
        // bez COMPARE_DIRECTORIES_ONEPANELDIRS)
        if ((flags & COMPARE_DIRECTORIES_SUBDIRS) || (flags & COMPARE_DIRECTORIES_ONEPANELDIRS))
        {
            // nyni porovname podadresare
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
                    if (r < right->Count) // oba panely obsahuji adresare
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

                                // stejna jmena, stejne atributy -> budeme zkoumat obsah podadresaru
                                // pokud je adresar vybran z predchoziho kola, nebudeme ho tupe znovu enumerovat (getTotal)
                                if ((flags & COMPARE_DIRECTORIES_SUBDIRS) && (!select) && leftDir->Selected == 0)
                                {
                                    // do promennych left/rightSubDir vlozime podcestu, ktera je
                                    // v pripade ptDisk vztazena k ceste v panelu a v pripade
                                    // ptZIPArchive k ceste k archivu
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
                                    CQuadWord lastTotal; // pokud bude podadresar ruzny, preskocime jeho velikost pomoci 'lastTotal' + 'dirSubTotal'
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
                                            select = TRUE; // select nastavime i v pripade getTotal; optimalizace, v druhem kole cely strom preskocime
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
                                            // total pricteme pouze pokud budeme cely strom porovnavat podle obsahu
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
                                        else // pokud neni 'canceled', byla zobrazena chybova hlaska a uzivatel chce pokracovat
                                        {
                                            if (!getTotal)
                                                select = TRUE;
                                        }
                                    }
                                }

                                if (select) // pozor, uziva se i v pripade getTotal == TRUE (optimalizace)
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
                    else // jen left
                    {
                        leftDir->Selected = 1;
                        identical = FALSE;
                        if (++l < left->Count)
                            leftDir = &left->At(l);
                    }
                }
                else // jen right
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

        //--- serazeni podle nastaveni
        if (LeftPanel->SortType != stName || LeftPanel->ReverseSort)
            SortFilesAndDirectories(leftFiles, leftDirs, LeftPanel->SortType, LeftPanel->ReverseSort, Configuration.SortDirsByName);
        if (RightPanel->SortType != stName || RightPanel->ReverseSort)
            SortFilesAndDirectories(rightFiles, rightDirs, RightPanel->SortType, RightPanel->ReverseSort, Configuration.SortDirsByName);

        //--- zavreme progress dialog
        if (displayDialogBox)
        {
            EnableWindow(HWindow, TRUE);
            HWND actWnd = GetForegroundWindow();
            BOOL activate = actWnd == progressDlg.HWindow || actWnd == HWindow;
            DestroyWindow(progressDlg.HWindow);
            if (activate && hFocusedWnd != NULL)
                SetFocus(hFocusedWnd);
        }

        // drive se selection shodila na zacatku comparu a prubezne nastavovala behem nej
        // pokud se v te dobe dorucila od icon readeru zprava o preklesleni ikonky, shodil
        // se danemu souboru/adresari dirty bit a nasledne na konci uz se neprekreslil
        // (zustal nevybrany, prestoze mel byt oznaceny)

        // nyni selection shodime az na konci a mezi zmenou selection stavu a volanim
        // RepaintListBox nedochazi k distribuci zprav, takze nemuze dojit k doruceni
        // zadosti o prekresleni ikonky

        // zrusime selection
        LeftPanel->SetSel(FALSE, -1);
        RightPanel->SetSel(FALSE, -1);

        // pokud nedoslo ke cancelu, preneseme oznaceni
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

    //--- destrukce pomocnych poli
    if (leftFiles != NULL)
        delete leftFiles;
    if (leftDirs != NULL)
        delete leftDirs;
    if (rightFiles != NULL)
        delete rightFiles;
    if (rightDirs != NULL)
        delete rightDirs;

    //--- oznaceni prislusnych souboru
    LeftPanel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    RightPanel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    PostMessage(LeftPanel->HWindow, WM_USER_SELCHANGED, 0, 0);  // sel-change notify
    PostMessage(RightPanel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify

    //--- povolime nacitani ikonek
    if (LeftPanel->UseSystemIcons || LeftPanel->UseThumbnails)
        LeftPanel->WakeupIconCacheThread();
    if (RightPanel->UseSystemIcons || RightPanel->UseThumbnails)
        RightPanel->WakeupIconCacheThread();

    //--- zobrazime konfirmaci pokud se objevily soubory lisici se presne o hodinu nebo dve (bud varujeme,
    //    ze vsechny soubory nemaji stejny cas nebo upozornujeme na to, ze se nalezeny rozdil da ignorovat
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

    //--- zobrazime rozsudek
    if (!resultAlreadyShown && !canceled && identical)
    {
        int messageID = (flags & COMPARE_DIRECTORIES_BYCONTENT) ? IDS_COMPAREDIR_ARE_IDENTICAL : IDS_COMPAREDIR_SEEMS_IDENTICAL;
        SalMessageBox(HWindow, LoadStr(messageID), LoadStr(IDS_COMPAREDIRSTITLE), MB_OK | MB_ICONINFORMATION);
    }

    // opet zvysime prioritu threadu, operace dobehla
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
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
        int size = Length + len + 1 + 256; // +256 znaku do foroty, at se tak casto nealokuje
        char* newBuf = (char*)realloc(Buffer, size);
        if (newBuf != NULL)
        {
            Buffer = newBuf;
            Allocated = size;
        }
        else // nedostatek pameti, smula...
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
