// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dlldefs.h"
#include "fileio.h"
#include "tar.h"
#include "deb/deb.h"

#include "tar.rh"
#include "tar.rh2"
#include "lang\lang.rh"

//
// ****************************************************************************
//
// Pomocne funkce
//
// ****************************************************************************
//

int TxtToShort(const unsigned char* txt, unsigned long& result)
{
    CALL_STACK_MESSAGE2("TxtToShort(%s, )", txt);
    result = 0;
    unsigned long i;
    for (i = 0; i < 4; i++)
    {
        if ((*txt >= '0' && *txt <= '9') || (tolower(*txt) >= 'a' && tolower(*txt) <= 'f'))
        {
            result *= 16;
            if (*txt >= '0' && *txt <= '9')
                result += *txt - '0';
            else
                result += tolower(*txt) - 'a' + 10;
            txt++;
        }
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return TAR_OK;
}

int TxtToLong(const unsigned char* txt, unsigned long& result)
{
    CALL_STACK_MESSAGE2("TxtToLong(%s, )", txt);
    result = 0;
    unsigned long i;
    for (i = 0; i < 8; i++)
    {
        if ((*txt >= '0' && *txt <= '9') || (tolower(*txt) >= 'a' && tolower(*txt) <= 'f'))
        {
            result *= 16;
            if (*txt >= '0' && *txt <= '9')
                result += *txt - '0';
            else
                result += tolower(*txt) - 'a' + 10;
            txt++;
        }
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return TAR_OK;
}

int TxtToQuad(const unsigned char* txt, CQuadWord& result)
{
    CALL_STACK_MESSAGE2("TxtToQuad(%s, )", txt);
    result = CQuadWord(0, 0);
    unsigned long i;
    for (i = 0; i < 8; i++)
    {
        if ((*txt >= '0' && *txt <= '9') || (tolower(*txt) >= 'a' && tolower(*txt) <= 'f'))
        {
            result <<= 4;
            if (*txt >= '0' && *txt <= '9')
                result += CQuadWord(*txt - '0', 0);
            else
                result += CQuadWord(tolower(*txt) - 'a' + 10, 0);
            txt++;
        }
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return TAR_OK;
}

// prevod z oktalove soustavy
BOOL FromOctalQ(const unsigned char* ptr, const int length, CQuadWord& result)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("FromOctalQ(, %d,)", length);
    int i = 0;
    while (i < length && isspace(*ptr))
    {
        i++;
        ptr++;
    }
    result.Set(0, 0);
    while (i < length && *ptr >= '0' && *ptr <= '7')
    {
        result.Value = (result.Value << 3) | (*ptr++ - '0');
        i++;
    }
    if (i < length && *ptr && !isspace(*ptr))
        return FALSE;
    return TRUE;
}

//
// ****************************************************************************
//
// SCommonHeader
//
// ****************************************************************************
//
SCommonHeader::SCommonHeader()
{
    Path = NULL;
    Name = NULL;
    FileInfo.Name = NULL;
    Initialize();
}

SCommonHeader::~SCommonHeader()
{
    Initialize();
}

void SCommonHeader::Initialize()
{
    if (Path != NULL)
        free(Path);
    Path = NULL;
    if (Name != NULL)
        free(Name);
    Name = NULL;
    if (FileInfo.Name != NULL)
        SalamanderGeneral->Free(FileInfo.Name);
    memset(&FileInfo, 0, sizeof(FileInfo));
    IsDir = FALSE;
    Finished = FALSE;
    Ignored = FALSE;
    Checksum.Set(0, 0);
    Mode.Set(0, 0);
    MTime.Set(0, 0);
    Format = e_Unknown;
    IsExtendedTar = FALSE;
}

//
// ****************************************************************************
//
// Public funkce
//
// ****************************************************************************
//

CArchive::CArchive(const char* fileName, CSalamanderForOperationsAbstract* salamander, DWORD offset, CQuadWord inputSize)
{
    CALL_STACK_MESSAGE2("CArchive::CArchive(%s, )", fileName);

    // inicializace
    Offset.Set(0, 0);
    Silent = 0;
    Ok = TRUE;
    Stream = NULL;
    SalamanderIf = salamander;
    if (fileName == NULL || salamander == NULL)
    {
        Ok = FALSE;
        return;
    }
    // otevreme vstupni soubor a zjistime, je-li kompresenej
    Stream = CDecompressFile::CreateInstance(fileName, offset, inputSize);
    // zkontrolujem stream
    if (Stream == NULL || !Stream->IsOk())
    {
        Ok = FALSE;
        return;
    }
}

CArchive::~CArchive()
{
    CALL_STACK_MESSAGE1("CArchive::~CArchive()");
    // uz muzeme zavrit archiv
    if (Stream != NULL)
        delete Stream;
}

BOOL CArchive::ListArchive(const char* prefix, CSalamanderDirectoryAbstract* dir)
{
    CALL_STACK_MESSAGE1("CArchive::ListArchive( )");

    if (!IsOk())
        return FALSE;

    // nejprve se pokusime detekovat archiv a nacist prvni header
    Silent = 0;
    Offset.Set(0, 0);
    SCommonHeader header;
    int ret = ReadArchiveHeader(header, TRUE);

    // pokud to neni podporovany format, vybalime jen vnejsi kompresi, pokud je
    if (ret == TAR_NOTAR)
        if (Stream->IsCompressed())
            return ListStream(dir);
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_UNKNOWN), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return FALSE;
        }

    if (ret != TAR_OK)
        return FALSE;

    if (header.Finished)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }

    // mame archiv, muzeme pokracovat - dekodujeme vsechny soubory z archivu
    for (;;)
    {
        // pokud to, co jsme nasli, neumime interpretovat, tak to ignorujeme
        if (!header.Ignored)
        {
            char path[2 * MAX_PATH];

            if (prefix)
            {
                strcpy(path, prefix);
                if (header.Path)
                    strcat(path, header.Path);
            }
            // a pridame bud novy soubor, nebo adresar
            if (!header.IsDir)
            {
                // TODO: pridat i user data s pozici souboru v archivu pro oddeleni stejne pojmenovanych souboru
                // je to soubor, pridame soubor
                if (!dir->AddFile(prefix ? path : header.Path, header.FileInfo, NULL))
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_FDATA), LoadStr(IDS_TARERR_TITLE),
                                                      MSGBOX_ERROR);
                    return FALSE;
                }
            }
            else
            {
                // TODO: pridat i user data s pozici souboru v archivu pro oddeleni stejne pojmenovanych souboru
                // je to adresar, pridame adresar
                if (!dir->AddDir(prefix ? path : header.Path, header.FileInfo, NULL))
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_FDATA), LoadStr(IDS_TARERR_TITLE),
                                                      MSGBOX_ERROR);
                    return FALSE;
                }
            }
            // nastavime na null, abychom nahodou nedealokovali predanou pamet
            header.FileInfo.Name = NULL;
        }

        // precteme data, abychom byli na dalsim souboru
        ret = WriteOutData(header, NULL, NULL, TRUE, FALSE);

        if (ret != TAR_OK)
        {
            // Patera 2004.03.02: Return TRUE if TAR file ended exactly at the end
            // of last stream
            return (ret == TAR_EOF) ? TRUE : FALSE;
        }

        // pripravime novy header pro dalsi kolo
        if (ReadArchiveHeader(header, FALSE) != TAR_OK)
            return FALSE;

        // mame konec archivu, koncime, jde jen o to jak
        if (header.Finished)
            return TRUE;
    }
}

