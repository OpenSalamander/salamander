// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <stdio.h>
#include "TreeMap.FileData.CZDirectory.h"
#include "TreeMap.FileData.CZRoot.h"

void CZDirectory::TEST_ENUM(FILE* fileHandle)
{
    TCHAR name[512];
    TCHAR str[512];
    size_t strSize;
    int cnt = this->_files->GetCount();
    for (int i = 0; i < cnt; i++)
    {

        CZFile* f = this->_files->At(i);

        TCHAR const* sn = f->GetName();
        TCHAR* dn = name;
        //_tcscpy_s(name, sizeof(name)/sizeof(wchar_t), f->GetName());
        while (*sn != TEXT('\0'))
        {
            switch (*sn)
            {
            case '&':
                *dn++ = TEXT('&');
                *dn++ = TEXT('a');
                *dn++ = TEXT('m');
                *dn++ = TEXT('p');
                *dn++ = TEXT(';');
                break;
            case '"':
                *dn++ = TEXT('&');
                *dn++ = TEXT('q');
                *dn++ = TEXT('u');
                *dn++ = TEXT('o');
                *dn++ = TEXT('t');
                *dn++ = TEXT(';');
                break;
            default:
                *dn++ = *sn;
                break;
            }
            sn++;
        }
        *dn++ = TEXT('\0');

        if (f->IsDirectory())
        {
            //strSize = _stprintf_s(str, sizeof(str)/sizeof(TCHAR), TEXT("<folder name=\"%s\" size=\"%I64d\">\n"), name, f->GetSize());
            strSize = _stprintf(str, TEXT("<folder name=\"%s\" datasize=\"%I64d\" realsize=\"%I64d\" disksize=\"%I64d\">\n"), name, f->GetSizeEx(FILESIZE_DATA), f->GetSizeEx(FILESIZE_REAL), f->GetSizeEx(FILESIZE_DISK));
            if (fwrite(str, sizeof(wchar_t), strSize, fileHandle) != strSize)
            {
                _tprintf(TEXT("fwrite failed!\n"));
            }

            ((CZDirectory*)f)->TEST_ENUM(fileHandle);
            //strSize = _stprintf_s(str, sizeof(str)/sizeof(TCHAR), TEXT("</folder>\n"));
            strSize = _stprintf(str, TEXT("</folder>\n"));
            if (fwrite(str, sizeof(wchar_t), strSize, fileHandle) != strSize)
            {
                _tprintf(TEXT("fwrite failed!\n"));
            }
        }
        else
        {
            //strSize = _stprintf_s(str, sizeof(str)/sizeof(TCHAR), TEXT("<file name=\"%s\" size=\"%I64d\" />\n"), name, f->GetSize());
            strSize = _stprintf(str, TEXT("<file name=\"%s\" datasize=\"%I64d\" realsize=\"%I64d\" disksize=\"%I64d\" />\n"), name, f->GetSizeEx(FILESIZE_DATA), f->GetSizeEx(FILESIZE_REAL), f->GetSizeEx(FILESIZE_DISK));
            if (fwrite(str, sizeof(wchar_t), strSize, fileHandle) != strSize)
            {
                _tprintf(TEXT("fwrite failed!\n"));
            }
        }
    }
}

void CZDirectory::TEST()
{
    TCHAR str[512];
    size_t strSize;
    FILE* fileHandle;

    // Create an the xml file in text and Unicode encoding mode.
    if ((fileHandle = _tfopen(TEXT("_test.xml"), TEXT("wt+,ccs=UTF-8"))) == NULL) // C4996
                                                                                  // Note: _wfopen is deprecated; consider using _wfopen_s instead
    {
        _tprintf(TEXT("fopen failed!\n"));
        return;
    }

    // Write a string into the file.
    //wcscpy_s(str, sizeof(str)/sizeof(wchar_t), L"<root>\n");
    //strSize = _stprintf_s(str, sizeof(str)/sizeof(TCHAR), TEXT("<root path=\"%s\">\n"), this->_name);
    strSize = _stprintf(str, TEXT("<root path=\"%s\">\n"), this->_name);
    //strSize = wcslen(str);
    if (fwrite(str, sizeof(wchar_t), strSize, fileHandle) != strSize)
    {
        _tprintf(TEXT("fwrite failed!\n"));
    }

    TEST_ENUM(fileHandle);

    // Write a string into the file.
    //_tcscpy_s(str, sizeof(str)/sizeof(TCHAR), TEXT("</root>"));
    _tcscpy(str, TEXT("</root>"));
    strSize = _tcslen(str);
    if (fwrite(str, sizeof(TCHAR), strSize, fileHandle) != strSize)
    {
        _tprintf(TEXT("fwrite failed!\n"));
    }

    // Close the file.
    if (fclose(fileHandle))
    {
        _tprintf(TEXT("fclose failed!\n"));
    }
}