BOOL CArchive::UnpackOneFile(const char* nameInArchive, const CFileData* fileData,
                             const char* targetPath, const char* newFileName)
{
    CALL_STACK_MESSAGE4("CArchive::UnpackOneFile(%s, , %s, %s)", nameInArchive, targetPath, newFileName);

    if (!IsOk())
        return FALSE;

    // nejprve se pokusime detekovat archiv a nacist prvni header
    Silent = 0;
    Offset.Set(0, 0);
    SCommonHeader header;
    int ret = ReadArchiveHeader(header, TRUE);
    // pokud to neni podporovany format, vybalime jen vnejsi kompresi, pokud je
    if (ret == TAR_NOTAR && Stream->IsCompressed())
        return UnpackStream(targetPath, FALSE, nameInArchive, NULL, newFileName);
    if (ret != TAR_OK)
        return FALSE;
    if (header.Finished)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    BOOL found = FALSE;
    // mame archiv, muzeme pokracovat - dekodujeme vsechny soubory z archivu
    for (;;)
    {
        found = !strcmp(header.Name, nameInArchive);
        // pokud, to co jsme nasli, neumime interpretovat, tak to ignorujeme, jinak vybalime soubor
        // What we don't support (everything except files & dirs) has zero size and Ignored set.
        // This new condition unlike the old one also extracts empty files.
        ret = WriteOutData(header, targetPath, newFileName ? newFileName : header.FileInfo.Name,
                           (header.Ignored || !found), FALSE);
        if (ret != TAR_OK)
        {
            if (ret == TAR_EOF)
            {
                if (found)
                    return TRUE;

                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            }
            return FALSE;
        }
        // pokud jsme nasli, muzem koncit
        if (found)
            return TRUE;
        // pripravime novy header pro dalsi kolo
        if (ReadArchiveHeader(header, FALSE) != TAR_OK)
            return FALSE;
        // mame konec archivu, koncime, jde jen o to jak
        if (header.Finished)
            return FALSE;
    }
}

// extrakce vybranych souboru
BOOL CArchive::UnpackArchive(const char* targetPath, const char* archiveRoot,
                             SalEnumSelection next, void* param)
{
    CALL_STACK_MESSAGE3("CArchive::UnpackArchive(%s, %s, , )", targetPath, archiveRoot);

    if (!IsOk())
        return FALSE;

    // inicializace vybalovanych jmen
    const char* curName;
    BOOL isDir;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    CNames names;
    while ((curName = next(NULL, 0, &isDir, &size, NULL, param, NULL)) != NULL)
    {
        if (archiveRoot != NULL && *archiveRoot != '\0')
        {
            char* tmpName = (char*)malloc(strlen(archiveRoot) + 1 + strlen(curName) + 1);
            if (tmpName == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                                  MSGBOX_ERROR);
                return FALSE;
            }
            strcpy(tmpName, archiveRoot);
            if (tmpName[strlen(tmpName) - 1] != '\\')
                strcat(tmpName, "\\");
            strcat(tmpName, curName);
            names.AddName(tmpName, isDir, NULL, NULL);
            free(tmpName);
        }
        else
            names.AddName(curName, isDir, NULL, NULL);
        totalSize = totalSize + size;
    }
    // kontrola volneho mista, predpokladam, ze TestFreeSpace vyhodi patricnou hlasku
    if (!SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                          targetPath, totalSize, LoadStr(IDS_TARERR_HEADER)))
        return FALSE;

    // a vlastni vybaleni podle jmen
    return DoUnpackArchive(targetPath, archiveRoot, names);
}

BOOL CArchive::UnpackWholeArchive(const char* mask, const char* targetPath)
{
    CALL_STACK_MESSAGE3("CArchive::UnpackWholeArchive(%s, %s)", mask, targetPath);

    if (!IsOk())
        return FALSE;

    // inicializace seznamu jmen k vybaleni podle zadaneho seznamu masek
    CNames names;
    char* tmp = (char*)malloc(strlen(mask) + 1);
    if (tmp == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
        return TAR_ERROR;
    }
    strcpy(tmp, mask);
    char* ptr = tmp + strlen(tmp) - 1;
    for (;;)
    {
        while (ptr > tmp && *ptr != ';')
            ptr--;
        if (*ptr == ';')
        {
            if (strlen(ptr + 1) > 0)
                names.AddName(ptr + 1, FALSE, NULL, NULL);
            *ptr = '\0';
            ptr--;
        }
        else if (strlen(ptr) > 0)
            names.AddName(ptr, FALSE, NULL, NULL);
        if (ptr <= tmp)
            break;
    }
    free(tmp);

    // a ted vlastni vybaleni
    return DoUnpackArchive(targetPath, NULL, names);
}

//
// ****************************************************************************
//
// Interni funkce
//
// ****************************************************************************
//

BOOL CArchive::DoUnpackArchive(const char* targetPath, const char* archiveRoot, CNames& names)
{
    CALL_STACK_MESSAGE3("CArchive::DoUnpackArchive(%s, %s, )", targetPath, archiveRoot);

    if (!IsOk())
        return FALSE;

    CQuadWord filePos, sizeDelta;
    filePos = Stream->GetStreamPos();
    // nejprve se pokusime detekovat archiv a nacist prvni header
    Silent = 0;
    Offset.Set(0, 0);
    SCommonHeader header;
    int ret = ReadArchiveHeader(header, TRUE);
    // pokud to neni podporovany format, vybalime jen vnejsi kompresi, pokud je
    if (ret == TAR_NOTAR && Stream->IsCompressed())
        return UnpackStream(targetPath, TRUE, NULL, &names, NULL);
    if (ret != TAR_OK)
        return FALSE;
    if (header.Finished)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // otevreme progress bar
    SalamanderIf->OpenProgressDialog(LoadStr(IDS_UNPACKPROGRESS_TITLE), FALSE, NULL, FALSE);
    SalamanderIf->ProgressSetTotalSize(Stream->GetStreamSize(), CQuadWord(-1, -1));
    // updatneme progress po precteni headeru
    sizeDelta = filePos;
    filePos = Stream->GetStreamPos();
    sizeDelta = filePos - sizeDelta;
    if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
    {
        // osetrime cancel
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    // mame archiv, muzeme pokracovat - dekodujeme vsechny soubory z archivu
    for (;;)
    {
        // zjistime, je-li to "nas" soubor
        BOOL found = names.IsNamePresent(header.Name);
        // jak se ma jmenovat vytvareny soubor? (relativni cesta)
        char* ptr = header.Name;
        if (found && !header.Ignored && archiveRoot != NULL)
            ptr += strlen(archiveRoot);

        // pokud to, co jsme nasli neumime interpretovat, tak to ignorujeme, jinak vybalime soubor
        // What we don't support (everything except files & dirs) has zero size and Ignored set.
        // This new condition unlike the old one also extracts empty files.
        ret = WriteOutData(header, targetPath, ptr, (header.Ignored || !found), TRUE);
        if (ret != TAR_OK)
        {
            SalamanderIf->CloseProgressDialog();
            return (ret == TAR_EOF) ? TRUE : FALSE;
        }

        // progress jsme posouvali pri rozbalovani, ted to jen vzit na vedomi
        filePos = Stream->GetStreamPos();
        // pripravime novy header pro dalsi kolo
        if (ReadArchiveHeader(header, FALSE) != TAR_OK)
        {
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // posuneme progress o precteny header
        sizeDelta = filePos;
        filePos = Stream->GetStreamPos();
        sizeDelta = filePos - sizeDelta;
        if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
        {
            // osetrime cancel
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // mame konec archivu, koncime
        if (header.Finished)
        {
            SalamanderIf->CloseProgressDialog();
            return TRUE;
        }
    }
}

void CArchive::MakeFileInfo(const SCommonHeader& header, char* arcfiledata, char* arcfilename)
{
    CALL_STACK_MESSAGE1("CArchive::MakeFileInfo( , , )");

    if (header.IsDir)
    {
        // u adresare nas tohle nezajima
        arcfiledata[0] = '\0';
        arcfilename[0] = '\0';
    }
    else
    {
        // filedata
        FILETIME ft;
        FileTimeToLocalFileTime(&header.FileInfo.LastWrite, &ft);
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        char date[50], time[50], number[50];
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
            sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
            sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        sprintf(arcfiledata, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, header.FileInfo.Size), date, time);
        // filename
        sprintf(arcfilename, "%s\\%s", Stream->GetArchiveName(), header.Name);
    }
}

int CArchive::WriteOutData(const SCommonHeader& header, const char* targetPath,
                           const char* targetName, BOOL simulate, BOOL doProgress)
{
    SLOW_CALL_STACK_MESSAGE5("CArchive::WriteOutData( , %s, %s, %d, %d)",
                             targetPath, targetName, simulate, doProgress);

    BOOL toSkip = TRUE;
    char* extractedName;
    HANDLE file;
    if (!simulate || doProgress)
    {
        // zkonstruujeme jmeno
        extractedName = (char*)malloc(strlen(targetPath) + 1 + strlen(targetName) + 1);
        if (extractedName == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        strcpy(extractedName, targetPath);
        if (extractedName[strlen(extractedName) - 1] != '\\')
            strcat(extractedName, "\\");
        strcat(extractedName, (targetName[0] == '\\' ? 1 : 0) + targetName);
        // vytvorime novy soubor
        char arcfiledata[500];
        char arcfilename[500];
        MakeFileInfo(header, arcfiledata, arcfilename);
        if (!simulate)
        {
            file = SalamanderSafeFile->SafeFileCreate(extractedName, GENERIC_WRITE, 0, FILE_ATTRIBUTE_NORMAL,
                                                      header.IsDir, SalamanderGeneral->GetMainWindowHWND(),
                                                      arcfilename, arcfiledata, &Silent, TRUE, &toSkip, NULL, 0, NULL, NULL);
            // pri jakemkoli problemu koncime
            if (file == INVALID_HANDLE_VALUE)
            {
                if (toSkip)
                    simulate = TRUE;
                else
                {
                    free(extractedName);
                    return TAR_ERROR;
                }
            }
        }
        // updatneme nazev souboru v progressu
        if (doProgress)
        {
            char progresstxt[1000];
            if (!toSkip)
                strcpy(progresstxt, LoadStr(IDS_UNPACKPROGRESS_TEXT));
            else
                strcpy(progresstxt, LoadStr(IDS_SKIPPROGRESS_TEXT));
            strcat(progresstxt, header.Name);
            SalamanderIf->ProgressDialogAddText(progresstxt, TRUE);
        }
    }
    unsigned long checksum = 0;
    CQuadWord size(0, 0);
    CQuadWord filePos, sizeDelta;
    filePos = Stream->GetStreamPos();
    // pokud jde o tar a header je extended, musime zpracovat extenze
    // TODO: pokud extended header obsabuje dlouhe jmeno, musime ho zpracovat
    //   u pri cteni headeru, tady uz jen ridke fajly
    // TODO: a vubec to bude chtit rozsirit zjistovani typu u taru
    if (header.IsExtendedTar)
    {
        // TODO: tohle bude chtit protestovat
        const TTarBlock* tarHeader;
        do
        {
            // nacteme dalsi blok
            tarHeader = (const TTarBlock*)Stream->GetBlock(BLOCKSIZE);
            if (tarHeader == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                if (!simulate)
                {
                    CloseHandle(file);
                    DeleteFile(extractedName);
                    free(extractedName);
                }
                return TAR_ERROR;
            }
            EArchiveFormat format;
            BOOL finished;
            if (!IsTarHeader(tarHeader->RawBlock, finished, format))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                if (!simulate)
                {
                    CloseHandle(file);
                    DeleteFile(extractedName);
                    free(extractedName);
                }
                return TAR_ERROR;
            }
            if (!simulate)
            {
                // TODO: Tady je potreba zpracovat extended headery podle jejich typu
                /*
        DWORD written;
        if (!WriteFile(file, buffer, toRead, &written, NULL) || written != toRead)
        {
          char message[1000];
          DWORD err = GetLastError();
          strcpy(message, LoadStr(IDS_TARERR_FWRITE));
          if (written != toRead)
            strcat(message, LoadStr(IDS_TARERR_WRSIZE));
          else
            strcat(message, SalamanderGeneral->GetErrorText(err));
          SalamanderGeneral->ShowMessageBox(message, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
          CloseHandle(file);
          DeleteFile(extractedName);
          free(extractedName);
          return TAR_ERROR;
        }
        */
            }
            // updatneme progress, pokud je to treba
            if (doProgress)
            {
                sizeDelta = filePos;
                filePos = Stream->GetStreamPos();
                sizeDelta = filePos - sizeDelta;
                // posuneme progress a osetrime cancel
                if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
                {
                    // osetrime cancel
                    if (!simulate)
                    {
                        CloseHandle(file);
                        DeleteFile(extractedName);
                        free(extractedName);
                    }
                    return TAR_ERROR;
                }
            }
            size += CQuadWord(BLOCKSIZE, 0);
            Offset += CQuadWord(BLOCKSIZE, 0);
        } while (tarHeader->SparseHeader.isextended);
    }
    // TODO: tady to bude chtit jeste zkontrolovat, jak je to s tou velikosti
    size.Set(0, 0);
    while (size < header.FileInfo.Size)
    {
        unsigned short toRead = (unsigned short)(header.FileInfo.Size - size > CQuadWord(BUFSIZE, 0) ? BUFSIZE : (header.FileInfo.Size - size).Value);
        const unsigned char* buffer = Stream->GetBlock(toRead);

        if (buffer == NULL)
        {
            if (!Stream->IsOk())
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            else
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_EOF), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            if (!simulate)
            {
                CloseHandle(file);
                DeleteFile(extractedName);
                free(extractedName);
            }
            return TAR_ERROR;
        }
        if (!simulate)
        {
            DWORD written;
            if (!WriteFile(file, buffer, toRead, &written, NULL) || written != toRead)
            {
                char message[1000];
                strcpy(message, LoadStr(IDS_TARERR_FWRITE));
                if (written != toRead)
                    strcat(message, LoadStr(IDS_TARERR_WRSIZE));
                else
                    strcat(message, SalamanderGeneral->GetErrorText(GetLastError()));
                SalamanderGeneral->ShowMessageBox(message, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                CloseHandle(file);
                DeleteFile(extractedName);
                free(extractedName);
                return TAR_ERROR;
            }
        }
        // updatneme progress, pokud je to treba
        if (doProgress)
        {
            sizeDelta = filePos;
            filePos = Stream->GetStreamPos();
            sizeDelta = filePos - sizeDelta;
            // posuneme progress a osetrime cancel
            if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
            {
                // osetrime cancel
                if (!simulate)
                {
                    CloseHandle(file);
                    DeleteFile(extractedName);
                    free(extractedName);
                }
                return TAR_ERROR;
            }
        }
        // pokud je to treba, pocitame checksum
        if (header.Format == e_CRCASCII)
        {
            unsigned long k;
            for (k = 0; k < toRead; k++)
                checksum += buffer[k];
        }
        size += CQuadWord(toRead, 0);
        Offset += CQuadWord(toRead, 0);
    }
    // mame-li co, zkontrolujeme checksumy
    if (header.Format == e_CRCASCII && !header.Ignored &&
        CQuadWord(checksum, 0) != header.Checksum)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_CHECKSUM), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        if (!simulate)
        {
            CloseHandle(file);
            DeleteFile(extractedName);
            free(extractedName);
        }
        return TAR_ERROR;
    }
    // nastavime vlastnosti u vytvoreneho souboru
    if (!simulate)
    {
        if (!header.IsDir)
        {
            SetFileTime(file, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite);
            if (!CloseHandle(file))
            {
                char buffer[1000];
                DWORD err = GetLastError();
                strcpy(buffer, LoadStr(IDS_TARERR_FWRITE));
                strcat(buffer, SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->ShowMessageBox(buffer, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                DeleteFile(extractedName);
                free(extractedName);
                return TAR_ERROR;
            }
        }
        SetFileAttributes(extractedName, header.FileInfo.Attr);
    }
    // jmeno uz neni potreba
    if (!simulate || doProgress)
        free(extractedName);

    // a zarovname vstup na cele bloky
    int ret = SkipBlockPadding(header);

    // updatneme progress, pokud je to treba
    if (doProgress)
    {
        sizeDelta = filePos;
        filePos = Stream->GetStreamPos();
        sizeDelta = filePos - sizeDelta;
        // posuneme progress a osetrime cancel
        if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
            return TAR_ERROR;
    }

    if (ret != TAR_OK)
    {
        if (ret == TAR_EOF)
        {
            // Patera 2004.03.02:
            // We're at the end of the TAR file just at the end of the last stream.
            // Such files are created by bugreporting system of Kerio Personal Firewall
        }
        else if (ret == TAR_PREMATURE_END)
        {
            // all other errors reported inside SkipBlockPadding()
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    return ret;
} /* CArchive::WriteOutData */

// preskoci nepouzity zbytek bloku na zacatek dalsiho, platneho
int CArchive::SkipBlockPadding(const SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CArchive::SkipBlockPadding( )");
    unsigned short pad;

    if (header.Format == e_CRCASCII || header.Format == e_NewASCII)
        pad = (unsigned short)((4 - (Offset.Value % 4)) % 4);
    else if (header.Format == e_Binary || header.Format == e_SwapBinary)
        pad = (unsigned short)((2 - (Offset.Value % 2)) % 2);
    else if (header.Format == e_TarPosix ||
             header.Format == e_TarOldGnu ||
             header.Format == e_TarV7)
        pad = (unsigned short)((BLOCKSIZE - (Offset.Value % BLOCKSIZE)) % BLOCKSIZE);
    else
        pad = 0;

    if (pad != 0)
    {
        unsigned short read;
        const unsigned char* buffer = Stream->GetBlock(pad, &read);

        if (buffer == NULL)
        {
            // TAR_EOF indicates there is nowhere to seek at all
            // TAR_PREMATURE_END indicates there were some bytes to skip but less than expected
            return read ? TAR_PREMATURE_END : TAR_EOF;
        }
        Offset += CQuadWord(pad, 0);
    }
    return TAR_OK;
} /* CArchive::SkipBlockPadding */

// nacte header bloku v archivu (s autodetekci typu)
int CArchive::ReadArchiveHeader(SCommonHeader& header, BOOL probe)
{
    SLOW_CALL_STACK_MESSAGE2("CArchive::ReadArchiveHeader(, %d)", probe);

    // radeji hned vycistime strukturu
    header.Initialize();
    // pokusime se nacist maximalni velikost - header taru
    unsigned short preRead = BLOCKSIZE;
    const unsigned char* buffer = Stream->GetBlock(preRead);

    if (buffer == NULL)
    {
        // asi nemame tolik dat, vezmem zavdek velikosti "magic" cisla cpio
        if (Stream->IsOk())
        {
            unsigned short available = 0;
            preRead = 6;

            buffer = Stream->GetBlock(preRead, &available);
            if (available < 6)
            {
                preRead = available;
                buffer = Stream->GetBlock(available);
                if (buffer)
                {
                    memset((char*)buffer + available, 0, 6 - available);
                }
            }
            if (!available)
            {
                // Patera 2005.04.28: Fix of bug #183 (phphomework-alpha2.tar.gz)
                // Not able to read anything: try some heuristics
                if ((Stream->GetStreamSize() == Stream->GetStreamPos()) && (Stream->GetStreamSize() > CQuadWord(0, 0)))
                {
                    // TAR: parsed everything; Looks like there is missing terminating empty block
                    header.Finished = TRUE;
                    return TAR_OK;
                }
                FILETIME lastWrite;
                CQuadWord fileSize;
                DWORD fileAttr;
                Stream->GetFileInfo(lastWrite, fileSize, fileAttr);
                if (fileSize == Offset)
                {
                    // TAR+GZIP: decompressed as many data as it is stated at the end
                    header.Finished = TRUE;
                    return TAR_OK;
                }
            }
        }
        // kdyz neprectem ani to, muzem se jit klouzat...
        if (buffer == NULL)
        {
            if (!Stream->IsOk())
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            else
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_EOF),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }

    // a zkontrolujem typ archivu (po tomto kroku jsou v header strukture platne polozky Format a pro tar i Finished)
    GetArchiveType(buffer, preRead, header);

    // File starting with 512 zeros is automatically recognized by IsTarHeader as finished e_TarPosix.
    // Unless the TAR contains just one empty sector, it is not a TAR file.
    if (probe && (header.Format == e_TarPosix) && header.Finished)
        header.Format = e_Unknown;

    // neznamy archiv - balime to
    if (header.Format == e_Unknown)
    {
        // vratime, co nam nepatri
        Stream->Rewind(preRead);
        if (!Stream->IsOk())
        {
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        // a odchod
        if (!probe)
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_UNKNOWN), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_NOTAR;
    }
    // pokud tar skoncil, balime...
    if (header.Finished)
        return TAR_OK;
    // pokud nepracujeme s tarem, musime vratit zbytek hlavicky
    if (header.Format != e_TarPosix &&
        header.Format != e_TarOldGnu &&
        header.Format != e_TarV7)
        Stream->Rewind(preRead - 6);

    // a nacteme header
    int ret;
    switch (header.Format)
    {
    case e_NewASCII:
    case e_CRCASCII:
        ret = ReadNewASCIIHeader(header);
        break;
    case e_OldASCII:
        ret = ReadOldASCIIHeader(header);
        break;
    case e_Binary:
    case e_SwapBinary:
        ret = ReadBinaryHeader(header);
        break;
    case e_TarPosix:
    case e_TarOldGnu:
    case e_TarV7:
        ret = ReadTarHeader(buffer, header);
        break;
    default:
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_FORMAT), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        ret = TAR_ERROR;
    }
    // pri chybe balime
    if (ret != TAR_OK)
        return ret;
    // zarovnej blok
    ret = SkipBlockPadding(header);
    if (ret != TAR_OK)
    {
        if ((ret == TAR_PREMATURE_END) || (ret == TAR_EOF))
        {
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        return ret;
    }

    // pro tar uz jsme detekci konce udelali, ted cpio ukonceni, se kterym jsme museli cekat az na nazev
    if (header.Format != e_TarPosix && header.Format != e_TarOldGnu &&
        header.Format != e_TarV7 && header.Name != NULL &&
        !strcmp((char*)header.Name, "TRAILER!!!"))
    {
        header.Finished = TRUE;
        return TAR_OK;
    }
    // preparsujeme cestu: odstranime "." a ".." a prevedeme vse na backslashe
    char* tmpName = (char*)malloc(strlen(header.Name) + 1);
    if (tmpName == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                          MSGBOX_ERROR);
        return TAR_ERROR;
    }
    const char* src = header.Name;
    char* dst = tmpName;
    while (*src != '\0')
    {
        switch (*src)
        {
        case '\\':
        case '/':
            // pokud narazime na slash, kopirujeme pokud to neni prvni znak cesty a pokud uz nejake nemame
            if (dst > tmpName && *(dst - 1) != '\\')
                *dst++ = '\\';
            src++;
            break;
        case '.':
            if (*(src + 1) == '.' && (*(src + 2) == '\\' || *(src + 2) == '/' || *(src + 2) == '\0'))
            {
                char* dstOld = dst;

                // dve tecky - zrusime jednu uroven adresare, pokud je co rusit
                if (dst > tmpName)
                    dst -= 2;
                while ((dst > tmpName) && (*dst != '\\'))
                    dst--;
                // testAIX.tar from martin.pobitzer@gmx.at created on AIX starts with path "../../empty"
                if (dst < dstOld)
                {
                    dst++;
                }
                src += 2;
                break;
            }
            else if (*(src + 1) == '\\' || *(src + 1) == '/' || *(src + 1) == '\0')
            {
                // jedna tecka - proste ignorujeme
                src++;
                break;
            }
            // jinak je to zacatek nazvu a nechame to propadnout do kopirovani
        default:
            // pokud narazime na pismeno, proste kopirujem az do slashe
            while (*src != '\0' && *src != '\\' && *src != '/')
                *dst++ = *src++;
            break;
        }
    }
    // a zakoncime cilovy retezec
    *dst = '\0';
    // vynechame invalidni nazvy
    if (tmpName[0] == '\0')
    {
        free(tmpName);
        header.Ignored = TRUE;
        return TAR_OK;
    }
    // vynechame zaverecne lomitko, ale zapamatujeme si ze tam bylo
    if (dst > tmpName && *(dst - 1) == '\\')
    {
        header.IsDir = TRUE;
        dst--;
        *dst = '\0';
    }

    // ulozime nazev misto stareho
    free(header.Name);
    header.Name = (char*)malloc(strlen(tmpName) + 1);
    if (header.Name == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                          MSGBOX_ERROR);
        free(tmpName);
        return TAR_ERROR;
    }
    strcpy(header.Name, tmpName);
    free(tmpName);
    // ted provedeme analyzu nazvu
    const char* ptr = header.Name + strlen(header.Name) - 1;
    // najdeme dalsi - oddelime nazev a cestu
    while (ptr > header.Name && *ptr != '\\')
        ptr--;
    if (ptr == header.Name)
    {
        // nenasli jsme, mame jen nazev
        header.Path = NULL;
        header.FileInfo.NameLen = strlen(header.Name);
        header.FileInfo.Name = SalamanderGeneral->DupStr(header.Name);
        if (header.FileInfo.Name == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    else
    {
        // nasli jsme, nakopirujeme nazev i cestu
        header.Path = (char*)malloc(ptr - header.Name + 1);
        if (header.Path == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        strncpy_s(header.Path, ptr - header.Name + 1, header.Name, _TRUNCATE);
        ptr++;
        header.FileInfo.NameLen = strlen(ptr);
        header.FileInfo.Name = SalamanderGeneral->DupStr(ptr);
        if (header.FileInfo.Name == NULL)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    // nastavime priponu
    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);
    if (sortByExtDirsAsFiles || !header.IsDir)
    {
        char* s = header.FileInfo.Name + header.FileInfo.NameLen - 1;
        while (s >= header.FileInfo.Name && *s != '.')
            s--;
        //    if (s > header.FileInfo.Name)   // ".cvspass" ve Windows je pripona ...
        if (s >= header.FileInfo.Name)
            header.FileInfo.Ext = s + 1;
        else
            header.FileInfo.Ext = header.FileInfo.Name + header.FileInfo.NameLen;
    }
    else
        header.FileInfo.Ext = header.FileInfo.Name + header.FileInfo.NameLen; // adresare nemaji pripony
    // provedeme konverzi informaci o souboru z UNIX do WIN formatu
    // datum a cas
    header.MTime.Value += 11644473600;      // 0 je 1.1.1601
    header.MTime.Value *= 1000 * 1000 * 10; // a prevedeme na desetiny mikrosekund
    header.FileInfo.LastWrite.dwLowDateTime = header.MTime.LoDWord;
    header.FileInfo.LastWrite.dwHighDateTime = header.MTime.HiDWord;
    // typ souboru od taru uz mame, ted jen cpio
    if (header.Format != e_TarPosix && header.Format != e_TarOldGnu &&
        header.Format != e_TarV7)
        if ((header.Mode.LoDWord & CP_IFMT) == CP_IFDIR)
            header.IsDir = TRUE;
        else if ((header.Mode.LoDWord & CP_IFMT) != CP_IFREG)
            header.Ignored = TRUE;
    // atributy
    header.FileInfo.Attr = FILE_ATTRIBUTE_ARCHIVE;
    if ((header.Mode.LoDWord & 0200) == 0)
        header.FileInfo.Attr |= FILE_ATTRIBUTE_READONLY;
    if (header.IsDir)
        header.FileInfo.Attr |= FILE_ATTRIBUTE_DIRECTORY;
    // a zbytek
    header.FileInfo.DosName = NULL;
    header.FileInfo.Hidden = header.FileInfo.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
    if (header.IsDir)
        header.FileInfo.IsLink = 0;
    else
        header.FileInfo.IsLink = SalamanderGeneral->IsFileLink(header.FileInfo.Ext);
    header.FileInfo.IsOffline = 0;
    // TODO: do PluginData nejak musime dostat jednoznacne urceni souboru, treba offset...
    header.FileInfo.PluginData = -1; // zbytecne, jen tak pro formu
    return TAR_OK;
} /* CArchive::ReadArchiveHeader */

// provede autodetekci typu bloku v archivu podle obsahu headeru
void CArchive::GetArchiveType(const unsigned char* buffer, const unsigned short preRead, SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CArchive::GetArchiveType(, %hu, )", preRead);

    header.Finished = FALSE;
    // porovname signaturu v bufferu se znamymi vzorky
    if (!strncmp((const char*)buffer, "070701", 6))
        header.Format = e_NewASCII;
    else if (!strncmp((const char*)buffer, "070702", 6))
        header.Format = e_CRCASCII;
    else if (!strncmp((const char*)buffer, "070707", 6))
        header.Format = e_OldASCII;
    else if ((*(unsigned short*)buffer == 070707) && (preRead >= 26))
        header.Format = e_Binary;
    else if (*(unsigned short*)buffer == 0707070)
        header.Format = e_SwapBinary;
    else if ((preRead >= BLOCKSIZE) &&
             IsTarHeader(buffer, header.Finished, header.Format))
        ;
    else
        header.Format = e_Unknown;
}

// precte jmeno souboru z headeru cpio archivu
int CArchive::ReadCpioName(unsigned long namesize, SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CArchive::ReadCpioName(%lu, )", namesize);
    // kontrola "rozumnosti" delky nazvu
    if (namesize > 10000)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // pripravime misto na nazev
    header.Name = (char*)malloc(namesize);
    if (header.Name == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // nacteme nazev
    const unsigned char* buffer = Stream->GetBlock((unsigned short)namesize);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // zkontrolujem, ze delka sedi
    if (buffer[namesize - 1] != '\0')
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    // napocitame ofset pro zarovnani
    Offset += CQuadWord(namesize, 0);
    // kopie nazvu do struktury
    strcpy(header.Name, (const char*)buffer);
    // a konec
    return TAR_OK;
}

int CArchive::ReadNewASCIIHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::ReadNewASCIIHeader( )");

    // nacti zbytek headeru
    const unsigned char* buffer = Stream->GetBlock(13 * 8);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }

    // napocitame ofset pro zarovnani
    Offset += CQuadWord(6 + 13 * 8, 0);
    // vytahneme velikost nazvu
    unsigned long namesize;
    if (TxtToLong(buffer + 11 * 8, namesize) != TAR_OK)
        return TAR_ERROR;
    // a vytahneme podrobnosti o souboru, ktere nas zajimaji
    if (TxtToQuad(buffer + 1 * 8, header.Mode) != TAR_OK)
        return TAR_ERROR;
    if (TxtToQuad(buffer + 5 * 8, header.MTime) != TAR_OK)
        return TAR_ERROR;
    if (TxtToQuad(buffer + 6 * 8, header.FileInfo.Size) != TAR_OK)
        return TAR_ERROR;
    if (TxtToQuad(buffer + 12 * 8, header.Checksum) != TAR_OK)
        return TAR_ERROR;
    // nacteme nazev
    return ReadCpioName(namesize, header);
}

int CArchive::ReadOldASCIIHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CArchive::ReadOldASCIIHeader( )");
    // nacti zbytek headeru
    const unsigned char* buffer = Stream->GetBlock(70);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }

    // napocitame ofset pro zarovnani
    Offset += CQuadWord(6 + 70, 0);
    // vytahneme velikost nazvu
    CQuadWord qnamesize;
    if (!FromOctalQ(buffer + 6 * 7 + 11, 6, qnamesize))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    unsigned long namesize;
    namesize = qnamesize.LoDWord;
    // a vytahneme podrobnosti o souboru, ktere nas zajimaji
    if (!FromOctalQ(buffer + 6 * 2, 6, header.Mode))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    if (!FromOctalQ(buffer + 6 * 7, 11, header.MTime))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    if (!FromOctalQ(buffer + 6 * 8 + 11, 11, header.FileInfo.Size))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }
    header.Checksum.Set(0, 0);

    // nacteme nazev
    return ReadCpioName(namesize, header);
}

int CArchive::ReadBinaryHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::ReadBinaryHeader( )");
    // nacti zbytek headeru
    const unsigned char* buffer = Stream->GetBlock(20);
    if (buffer == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        return TAR_ERROR;
    }

    // napocitame ofset pro zarovnani
    Offset += CQuadWord(6 + 20, 0);
    unsigned long namesize;
    if (header.Format == e_Binary)
    {
        // vytahneme velikost nazvu
        namesize = *(unsigned short*)(buffer + 2 * 7);
        header.Mode.Set(*(unsigned short*)(buffer + 2 * 0), 0);
        header.MTime.Set((*(unsigned short*)(buffer + 2 * 5) << 16) + *(unsigned short*)(buffer + 2 * 6), 0);
        header.FileInfo.Size.Set((*(unsigned short*)(buffer + 2 * 8) << 16) + *(unsigned short*)(buffer + 2 * 9), 0);
    }
    else
    {
        // vytahneme velikost nazvu
        if (!TxtToShort(buffer + 2 * 7, namesize))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        // a vytahneme podrobnosti o souboru, ktere nas zajimaji
        unsigned long mode;
        if (!TxtToShort(buffer + 2 * 0, mode))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        header.Mode.Set(mode, 0);
        if (!TxtToQuad(buffer + 2 * 5, header.MTime))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
        if (!TxtToQuad(buffer + 2 * 8, header.FileInfo.Size))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CPIOERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            return TAR_ERROR;
        }
    }
    header.Checksum.Set(0, 0);

    // nacteme nazev
    return ReadCpioName(namesize, header);
}