INT64 CZDirectory::PopulateDir(CWorkerThread* mythread, TCHAR* path, int pos, size_t pathsize)
{
    if (this->_root == NULL)
        Beep(1000, 100);
    if (pathsize < 2 * MAX_PATH)
        Beep(1000, 100);

    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    DWORD dwError;

    int dircount = 0;
    int filecount = 0;
    INT64 tsize = 0;

    int sortorder = this->_root->GetSortOrder();

    //prilepit aktualni nazev
    if ((pos + this->_namelen + 1) < pathsize)
    {
        if (pos && path[pos - 1] != TEXT('\\'))
        {
            path[pos] = TEXT('\\');
            pos++;
        }
        _tcscpy(path + pos, this->_name);
        pos += (int)this->_namelen;
    }
    if (!pos || path[pos - 1] != TEXT('\\'))
        path[pos++] = TEXT('\\');

    //zkontrolovat delku
    if (pos >= MAX_PATH)
    {
        //ERROR
        this->_root->Log(LOG_ERROR, TEXT("Path is too long."), this);
        return -1;
    }

    TCHAR* filepart = &path[pos]; //ukazatel na zacatek mista pro prilepeni nazvu souboru

    path[pos] = TEXT('*');
    path[pos + 1] = TEXT('\0');
    /*
	int radixHistorgram[2048 * 6];
	radixHistorgram[0] = 0;
	for (int i = 1; i < 2048 * 6; i++)
	{
		radixHistorgram[i] = radixHistorgram[i - 1] + 1;
	}
	path[MAX_PATH] = (char)radixHistorgram[2048];
*/
    hFind = FindFirstFile(path, &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        //ERROR
        this->_root->LogLastError(this);
        return -1;
    }
    else
    {
        DWORD lastTime = GetTickCount();
        do
        {
            if (FindFileData.cFileName[0] == '.' && (FindFileData.cFileName[1] == '\0' || (FindFileData.cFileName[1] == '.' && FindFileData.cFileName[2] == '\0')))
                continue;
            INT64 datasize = 0;
            INT64 realsize = 0;
            INT64 disksize = 0;

            CZFile* f = NULL;
            if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                {
                    f = new CZDirectory(this, FindFileData.cFileName, &FindFileData.ftCreationTime, &FindFileData.ftLastWriteTime);
                    datasize = ((CZDirectory*)f)->PopulateDir(mythread, path, pos, pathsize);
                    if (datasize < 0) //chyba!
                    {
                        //this->_root->Log(LOG_ERROR, TEXT("Negative size of directory contents."), f);
                        delete f;
                        f = NULL;
                        datasize = 0;
                        realsize = 0;
                        disksize = 0;
                    }
                    else if (datasize == 0) //vse ok, ale prazdne
                    {
                        this->_dircount++;
                        dircount++;
                        this->_dircount += ((CZDirectory*)f)->GetSubDirsCount(); //zapocitame prazdne, abychom meli stejne vysledky jako Explorer
                        this->_filecount += ((CZDirectory*)f)->GetSubFileCount();

                        delete f;
                        f = NULL;
                        datasize = 0;
                        realsize = 0;
                        disksize = 0;
                    }
                    else
                    {
                        this->_dircount++;
                        dircount++;
                        datasize = f->GetSizeEx(FILESIZE_DATA);
                        realsize = f->GetSizeEx(FILESIZE_REAL);
                        disksize = f->GetSizeEx(FILESIZE_DISK);
                        this->_dircount += ((CZDirectory*)f)->GetSubDirsCount();
                        this->_filecount += ((CZDirectory*)f)->GetSubFileCount();
                    }
                }
                else
                {
                    f = new CZDirectory(this, FindFileData.cFileName, &FindFileData.ftCreationTime, &FindFileData.ftLastWriteTime);
                    this->_root->Log(LOG_WARNING, TEXT("Ignoring Reparse Point."), f);
                    delete f;
                    f = NULL;
                }
            }
            else
            {
                datasize = ((INT64)FindFileData.nFileSizeHigh * ((INT64)(MAXDWORD) + 1)) + FindFileData.nFileSizeLow;
                if ((FindFileData.dwFileAttributes & (FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_COMPRESSED)) != 0)
                {
                    DWORD lo, hi;
                    _tcscpy(filepart, FindFileData.cFileName);
                    lo = GetCompressedFileSize(path, &hi);
                    if (lo == INVALID_FILE_SIZE)
                    {
                        //ErrorExit(FindFileData.cFileName);
                        realsize = datasize;
                    }
                    else
                    {
                        realsize = ((INT64)hi * ((INT64)(MAXDWORD) + 1)) + lo;
                    }
                }
                else
                {
                    realsize = datasize;
                }
                //zapocitame vzdy, abychom meli stejna cisla jako Explorer
                this->_filecount++;
                filecount++;
                if (datasize > 0)
                {
                    disksize = this->_root->GetDiskSize(realsize);

                    f = new CZFile(this, FindFileData.cFileName, datasize, realsize, disksize, &FindFileData.ftCreationTime, &FindFileData.ftLastWriteTime);
                    tsize += f->GetSizeEx(sortorder);
                }
            }
            if (datasize > 0)
            {
                this->_datasize += datasize;
                this->_realsize += realsize;
                this->_disksize += disksize;

                INT64 sortsize = f->GetSizeEx(sortorder);

                int fre = this->_files->Add(f);
                while (fre > 0)
                {
                    int parent = (fre - 1) / 2;
                    if (this->_files->At(parent)->GetSizeEx(sortorder) > sortsize) //pokud nadrazeny je vetsi, tak nesplnuje MIN-HEAP
                    {
                        this->_files->Copy(fre, parent);
                        fre = parent;
                    }
                    else
                    {
                        break;
                    }
                }
                this->_files->At(fre) = f;
            }
            //if (((filecount + dircount) > MAXREPORTEDFILES) || (GetTickCount() - lastTime > 500)) //bud mnoho souboru nebo ubehlo 0.5sec
            if ((GetTickCount() - lastTime > 250) && (filecount + dircount) > 0) //pokud ubehlo 0.25sec a naslo se alespon neco noveho
            {
                this->_root->IncStats(filecount, dircount, tsize);
                lastTime = GetTickCount();
                dircount = 0;
                filecount = 0;
                tsize = 0;
            }
        } while ((FindNextFile(hFind, &FindFileData) != 0) && (mythread == NULL || !mythread->Aborting()));

        this->_root->IncStats(filecount, dircount, tsize);

        dwError = GetLastError();
        FindClose(hFind);

        int cnt = this->_files->GetCount();
        for (int i = 1; i <= cnt; i++)
        {
            int end = cnt - i;
            CZFile* f = this->_files->At(end);
            this->_files->Copy(end, 0);
            int fre = 0;
            end--;
            while (fre * 2 + 1 <= end)
            {
                int child = fre * 2 + 1; //leva
                if ((child < end) && (this->_files->At(child)->GetSizeEx(sortorder) > this->_files->At(child + 1)->GetSizeEx(sortorder)))
                    child++;
                if (this->_files->At(child)->GetSizeEx(sortorder) < f->GetSizeEx(sortorder)) //pokud podrazeny je mensi, tak nesplnuje MIN-HEAP
                {
                    this->_files->Copy(fre, child);
                    fre = child;
                }
                else
                {
                    break;
                }
            }
            this->_files->At(fre) = f;
        }

        if (dwError != ERROR_NO_MORE_FILES)
        {
            //ERROR
            this->_root->LogError(this, dwError);
            return -1;
        }
    }

    return this->_realsize;
}