// detekuje platny TAR header
BOOL CArchive::IsTarHeader(const unsigned char* buffer, BOOL& finished, EArchiveFormat& format)
{
    SLOW_CALL_STACK_MESSAGE1("CArchive::IsTarHeader(, )");
    const TTarBlock* header = (const TTarBlock*)buffer;
    finished = FALSE;

    // zkontrolujeme checksum (signed i unsigned, je v tom bordel)
    long int signed_checksum = 0;
    long int unsigned_checksum = 0;
    int i;
    for (i = 0; i < sizeof(TPosixHeader); i++)
    {
        signed_checksum += (signed char)(header->RawBlock[i]);
        unsigned_checksum += (unsigned char)(header->RawBlock[i]);
    }
    // pokud je blok plny nul, je to konec archivu
    if (unsigned_checksum == 0)
    {
        // musime priradit aspon nejaky format, abychom meli platny vystup
        format = e_TarPosix;
        finished = TRUE;
        return TRUE;
    }
    // polozky checksumu musime zapocitavat jako mezery
    for (i = 0; i < 8; i++)
    {
        signed_checksum -= (signed char)(header->Header.chksum[i]);
        unsigned_checksum -= (unsigned char)(header->Header.chksum[i]);
    }
    int s = sizeof(header->Header.chksum);
    signed_checksum += ' ' * s;
    unsigned_checksum += ' ' * s;

    // kontrola, jestli je ok
    CQuadWord recorded_checksum;
    if (!FromOctalQ((const unsigned char*)header->Header.chksum, s, recorded_checksum))
        return FALSE;
    if (CQuadWord(signed_checksum, 0) != recorded_checksum &&
        CQuadWord(unsigned_checksum, 0) != recorded_checksum)
        return FALSE;
    // zjisti format archivu
    if (!strncmp(header->Header.magic, TMAGIC, TMAGLEN))
        format = e_TarPosix;
    else if (!strncmp(header->Header.magic, OLDGNU_MAGIC, OLDGNU_MAGLEN))
        format = e_TarOldGnu;
    else
        format = e_TarV7;
    return TRUE;
}

int CArchive::ReadTarHeader(const unsigned char* buffer, SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::ReadTarHeader(, )");

    // prvni blok s headerem uz mame nacteny a validovany
    const TTarBlock* tarHeader = (const TTarBlock*)buffer;
    Offset += CQuadWord(BLOCKSIZE, 0);

    for (;;)
    {
        // vytahneme velikost dat (TODO: v orig. taru je nula jen LNKTYPE, ostatni
        // se ctou. Zkontroluj ty ostatni, jestli je to koser)
        if (tarHeader->Header.typeflag == LNKTYPE ||
            tarHeader->Header.typeflag == SYMTYPE ||
            tarHeader->Header.typeflag == DIRTYPE)
            header.FileInfo.Size.Set(0, 0);
        else if (!FromOctalQ((const unsigned char*)tarHeader->Header.size, 1 + 12, header.FileInfo.Size))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }

        // zpracujeme dlouha GNU jmena
        if (tarHeader->Header.typeflag == GNUTYPE_LONGNAME ||
            tarHeader->Header.typeflag == GNUTYPE_LONGLINK)
        {
            // sanity checking
            if (header.FileInfo.Size > CQuadWord(10000, 0))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                return TAR_ERROR;
            }
            // linky ignorujem, bereme jen jmena skutecnych souboru
            if (tarHeader->Header.typeflag == GNUTYPE_LONGNAME)
            {
                if (header.Name != NULL)
                    free(header.Name);
                header.Name = (char*)malloc(header.FileInfo.Size.LoDWord + 1);
                if (header.Name == NULL)
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                                      MSGBOX_ERROR);
                    return TAR_ERROR;
                }
            }
            DWORD toread = header.FileInfo.Size.LoDWord;
            DWORD offs = 0;
            // data referenced by tarHeader is no longer valid once GetBlock is called
            char typeFlag = tarHeader->Header.typeflag;
            while (toread > 0)
            {
                const unsigned char* ptr = Stream->GetBlock(BLOCKSIZE);
                if (ptr == NULL)
                {
                    SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                      LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
                    return TAR_ERROR;
                }
                Offset += CQuadWord(BLOCKSIZE, 0);
                DWORD rest = BLOCKSIZE < toread ? BLOCKSIZE : toread;
                // linky ignorujem
                if (typeFlag == GNUTYPE_LONGNAME)
                    memcpy(header.Name + offs, ptr, rest);
                offs += rest;
                toread -= rest;
            }
            // a zakoncime nulou...
            if (typeFlag == GNUTYPE_LONGNAME)
                *(header.Name + offs) = '\0';
            // a nacteme dalsi blok pro dalsi zpracovani
            tarHeader = (const TTarBlock*)Stream->GetBlock(BLOCKSIZE);
            if (tarHeader == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                                  LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                return TAR_ERROR;
            }
            Offset += CQuadWord(BLOCKSIZE, 0);
            if (!IsTarHeader(tarHeader->RawBlock, header.Finished, header.Format))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
                return TAR_ERROR;
            }
            // a jedem znova dokola
            continue;
        }
        // pokud jsme nenacetli nejaky dlouhy nazev, vezmem ten z headeru
        if (header.Name == NULL)
        {
            // TODO: matchovani jmen v orig. TARu je bez prefixu. Nam by to asi vadit
            // nemelo, ale zkontrolovat by se to asi melo :-)
            char tmpName[101], tmpPrefix[156];
            strncpy_s(tmpName, tarHeader->Header.name, _TRUNCATE);
            // Old GNU header vyuzival polozku prefix jinak, tak ji nesmime cist
            if (header.Format != e_TarOldGnu)
                strncpy_s(tmpPrefix, tarHeader->Header.prefix, _TRUNCATE);
            else
                tmpPrefix[0] = '\0';
            size_t len = strlen(tmpName);
            if (tmpPrefix[0] != '\0')
                len += 1 + strlen(tmpPrefix);
            header.Name = (char*)malloc(len + 1);
            if (header.Name == NULL)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_MEMORY), LoadStr(IDS_TARERR_TITLE),
                                                  MSGBOX_ERROR);
                return TAR_ERROR;
            }
            if (tmpPrefix[0] != '\0')
            {
                strcpy(header.Name, tmpPrefix);
                strcat(header.Name, "/");
            }
            else
                header.Name[0] = '\0';
            strcat(header.Name, tmpName);
        }
        // predpokladame soubor
        header.IsDir = FALSE;
        // koukneme se na typ souboru
        switch (tarHeader->Header.typeflag)
        {
        case LNKTYPE:
        case SYMTYPE:
            // TODO: tar pro dos extrakti symlinky jako hardlinky a hardlinky
            //       jako kopie. Nebo muzem zkusit shortcuty...
        case CHRTYPE:
        case BLKTYPE:
        case FIFOTYPE:
        case CHRSPEC:
        case XHDTYPE:
        case XGLTYPE:
            // typy, se kterymi se nevime rady
            header.Ignored = TRUE;
            break;
        case AREGTYPE:
        case REGTYPE:
        case CONTTYPE: // podle zdrojaku taru je tohle brano jako fajl, buhvi proc...
            // soubory k rozbaleni
            header.IsDir = FALSE;
            header.Ignored = FALSE;
            break;
        case DIRTYPE:
            // adresare
            header.IsDir = TRUE;
            header.Ignored = FALSE;
            break;
        case GNUTYPE_VOLHDR:
        case GNUTYPE_MULTIVOL: // TODO: pokracovani souboru z predchoziho archivu...
        case GNUTYPE_NAMES:    // TODO: v originale se extrakti pres extract_mangle. Uz se to pry nepouziva, ale podivat se na to muzem...
            // informace nedulezite pro extrakci
            header.Ignored = TRUE;
            break;
        case GNUTYPE_DUMPDIR:
            // dump adresare v inkrementalnim archivu
            //   tar projede dump v archivu a odmazava fajly, co v nem
            //   nejsou. To asi pro nas nema smysl, takze nedelame nic.
            header.Ignored = TRUE;
            break;
        default:
            // error
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_BADSIG), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            header.Ignored = TRUE;
            break;
        }
        // nacteme informace o souboru
        if (!FromOctalQ((const unsigned char*)tarHeader->Header.mtime, sizeof(tarHeader->Header.mtime) + 1, header.MTime))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        if (!FromOctalQ((const unsigned char*)tarHeader->Header.mode, sizeof(tarHeader->Header.mode), header.Mode))
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_HEADER), LoadStr(IDS_TARERR_TITLE),
                                              MSGBOX_ERROR);
            return TAR_ERROR;
        }
        if (header.Format == e_TarOldGnu &&
            tarHeader->OldGnuHeader.isextended)
            header.IsExtendedTar = TRUE;
        // TODO: pokud je header extended necim jinym, nez sparse bloky,
        //   ale treba dlouhym jmenem, je to potreba zpracovat tady
        return TAR_OK;
    }
}

BOOL CArchive::GetStreamHeader(SCommonHeader& header)
{
    CALL_STACK_MESSAGE1("CArchive::GetStreamHeader( )");

    // vytvorime "fake" hlavicku streamu
    header.FileInfo.NameLen = strlen(Stream->GetOldName());
    header.FileInfo.Name = SalamanderGeneral->DupStr(Stream->GetOldName());
    header.Name = (char*)malloc(header.FileInfo.NameLen + 1);
    if (header.FileInfo.Name == NULL || header.Name == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    strcpy(header.FileInfo.Name, Stream->GetOldName());
    strcpy(header.Name, Stream->GetOldName());
    // nastavime priponu
    char* s = header.FileInfo.Name + header.FileInfo.NameLen - 1;
    while (s >= header.FileInfo.Name && *s != '.')
        s--;
    //  if (s > header.FileInfo.Name)   // ".cvspass" ve Windows je pripona ...
    if (s >= header.FileInfo.Name)
        header.FileInfo.Ext = s + 1;
    else
        header.FileInfo.Ext = header.FileInfo.Name + header.FileInfo.NameLen;
    // nastavime parametry shodne s nasim souborem
    Stream->GetFileInfo(header.FileInfo.LastWrite, header.FileInfo.Size,
                        header.FileInfo.Attr);
    if (!Stream->IsOk())
    {
        SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                          LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // a zbytek
    header.FileInfo.Hidden = header.FileInfo.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
    header.FileInfo.IsLink = SalamanderGeneral->IsFileLink(header.FileInfo.Ext);
    header.FileInfo.IsOffline = 0;
    header.FileInfo.PluginData = -1; // zbytecne, jen tak pro formu
    return TRUE;
} /* CArchive::GetStreamHeader */

BOOL CArchive::ListStream(CSalamanderDirectoryAbstract* dir)
{
    CALL_STACK_MESSAGE1("CArchive::ListStream( )");

    // pripravime strukturu s informacemi o fajlu
    SCommonHeader header;
    if (!GetStreamHeader(header))
        return FALSE;
    // u cisteho zipu je to vzdycky soubor, pridame soubor
    if (!dir->AddFile(NULL, header.FileInfo, NULL))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_FDATA), LoadStr(IDS_TARERR_TITLE),
                                          MSGBOX_ERROR);
        return FALSE;
    }
    // zabranime destruktoru v dealokaci
    header.FileInfo.Name = NULL;
    return TRUE;
}

BOOL CArchive::UnpackStream(const char* targetPath, BOOL doProgress,
                            const char* nameInArchive, CNames* names, const char* newName)
{
    CALL_STACK_MESSAGE3("CArchive::UnpackStream(%s, %d)", targetPath, doProgress);

    CQuadWord filePos, sizeDelta;
    if (doProgress)
    {
        filePos = Stream->GetStreamPos();
        // otevreme progress bar
        SalamanderIf->OpenProgressDialog(LoadStr(IDS_UNPACKPROGRESS_TITLE), FALSE, NULL, FALSE);
        SalamanderIf->ProgressSetTotalSize(Stream->GetStreamSize(), CQuadWord(-1, -1));
    }
    SCommonHeader header;
    if (!GetStreamHeader(header))
    {
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    if (!(nameInArchive != NULL && !strcmp(header.Name, nameInArchive)) &&
        !(names != NULL && names->IsNamePresent(header.Name)))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TARERR_NOTFOUND), LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    BOOL toSkip = TRUE;
    char* extractedName;
    HANDLE file;

    // zkostruujeme jmeno
    if (!newName)
        newName = header.Name;
    extractedName = (char*)malloc(strlen(targetPath) + 1 +
                                  strlen(newName) + 1);
    if (extractedName == NULL)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    strcpy(extractedName, targetPath);
    if (extractedName[strlen(extractedName) - 1] != '\\')
        strcat(extractedName, "\\");
    strcat(extractedName, (newName[0] == '\\' ? 1 : 0) + newName);
    // vytvorime novy soubor
    char arcfiledata[500];
    char arcfilename[500];
    MakeFileInfo(header, arcfiledata, arcfilename);
    file = SalamanderSafeFile->SafeFileCreate(extractedName, GENERIC_WRITE, 0, FILE_ATTRIBUTE_NORMAL,
                                              header.IsDir, SalamanderGeneral->GetMainWindowHWND(),
                                              arcfilename, arcfiledata, &Silent, TRUE, &toSkip, NULL, 0, NULL, NULL);
    // pri jakemkoli problemu koncime
    if (file == INVALID_HANDLE_VALUE)
    {
        SalamanderIf->CloseProgressDialog();
        free(extractedName);
        // pokud si uzivatel preje skip, nemame tu co delat
        if (toSkip)
            return TRUE;
        return FALSE;
    }
    // updatneme nazev souboru v progressu
    if (doProgress)
    {
        char progresstxt[1000];
        strcpy(progresstxt, LoadStr(IDS_UNPACKPROGRESS_TEXT));
        strcat(progresstxt, header.Name);
        SalamanderIf->ProgressDialogAddText(progresstxt, TRUE);
    }
    // polozka size v headeru nemusi byt platna, budem rozbalovat, dokud je co
    for (;;)
    {
        // precteme cely blok
        unsigned short read;
        const unsigned char* buffer = Stream->GetBlock(BUFSIZE, &read);
        // neni-li blok, vezmem co je
        if (buffer == NULL && read > 0)
            buffer = Stream->GetBlock(read);
        if (!Stream->IsOk())
        {
            SalamanderGeneral->ShowMessageBox(LoadErr(Stream->GetErrorCode(), Stream->GetLastErr()),
                                              LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            CloseHandle(file);
            DeleteFile(extractedName);
            free(extractedName);
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // pokud je stream v poradku a presto nic neda, jsme na konci
        if (buffer == NULL)
            break;
        // a zapisem to
        DWORD written;
        if (!WriteFile(file, buffer, read, &written, NULL) || written != read)
        {
            char message[1000];
            DWORD err = GetLastError();
            strcpy(message, LoadStr(IDS_TARERR_FWRITE));
            if (written != read)
                strcat(message, LoadStr(IDS_TARERR_WRSIZE));
            else
                strcat(message, SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->ShowMessageBox(message, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
            CloseHandle(file);
            DeleteFile(extractedName);
            free(extractedName);
            SalamanderIf->CloseProgressDialog();
            return FALSE;
        }
        // updatneme progress, pokud je to treba
        if (doProgress)
        {
            sizeDelta = filePos;
            filePos = Stream->GetStreamPos();
            sizeDelta = filePos - sizeDelta;
            // posuneme progress a osetrime cancel
            if (!SalamanderIf->ProgressAddSize((int)sizeDelta.Value, TRUE))
            {
                // osetrime cancel
                CloseHandle(file);
                DeleteFile(extractedName);
                free(extractedName);
                SalamanderIf->CloseProgressDialog();
                return FALSE;
            }
        }
    }
    // nastavime vlastnosti u vytvoreneho souboru
    SetFileTime(file, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite, &header.FileInfo.LastWrite);
    if (!CloseHandle(file))
    {
        char buffer[1000];
        DWORD err = GetLastError();
        strcpy(buffer, LoadStr(IDS_TARERR_FWRITE));
        strcat(buffer, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buffer, LoadStr(IDS_TARERR_TITLE), MSGBOX_ERROR);
        DeleteFile(extractedName);
        free(extractedName);
        SalamanderIf->CloseProgressDialog();
        return FALSE;
    }
    SetFileAttributes(extractedName, header.FileInfo.Attr);
    // jmeno uz neni potreba
    free(extractedName);
    SalamanderIf->CloseProgressDialog();
    return TRUE;
}

CArchiveAbstract* CreateArchive(LPCTSTR fileName, CSalamanderForOperationsAbstract* salamander)
{
    CArchiveAbstract* archive = new CDEBArchive(fileName, salamander);

    if (archive && archive->IsOk())
        return archive;
    delete archive;

    archive = new CArchive(fileName, salamander, 0, CQuadWord(0, 0));

    if (archive && archive->IsOk())
        return archive;
    delete archive;

    return NULL;
}
