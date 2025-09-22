// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "dialogs.h"
#include "zip.h"
#include "pack.h"

//
// ****************************************************************************
// CFilesWindow
//

CPanelTmpEnumData::CPanelTmpEnumData()
{
    Indexes = NULL;
    CurrentIndex = 0;
    IndexesCount = 0;
    ZIPPath = NULL;
    Dirs = NULL;
    Files = NULL;
    ArchiveDir = NULL;
    WorkPath[0] = 0;
    EnumLastDir = NULL;
    EnumLastIndex = 0;
    EnumLastPath[0] = 0;
    EnumTmpFileName[0] = 0;
    DiskDirectoryTree = NULL;
    EnumLastDosPath[0] = 0;
    EnumTmpDosFileName[0] = 0;
    FilesCountReturnedFromWP = 0;
}

CPanelTmpEnumData::~CPanelTmpEnumData()
{
    if (DiskDirectoryTree != NULL)
        delete DiskDirectoryTree;
}

void CPanelTmpEnumData::Reset()
{
    CurrentIndex = 0;
    EnumLastDir = NULL;
    EnumLastIndex = -1;
    EnumLastPath[0] = 0;
    EnumTmpFileName[0] = 0;
    EnumLastDosPath[0] = 0;
    EnumTmpDosFileName[0] = 0;
    FilesCountReturnedFromWP = 0;
}

const char* _PanelSalEnumSelection(int enumFiles, const char** dosName, BOOL* isDir, CQuadWord* size,
                                   const CFileData** fileData, void* param, HWND parent, int* errorOccured)
{
    SLOW_CALL_STACK_MESSAGE2("_PanelSalEnumSelection(%d, , , , , , ,)", enumFiles);
    CPanelTmpEnumData* data = (CPanelTmpEnumData*)param;
    if (dosName != NULL)
        *dosName = NULL;
    if (isDir != NULL)
        *isDir = FALSE;
    if (size != NULL)
        *size = CQuadWord(0, 0);
    if (fileData != NULL)
        *fileData = NULL;

    if (enumFiles == -1)
    {
        data->Reset();
        return NULL;
    }

    static char errText[1000];
    const char* curZIPPath;
    if (data->DiskDirectoryTree == NULL)
        curZIPPath = data->ZIPPath;
    else
        curZIPPath = "";

ENUM_NEXT:

    if (data->CurrentIndex >= data->IndexesCount)
        return NULL;
    if (enumFiles == 0)
    {
        int i = data->Indexes[data->CurrentIndex++];
        BOOL localIsDir = i < data->Dirs->Count;
        if (isDir != NULL)
            *isDir = localIsDir;
        CFileData* f = &(localIsDir ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));
        if (localIsDir)
        {
            if (size != NULL)
                *size = data->ArchiveDir->GetDirSize(data->ZIPPath, f->Name);
        }
        else
        {
            if (size != NULL)
                *size = f->Size;
            data->FilesCountReturnedFromWP++; // jen pro sichr, kdyby nekdo menil 'enumFiles' mezi 0 a 3
        }
        if (fileData != NULL)
            *fileData = f;
        return f->Name;
    }
    else
    {
        if (data->EnumLastDir == NULL) // je potreba "otevrit" adresar
        {
            int i = data->Indexes[data->CurrentIndex];
            BOOL localIsDir = i < data->Dirs->Count;
            if (isDir != NULL)
                *isDir = localIsDir;
            CFileData* f = &(localIsDir ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));
            if (localIsDir)
            {
                int zipPathLen = (int)strlen(curZIPPath);
                if (zipPathLen + (zipPathLen > 0 ? 1 : 0) + f->NameLen >= MAX_PATH)
                { // prilis dlouha cesta
                    if (errorOccured != NULL)
                        *errorOccured = SALENUM_ERROR;
                    _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG), f->Name, curZIPPath);
                    if (parent != NULL &&
                        SalMessageBox(parent, errText, LoadStr(IDS_ERRORTITLE),
                                      MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                    {
                        if (errorOccured != NULL)
                            *errorOccured = SALENUM_CANCEL;
                        return NULL; // cancel, koncime
                    }
                    data->CurrentIndex++; // preskocime adresar s dlouhym nazvem a pokracujeme dalsim adresarem nebo souborem
                    goto ENUM_NEXT;
                }
                else
                {
                    memcpy(data->EnumLastPath, curZIPPath, zipPathLen);
                    if (zipPathLen > 0)
                        data->EnumLastPath[zipPathLen++] = '\\';
                    strcpy(data->EnumLastPath + zipPathLen, f->Name);
                    if (data->DiskDirectoryTree != NULL)
                    { // pokud se bude pouzivat EnumLastDosPath, bude zipPathLen == 0
                        strcpy(data->EnumLastDosPath, (f->DosName == NULL) ? f->Name : f->DosName);
                    }
                    if (data->DiskDirectoryTree == NULL)
                        data->EnumLastDir = data->ArchiveDir;
                    else
                        data->EnumLastDir = data->DiskDirectoryTree;
                    data->EnumLastDir = data->EnumLastDir->GetSalamanderDir(data->EnumLastPath, TRUE);
                    if (data->EnumLastDir == NULL)
                        return NULL; // nejspis adresar "..", jinak neocekavana chyba
                    data->EnumLastIndex = 0;
                    goto FIND_NEXT;
                }
            }
            else
            {
                if (size != NULL)
                {
                    *size = f->Size;
                    if (enumFiles == 3 && data->DiskDirectoryTree != NULL && (f->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                    { // zjistenou velikost cil. souboru linku musime vzit z data->DiskDirectoryTree
                        CFileData const* f2 = data->DiskDirectoryTree->GetFile(data->FilesCountReturnedFromWP);
                        if (f2 != NULL && strcmp(f2->Name, f->Name) == 0) // always true
                            *size = f2->Size;
                        else
                        {
                            TRACE_E("_PanelSalEnumSelection(): unexpected situation: file \"" << f->Name << "\" not found or different in data->DiskDirectoryTree");
                        }
                    }
                }
                data->CurrentIndex++;
                if (data->DiskDirectoryTree != NULL)
                {
                    if (dosName != NULL)
                        *dosName = (f->DosName == NULL) ? f->Name : f->DosName;
                }
                if (fileData != NULL)
                    *fileData = f;
                data->FilesCountReturnedFromWP++;
                return f->Name;
            }
        }
        else
        {
            data->EnumLastIndex++; // posuv na dalsi polozku

        FIND_NEXT: // najdeme dalsi soubor ve stromu

            while (1)
            {
                if (data->EnumLastDir->IsDirectory(data->EnumLastIndex)) // adresar -> sestup
                {
                    CFileData* f = data->EnumLastDir->GetDirEx(data->EnumLastIndex);
                    BOOL tooLong1 = strlen(data->EnumLastPath) + 1 + f->NameLen >= MAX_PATH;
                    BOOL tooLong2 = data->DiskDirectoryTree != NULL && strlen(data->EnumLastDosPath) + 1 +
                                                                               (f->DosName == NULL ? f->NameLen : strlen(f->DosName)) >=
                                                                           MAX_PATH;
                    if (tooLong1 || tooLong2)
                    { // prilis dlouha cesta
                        if (errorOccured != NULL)
                            *errorOccured = SALENUM_ERROR;
                        _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG),
                                    (tooLong1 || f->DosName == NULL ? f->Name : f->DosName),
                                    (tooLong1 ? data->EnumLastPath : data->EnumLastDosPath));
                        if (parent != NULL &&
                            SalMessageBox(parent, errText, LoadStr(IDS_ERRORTITLE),
                                          MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                        {
                            if (errorOccured != NULL)
                                *errorOccured = SALENUM_CANCEL;
                            return NULL; // cancel, koncime
                        }
                        data->EnumLastIndex++; // preskocime adresar s dlouhym nazvem a pokracujeme dalsim adresarem nebo souborem (nebo vystupem z adresare)
                    }
                    else
                    {
                        strcat(data->EnumLastPath, "\\");
                        strcat(data->EnumLastPath, f->Name);
                        if (data->DiskDirectoryTree != NULL)
                        {
                            strcat(data->EnumLastDosPath, "\\");
                            strcat(data->EnumLastDosPath, (f->DosName == NULL) ? f->Name : f->DosName);
                        }
                        data->EnumLastDir = data->EnumLastDir->GetSalamanderDir(data->EnumLastIndex);
                        data->EnumLastIndex = 0;
                    }
                }
                else
                {
                    if (data->EnumLastDir->IsFile(data->EnumLastIndex)) // soubor -> nalezeno
                    {
                        if (enumFiles == 2)
                            goto ENUM_NEXT; // soubory z podadresaru ne, staci podadresare

                        CFileData* f = data->EnumLastDir->GetFileEx(data->EnumLastIndex);
                        int zipPathLen = (int)strlen(curZIPPath);
                        BOOL tooLong1 = strlen(data->EnumLastPath) - (zipPathLen + (zipPathLen > 0 ? 1 : 0)) +
                                            1 + f->NameLen >=
                                        MAX_PATH;
                        BOOL tooLong2 = data->DiskDirectoryTree != NULL && strlen(data->EnumLastDosPath) + 1 +
                                                                                   (f->DosName == NULL ? f->NameLen : strlen(f->DosName)) >=
                                                                               MAX_PATH;
                        if (tooLong1 || tooLong2)
                        { // prilis dlouha cesta
                            if (errorOccured != NULL)
                                *errorOccured = SALENUM_ERROR;
                            _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG),
                                        (tooLong1 || f->DosName == NULL ? f->Name : f->DosName),
                                        (tooLong1 ? data->EnumLastPath + zipPathLen + (zipPathLen > 0 ? 1 : 0) : data->EnumLastDosPath));
                            if (parent != NULL &&
                                SalMessageBox(parent, errText, LoadStr(IDS_ERRORTITLE),
                                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                            {
                                if (errorOccured != NULL)
                                    *errorOccured = SALENUM_CANCEL;
                                return NULL; // cancel, koncime
                            }
                            data->EnumLastIndex++; // preskocime soubor s dlouhym nazvem a pokracujeme dalsim souborem (nebo vystupem z adresare)
                        }
                        else
                        {
                            if (isDir != NULL)
                                *isDir = FALSE;
                            if (size != NULL)
                                *size = f->Size;
                            strcpy(data->EnumTmpFileName, data->EnumLastPath + zipPathLen + (zipPathLen > 0 ? 1 : 0));
                            strcat(data->EnumTmpFileName, "\\");
                            strcat(data->EnumTmpFileName, f->Name);
                            if (data->DiskDirectoryTree != NULL)
                            {
                                strcpy(data->EnumTmpDosFileName, data->EnumLastDosPath); // pouziva-li se, je zipPathLen == 0
                                strcat(data->EnumTmpDosFileName, "\\");
                                strcat(data->EnumTmpDosFileName, (f->DosName == NULL) ? f->Name : f->DosName);
                                if (dosName != NULL)
                                    *dosName = data->EnumTmpDosFileName;
                            }
                            if (fileData != NULL)
                                *fileData = f;
                            return data->EnumTmpFileName;
                        }
                    }
                    else // jsme na konci adresare -> vystup
                    {
                        // rozdelime cestu na adresar a podadresar
                        const char* dir;
                        char* subDir = strrchr(data->EnumLastPath, '\\');
                        char* subDirDos = NULL;
                        if (data->DiskDirectoryTree != NULL)
                            subDirDos = strrchr(data->EnumLastDosPath, '\\');
                        if (subDir != NULL)
                        {
                            dir = data->EnumLastPath;
                            *subDir++ = 0;
                            if (data->DiskDirectoryTree != NULL)
                                *subDirDos++ = 0;
                        }
                        else // urcite jsme vystoupili ze stromu
                        {
                            dir = "";
                            subDir = data->EnumLastPath;
                            if (data->DiskDirectoryTree != NULL)
                                subDirDos = data->EnumLastDosPath;
                        }
                        // overime jestli uz jsme nevystoupili ven ze stromu
                        if (strlen(dir) == strlen(curZIPPath))
                        {
                            if (fileData != NULL) // najdeme CFileData pro podadresar, ktery opoustime
                            {
                                int i = data->Indexes[data->CurrentIndex];
                                if (i < data->Dirs->Count)
                                    *fileData = &data->Dirs->At(i);
                                else
                                    TRACE_E("Unexpected situation in _PanelSalEnumSelection.");
                            }

                            data->CurrentIndex++; // tento adresar jsme uz prosli
                            data->EnumLastDir = NULL;
                            data->EnumLastIndex = -1;

                            if (isDir != NULL)
                                *isDir = TRUE;
                            if (size != NULL)
                                *size = CQuadWord(0, 0);
                            if (data->DiskDirectoryTree != NULL)
                            {
                                if (dosName != NULL)
                                    *dosName = subDirDos;
                            }
                            return subDir; // pri vystupu vratime adresar
                        }
                        // dokoncime vystup a provedeme posun
                        if (data->DiskDirectoryTree == NULL)
                            data->EnumLastDir = data->ArchiveDir;
                        else
                            data->EnumLastDir = data->DiskDirectoryTree;
                        data->EnumLastDir = data->EnumLastDir->GetSalamanderDir(dir, TRUE);
                        data->EnumLastIndex = data->EnumLastDir->GetIndex(subDir);

                        if (fileData != NULL) // najdeme CFileData pro podadresar, ktery opoustime
                        {
                            *fileData = data->EnumLastDir->GetDirEx(data->EnumLastIndex);
                        }

                        if (isDir != NULL)
                            *isDir = TRUE;
                        if (size != NULL)
                            *size = CQuadWord(0, 0);
                        int zipPathLen = (int)strlen(curZIPPath);
                        strcpy(data->EnumTmpFileName, data->EnumLastPath + zipPathLen + (zipPathLen > 0 ? 1 : 0));
                        strcat(data->EnumTmpFileName, "\\");
                        strcat(data->EnumTmpFileName, subDir);
                        if (data->DiskDirectoryTree != NULL)
                        {
                            strcpy(data->EnumTmpDosFileName, data->EnumLastDosPath); // pouziva-li se, je zipPathLen == 0
                            strcat(data->EnumTmpDosFileName, "\\");
                            strcat(data->EnumTmpDosFileName, subDirDos);
                            if (dosName != NULL)
                                *dosName = data->EnumTmpDosFileName;
                        }
                        return data->EnumTmpFileName; // pri vystupu vratime adresar
                    }
                }
            }
        }
    }
    return NULL;
}

const char* WINAPI PanelSalEnumSelection(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                         const CFileData** fileData, void* param, int* errorOccured)
{
    CALL_STACK_MESSAGE_NONE
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;
    if (enumFiles == 3)
    {
        TRACE_E("PanelSalEnumSelection(): invalid parameter: enumFiles==3, changing to 1");
        enumFiles = 1;
    }
    return _PanelSalEnumSelection(enumFiles, NULL, isDir, size, fileData, param, parent, errorOccured);
}

void CFilesWindow::UnpackZIPArchive(CFilesWindow* target, BOOL deleteOp, const char* tgtPath)
{
    CALL_STACK_MESSAGE3("CFilesWindow::UnpackZIPArchive(, %d, %s)", deleteOp, tgtPath);
    if (Files->Count + Dirs->Count == 0)
        return;

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    BeginStopRefresh(); // cmuchal si da pohov

    //---  ziskani souboru a adresaru se kterymi budeme pracovat
    char subject[MAX_PATH + 100]; // text do Unpack dialogu (co se rozpakovava)
    char path[MAX_PATH + 200];
    char expanded[200];
    CPanelTmpEnumData data;
    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    data.IndexesCount = GetSelCount();
    if (data.IndexesCount > 1) // platne oznaceni
    {
        int files = 0; // pocet oznacenych souboru
        data.Indexes = new int[data.IndexesCount];
        if (data.Indexes == NULL)
        {
            TRACE_E(LOW_MEMORY);
            EndStopRefresh(); // ted uz zase cmuchal nastartuje
            return;
        }
        else
        {
            GetSelItems(data.IndexesCount, data.Indexes);
            int i = data.IndexesCount;
            while (i--)
            {
                BOOL isDir = data.Indexes[i] < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(data.Indexes[i]) : &Files->At(data.Indexes[i] - Dirs->Count);
                if (!isDir)
                    files++;
            }
        }
        // sestavime subject pro dialog
        ExpandPluralFilesDirs(expanded, 200, files, data.IndexesCount - files, epfdmNormal, FALSE);
    }
    else // bereme vybrany soubor nebo adresar
    {
        int index;
        if (data.IndexesCount == 0)
            index = GetCaretIndex();
        else
            GetSelItems(1, &index);

        if (subDir && index == 0)
        {
            EndStopRefresh(); // ted uz zase cmuchal nastartuje
            return;           // neni co delat
        }
        else
        {
            data.Indexes = new int[1];
            if (data.Indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                EndStopRefresh(); // ted uz zase cmuchal nastartuje
                return;
            }
            else
            {
                data.Indexes[0] = index;
                data.IndexesCount = 1;
                // sestavime subject pro dialog
                BOOL isDir = index < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                AlterFileName(path, f->Name, -1, Configuration.FileNameFormat, 0, index < Dirs->Count);
                lstrcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
            }
        }
    }
    sprintf(subject, LoadStr(deleteOp ? IDS_CONFIRM_DELETEFROMARCHIVE : IDS_COPYFROMARCHIVETO), expanded);
    CTruncatedString str;
    str.Set(subject, data.IndexesCount > 1 ? NULL : path);

    data.CurrentIndex = 0;
    data.ZIPPath = GetZIPPath();
    data.Dirs = Dirs;
    data.Files = Files;
    data.ArchiveDir = GetArchiveDir();
    data.EnumLastDir = NULL;
    data.EnumLastIndex = -1;

    char changesRoot[MAX_PATH]; // adresar, od ktereho pripadaji v uvahu zmeny na disku
    changesRoot[0] = 0;

    if (!deleteOp) // copy
    {
        //---  ziskani ciloveho adresare
        if (target != NULL && target->Is(ptDisk))
        {
            strcpy(path, target->GetPath());

            target->UserWorkedOnThisPath = TRUE; // default akce = prace s cestou v cilovem panelu
        }
        else
            path[0] = 0;

        CCopyMoveDialog dlg(HWindow, path, MAX_PATH, LoadStr(IDS_UNPACKCOPY), &str, IDD_COPYDIALOG,
                            Configuration.CopyHistory, COPY_HISTORY_SIZE, TRUE);

    _DLG_AGAIN:

        if (tgtPath != NULL || dlg.Execute() == IDOK)
        {
            if (tgtPath != NULL)
                lstrcpyn(path, tgtPath, MAX_PATH);
            UpdateWindow(MainWindow->HWindow);
            //---  u diskovych cest preklopime '/' na '\\' a zahodime zdvojene '\\'
            if (!IsPluginFSPath(path) &&
                (path[0] != 0 && path[1] == ':' ||                                             // cesty typu X:...
                 (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\') || // UNC cesty
                 Is(ptDisk) || Is(ptZIPArchive)))                                              // disk+archiv relativni cesty
            {                                                                                  // jde o diskovou cestu (absolutni nebo relativni) - otocime vsechny '/' na '\\' a zahodime zdvojene '\\'
                SlashesToBackslashesAndRemoveDups(path);
            }
            //---  uprava zadane cesty -> absolutni, bez '.' a '..'

            int len = (int)strlen(path);
            BOOL backslashAtEnd = (len > 0 && path[len - 1] == '\\'); // cesta konci na backslash -> nutne adresar
            BOOL mustBePath = (len == 2 && LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                               path[1] == ':'); // cesta typu "c:" musi byt i po expanzi cesta (ne soubor)

            int pathType;
            BOOL pathIsDir;
            char* secondPart;
            char textBuf[2 * MAX_PATH + 200];
            if (ParsePath(path, pathType, pathIsDir, secondPart, LoadStr(IDS_ERRORCOPY), NULL, NULL, MAX_PATH))
            {
                // misto konstrukce 'switch' pouzijeme 'if', aby fungovali 'break' + 'continue'
                if (pathType == PATH_TYPE_WINDOWS) // Windows cesta (disk + UNC)
                {
                    char newDirs[MAX_PATH]; // pokud se vytvari kvuli operaci adresar, pamatujeme si jeho jmeno (pri chybe ho muzeme smazat)
                    newDirs[0] = 0;

                    if (pathIsDir) // existujici cast cesty je adresar
                    {
                        if (*secondPart != 0) // je zde i neexistujici cast cesty
                        {
                            if (!backslashAtEnd && !mustBePath) // nova cesta musi koncit backslashem, jinak jde o operacni masku (nepodporovane)
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_UNPACK_OPMASKSNOTSUP), LoadStr(IDS_ERRORCOPY),
                                              MB_OK | MB_ICONEXCLAMATION);
                                if (tgtPath != NULL)
                                {
                                    UpdateWindow(MainWindow->HWindow);
                                    delete[] (data.Indexes);
                                    EndStopRefresh();
                                    return;
                                }
                                goto _DLG_AGAIN;
                            }

                            // vytvorime ty nove adresare
                            strcpy(newDirs, path);

                            if (Configuration.CnfrmCreatePath) // zeptame se, jestli se ma cesta vytvorit
                            {
                                BOOL dontShow = FALSE;
                                sprintf(textBuf, LoadStr(IDS_MOVECOPY_CREATEPATH), newDirs);

                                MSGBOXEX_PARAMS params;
                                memset(&params, 0, sizeof(params));
                                params.HParent = HWindow;
                                params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_HINT;
                                params.Caption = LoadStr(IDS_UNPACKCOPY);
                                params.Text = textBuf;
                                params.CheckBoxText = LoadStr(IDS_MOVECOPY_CREATEPATH_CNFRM);
                                params.CheckBoxValue = &dontShow;
                                BOOL cont = (SalMessageBoxEx(&params) != IDYES);
                                Configuration.CnfrmCreatePath = !dontShow;
                                if (cont)
                                {
                                    SalPathAddBackslash(path, MAX_PATH + 200);
                                    if (tgtPath != NULL)
                                    {
                                        UpdateWindow(MainWindow->HWindow);
                                        delete[] (data.Indexes);
                                        EndStopRefresh();
                                        return;
                                    }
                                    goto _DLG_AGAIN;
                                }
                            }

                            BOOL ok = TRUE;
                            char* st = newDirs + (secondPart - path);
                            char* firstSlash = NULL;
                            while (1)
                            {
                                BOOL invalidPath = *st != 0 && *st <= ' ';
                                char* slash = strchr(st, '\\');
                                if (slash != NULL)
                                {
                                    if (slash > st && (*(slash - 1) <= ' ' || *(slash - 1) == '.'))
                                        invalidPath = TRUE;
                                    *slash = 0;
                                    if (firstSlash == NULL)
                                        firstSlash = slash;
                                }
                                else
                                {
                                    if (*st != 0)
                                    {
                                        char* end = st + strlen(st) - 1;
                                        if (*end <= ' ' || *end == '.')
                                            invalidPath = TRUE;
                                    }
                                }
                                if (invalidPath || !CreateDirectory(newDirs, NULL))
                                {
                                    sprintf(textBuf, LoadStr(IDS_CREATEDIRFAILED), newDirs);
                                    SalMessageBox(HWindow, textBuf, LoadStr(IDS_ERRORCOPY), MB_OK | MB_ICONEXCLAMATION);
                                    ok = FALSE;
                                    break;
                                }
                                if (slash != NULL)
                                    *slash = '\\';
                                else
                                    break; // to byl posledni '\\'
                                st = slash + 1;
                            }

                            // zjisteni stare cesty (odkud se zakladaly nove adresare)
                            memcpy(changesRoot, path, secondPart - path);
                            changesRoot[secondPart - path] = 0;

                            if (!ok)
                            {
                                //---  refresh neautomaticky refreshovanych adresaru
                                // pokud selhalo vytvareni adresaru, provedeme hlaseni o zmenach hned (uzivatel muze
                                // zvolit priste uplne jinou cestu); nove vytvorena cesta se nerusi (temer dead-code)
                                MainWindow->PostChangeOnPathNotification(changesRoot, TRUE);

                                SalPathAddBackslash(path, MAX_PATH + 200);
                                if (tgtPath != NULL)
                                {
                                    UpdateWindow(MainWindow->HWindow);
                                    delete[] (data.Indexes);
                                    EndStopRefresh();
                                    return;
                                }
                                goto _DLG_AGAIN;
                            }
                            if (firstSlash != NULL)
                                *firstSlash = 0; // do newDirs prijde jmeno prvniho vytvoreneho adresare
                        }
                    }
                    else // prepis souboru - 'secondPart' ukazuje na jmeno souboru v ceste 'path'
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_UNPACK_OPMASKSNOTSUP), LoadStr(IDS_ERRORCOPY),
                                      MB_OK | MB_ICONEXCLAMATION);
                        if (backslashAtEnd || mustBePath)
                            SalPathAddBackslash(path, MAX_PATH + 200);
                        if (tgtPath != NULL)
                        {
                            UpdateWindow(MainWindow->HWindow);
                            delete[] (data.Indexes);
                            EndStopRefresh();
                            return;
                        }
                        goto _DLG_AGAIN;
                    }

                    // pokud se nevytvareji zadne nove adresare, zmeny zacinaji na cilove ceste
                    if (changesRoot[0] == 0)
                        strcpy(changesRoot, path);

                    //---  vlastni rozpakovani
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    if (PackUncompress(MainWindow->HWindow, this, GetZIPArchive(), PluginData.GetInterface(),
                                       path, GetZIPPath(), PanelSalEnumSelection, &data))
                    {                        // rozpakovani se povedlo
                        if (tgtPath == NULL) // pokud nejde o drag&drop (tam se odznacovani nedela)
                        {
                            SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                        }
                    }
                    else
                    {
                        if (newDirs[0] != 0)
                            RemoveEmptyDirs(newDirs);
                    }
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    if (GetForegroundWindow() == MainWindow->HWindow) // z nepochopitelnych duvodu mizi fokus z panelu pri drag&dropu do Explorera, vratime ho tam
                        RestoreFocusInSourcePanel();
                }
                else
                {
                    SalMessageBox(HWindow, LoadStr(IDS_UNPACK_ONLYDISK), LoadStr(IDS_ERRORCOPY),
                                  MB_OK | MB_ICONEXCLAMATION);
                    if (pathType == PATH_TYPE_ARCHIVE && (backslashAtEnd || mustBePath))
                    {
                        SalPathAddBackslash(path, MAX_PATH + 200);
                    }
                    if (tgtPath != NULL)
                    {
                        UpdateWindow(MainWindow->HWindow);
                        delete[] (data.Indexes);
                        EndStopRefresh();
                        return;
                    }
                    goto _DLG_AGAIN;
                }
            }
            else
            {
                if (tgtPath != NULL)
                {
                    UpdateWindow(MainWindow->HWindow);
                    delete[] (data.Indexes);
                    EndStopRefresh();
                    return;
                }
                goto _DLG_AGAIN;
            }

            //---  refresh neautomaticky refreshovanych adresaru
            // zmena na cilove ceste a jejich podadresarich (vytvareni novych adresaru a vypakovani
            // souboru/adresaru)
            MainWindow->PostChangeOnPathNotification(changesRoot, TRUE);
            // zmena v adresari, kde je umisteny archiv (pri unpacku by nemelo nastat, ale radsi refreshneme)
            MainWindow->PostChangeOnPathNotification(GetPath(), FALSE);
        }
    }
    else // delete
    {
        //---  zeptame se jestli to mysli vazne
        HICON hIcon = (HICON)HANDLES(LoadImage(Shell32DLL, MAKEINTRESOURCE(WindowsVistaAndLater ? 16777 : 161), // delete icon
                                               IMAGE_ICON, 32, 32, IconLRFlags));
        if (!Configuration.CnfrmFileDirDel ||
            CMessageBox(HWindow, MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_SILENT,
                        LoadStr(IDS_CONFIRM_DELETE_TITLE), &str, NULL,
                        NULL, hIcon, 0, NULL, NULL, NULL, NULL)
                    .Execute() == IDYES)
        {
            UpdateWindow(MainWindow->HWindow);
            //---  nalezeni neprazdnych adresaru - pripadne dotazy na mazani
            BOOL cancel = FALSE;
            if (Configuration.CnfrmNEDirDel)
            {
                int i;
                for (i = 0; i < data.IndexesCount; i++)
                {
                    if (data.Indexes[i] < Dirs->Count)
                    {
                        int dirsCount = 0;
                        int filesCount = 0;
                        GetArchiveDir()->GetDirSize(GetZIPPath(), Dirs->At(data.Indexes[i]).Name,
                                                    &dirsCount, &filesCount);
                        if (dirsCount + filesCount > 0)
                        {
                            char name[2 * MAX_PATH];
                            strcpy(name, GetZIPArchive());
                            if (GetZIPPath()[0] != 0)
                            {
                                if (GetZIPPath()[0] != '\\')
                                    strcat(name, "\\");
                                strcat(name, GetZIPPath());
                            }
                            strcat(name, "\\");
                            strcat(name, Dirs->At(data.Indexes[i]).Name);

                            char text[2 * MAX_PATH + 100];
                            sprintf(text, LoadStr(IDS_NONEMPTYDIRDELCONFIRM), name);
                            int res = SalMessageBox(HWindow, text, LoadStr(IDS_QUESTION),
                                                    MB_YESNOCANCEL | MB_ICONQUESTION);
                            if (res == IDCANCEL)
                            {
                                cancel = TRUE;
                                break;
                            }
                            if (res == IDNO)
                            {
                                memmove(data.Indexes + i, data.Indexes + i + 1, (data.IndexesCount - i - 1) * sizeof(int));
                                data.IndexesCount--;
                                i--;
                            }
                        }
                    }
                }
                if (data.IndexesCount == 0)
                    cancel = TRUE;
            }
            //---  vlastni mazani
            if (!cancel)
            {
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                if (PackDelFromArc(MainWindow->HWindow, this, GetZIPArchive(), PluginData.GetInterface(),
                                   GetZIPPath(), PanelSalEnumSelection, &data))
                {                                                   // mazani se povedlo
                    SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                }
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                //---  refresh neautomaticky refreshovanych adresaru
                // zmena v adresari, kde je umisteny archiv
                MainWindow->PostChangeOnPathNotification(GetPath(), FALSE);
            }
        }
        HANDLES(DestroyIcon(hIcon));
    }

    UpdateWindow(MainWindow->HWindow);
    delete[] (data.Indexes);

    //---  pokud je aktivni nejake okno salamandra, konci suspend mode
    EndStopRefresh();
}

void CFilesWindow::DeleteFromZIPArchive()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DeleteFromZIPArchive()");
    UnpackZIPArchive(NULL, TRUE); // jde temer o to same
}

BOOL _ReadDirectoryTree(HWND parent, char (&path)[MAX_PATH], char* name, CSalamanderDirectory* dir,
                        int* errorOccured, BOOL getLinkTgtFileSize, BOOL* errGetFileSizeOfLnkTgtIgnAll,
                        int* containsDirLinks, char* linkName)
{
    CALL_STACK_MESSAGE4("_ReadDirectoryTree(, %s, %s, , , %d, , ,)", path, name, getLinkTgtFileSize);
    char* end = path + strlen(path);
    char text[2 * MAX_PATH + 100];
    if ((end - path) + (*(end - 1) != '\\' ? 1 : 0) + strlen(name) + 2 >= _countof(path))
        return TRUE; // prilis dlouha cesta: budeme pokracovat, chybu ukazeme az pri enumeraci
    if (*(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    strcpy(end, name);
    strcat(end, "\\*");

    WIN32_FIND_DATA file;
    HANDLE find = HANDLES_Q(FindFirstFile(path, &file));
    *end = 0; // opravime cestu
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
        {
            if (errorOccured != NULL)
                *errorOccured = SALENUM_ERROR;
            strcpy(end, name);
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            *end = 0; // opravime cestu
            if (parent != NULL &&
                SalMessageBox(parent, text, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
                if (errorOccured != NULL)
                    *errorOccured = SALENUM_CANCEL;
                return FALSE; // user chce koncit
            }
        }
        return TRUE; // user chce pokracovat
    }
    else
    {
        strcpy(end, name);
        char* end2 = end + strlen(end);
        BOOL ok = TRUE;
        CFileData newF; // s temito polozkami uz nepracujeme
        if (dir != NULL)
        {
            newF.PluginData = -1; // -1 jen tak, ignoruje se
            newF.Association = 0;
            newF.Selected = 0;
            newF.Shared = 0;
            newF.Archive = 0;
            newF.SizeValid = 0;
            newF.Dirty = 0; // zbytecne, jen pro formu
            newF.CutToClip = 0;
            newF.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
            newF.IconOverlayDone = 0;
        }
        else
            memset(&newF, 0, sizeof(newF));
        BOOL testFindNextErr = TRUE;

        do
        {
            if (file.cFileName[0] == 0 ||
                file.cFileName[0] == '.' &&
                    (file.cFileName[1] == 0 || (file.cFileName[1] == '.' && file.cFileName[2] == 0)))
                continue; // "." a ".."

            static DWORD lastBreakCheck = 0;
            if (containsDirLinks != NULL && GetTickCount() - lastBreakCheck > 200)
            {
                lastBreakCheck = GetTickCount();
                if (UserWantsToCancelSafeWaitWindow())
                {
                    *containsDirLinks = 2; // po preruseni simulujeme chybu, aby se hledani okamzite ukoncilo
                    ok = FALSE;
                    testFindNextErr = FALSE;
                    break;
                }
            }

            if (dir != NULL)
            {
                newF.Size = CQuadWord(file.nFileSizeLow, file.nFileSizeHigh);

                BOOL cancel = FALSE;
                if (getLinkTgtFileSize &&
                    (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&   // jde o soubor
                    (file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) // jde o link
                {                                                                // u symlinku na soubor zjistime velikost ciloveho souboru
                    CQuadWord size;
                    if (SalPathAppend(path, file.cFileName, _countof(path)))
                    { // jen neni-li prilis dlouha cesta (pripadnou chybu ukazeme az pri enumeraci)
                        if (GetLinkTgtFileSize(parent, path, NULL, &size, &cancel, errGetFileSizeOfLnkTgtIgnAll))
                            newF.Size = size;
                        else
                        {
                            if (cancel && errorOccured != NULL)
                                *errorOccured = SALENUM_CANCEL;
                        }
                    }
                    *end2 = 0; // uvedeni 'path' do puvodniho stavu
                }

                newF.Name = !cancel ? DupStr(file.cFileName) : NULL;
                newF.DosName = NULL;
                if (cancel || newF.Name == NULL)
                {
                    ok = FALSE;
                    testFindNextErr = FALSE;
                    break;
                }
                newF.NameLen = strlen(newF.Name);
                if (!Configuration.SortDirsByExt && (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // adresar, jde jiste o disk
                {
                    newF.Ext = newF.Name + newF.NameLen; // adresare nemaji pripony
                }
                else
                {
                    newF.Ext = strrchr(newF.Name, '.');
                    if (newF.Ext == NULL)
                        newF.Ext = newF.Name + newF.NameLen; // ".cvspass" ve Windows je pripona ...
                                                             //        if (newF.Ext == NULL || newF.Ext == newF.Name) newF.Ext = newF.Name + newF.NameLen;
                    else
                        newF.Ext++;
                }

                if (file.cAlternateFileName[0] != 0)
                {
                    newF.DosName = DupStr(file.cAlternateFileName);
                    if (newF.DosName == NULL)
                    {
                        free(newF.Name);
                        ok = FALSE;
                        testFindNextErr = FALSE;
                        break;
                    }
                }

                newF.Attr = file.dwFileAttributes;
                newF.LastWrite = file.ftLastWriteTime;
                newF.Hidden = newF.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
                newF.IsOffline = newF.Attr & FILE_ATTRIBUTE_OFFLINE ? 1 : 0;
            }

            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) // adresar, jde jiste o disk
            {
                CSalamanderDirectory* salDir = NULL;
                if (dir != NULL)
                {
                    newF.IsLink = (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; // volume mount point nebo junction point = zobrazime adresar s link overlayem
                    BOOL addDirOK = dir->AddDir("", newF, NULL);
                    if (addDirOK)
                        salDir = dir->GetSalamanderDir(newF.Name, FALSE); // alokujeme sal-dir pro zapis
                    else
                    {
                        free(newF.Name);
                        if (newF.DosName != NULL)
                            free(newF.DosName);
                    }
                    if (salDir == NULL)
                    {
                        ok = FALSE;
                        testFindNextErr = FALSE;
                        break;
                    }
                }
                else // jen hledame 1. link na adresar
                {
                    if ((file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                    {
                        *containsDirLinks = 1; // po nalezeni simulujeme chybu, aby se hledani okamzite ukoncilo
                        *end2 = 0;
                        _snprintf_s(linkName, MAX_PATH, _TRUNCATE, "%s\\%s", path, file.cFileName); // klidne orizneme, jen text do hlasky
                        ok = FALSE;
                        testFindNextErr = FALSE;
                        break;
                    }
                }
                if (!_ReadDirectoryTree(parent, path, file.cFileName, salDir, errorOccured, getLinkTgtFileSize,
                                        errGetFileSizeOfLnkTgtIgnAll, containsDirLinks, linkName))
                {
                    ok = FALSE;
                    testFindNextErr = FALSE;
                    break;
                }
            }
            else // soubor
            {
                if (dir != NULL)
                {
                    if (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT)
                        newF.IsLink = 1; // pokud je soubor reparse-point (mozna vubec neni mozne) = zobrazime ho s link overlayem
                    else
                        newF.IsLink = IsFileLink(newF.Ext);

                    if (!dir->AddFile("", newF, NULL))
                    {
                        free(newF.Name);
                        if (newF.DosName != NULL)
                            free(newF.DosName);
                        ok = FALSE;
                        testFindNextErr = FALSE;
                        break;
                    }
                }
            }
        } while (FindNextFile(find, &file));
        DWORD err = GetLastError();
        HANDLES(FindClose(find));
        *end = 0; // opravime cestu

        if (testFindNextErr && err != ERROR_NO_MORE_FILES)
        {
            if (errorOccured != NULL)
                *errorOccured = SALENUM_ERROR;
            strcpy(end, name);
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            *end = 0; // opravime cestu
            if (parent != NULL &&
                SalMessageBox(parent, text, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
                if (errorOccured != NULL)
                    *errorOccured = SALENUM_CANCEL;
                return FALSE; // user chce koncit
            }
        }

        if (!ok)
        {
            if (errorOccured != NULL && *errorOccured == SALENUM_SUCCESS)
                *errorOccured = SALENUM_ERROR;
            return FALSE;
        }
    }
    return TRUE;
}

CSalamanderDirectory* ReadDirectoryTree(HWND parent, CPanelTmpEnumData* data, int* errorOccured,
                                        BOOL getLinkTgtFileSize, int* containsDirLinks, char* linkName)
{
    CALL_STACK_MESSAGE2("ReadDirectoryTree(, , , %d, ,)", getLinkTgtFileSize);
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;
    if (containsDirLinks != NULL)
        *containsDirLinks = 0;
    BOOL cancel = FALSE;
    if (data->CurrentIndex >= data->IndexesCount || data->WorkPath[0] == 0)
    {
        TRACE_E("Unexpected situation in ReadDirectoryTree().");
        if (errorOccured != NULL)
            *errorOccured = SALENUM_ERROR;
        return NULL; // neni co delat
    }

    BOOL errGetFileSizeOfLnkTgtIgnAll = parent == NULL; // tichy rezim = nezobrazovat chybu, vracet uspech

    CSalamanderDirectory* dir = containsDirLinks == NULL ? new CSalamanderDirectory(TRUE) : NULL;
    if (dir == NULL && containsDirLinks == NULL)
    {
        if (errorOccured != NULL)
            *errorOccured = SALENUM_ERROR;
        return NULL; // nedostatek pameti
    }

    int index = data->CurrentIndex;
    CFileData newF;
    if (dir != NULL)
    {
        newF.PluginData = -1; // -1 jen tak, ignoruje se
        newF.Association = 0;
        newF.Selected = 0;
        newF.Shared = 0;
        newF.Archive = 0;
        newF.SizeValid = 0;
        newF.Dirty = 0; // zbytecne, jen pro formu
        newF.CutToClip = 0;
        newF.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
        newF.IconOverlayDone = 0;
    }
    else
        memset(&newF, 0, sizeof(newF));
    while (index < data->IndexesCount)
    {
        int i = data->Indexes[index++];
        BOOL isDir = i < data->Dirs->Count;
        CFileData* f = &(isDir ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));

        // pro jistotu preskocime ".." (prisel bug-report z "1.6 beta 5" na toto tema; nechapu jak
        // to sem mohlo dojit)
        if (f->Name[0] == '.' && f->Name[1] == '.' && f->Name[2] == 0)
            continue;

        if (dir != NULL)
        {
            newF.Name = DupStr(f->Name);
            newF.DosName = NULL;
            if (newF.Name == NULL)
                goto RETURN_ERROR;
            if (f->DosName != NULL)
            {
                newF.DosName = DupStr(f->DosName);
                if (newF.DosName == NULL)
                    goto RETURN_ERROR;
            }
            newF.NameLen = f->NameLen;
            newF.Ext = newF.Name + (f->Ext - f->Name);
            newF.Size = f->Size;
            newF.Attr = f->Attr;
            newF.LastWrite = f->LastWrite;
            newF.Hidden = f->Hidden;
            newF.IsLink = f->IsLink;
            newF.IsOffline = f->IsOffline;
        }

        char path[MAX_PATH];
        if (isDir) // adresar
        {
            CSalamanderDirectory* salDir = NULL;
            if (dir != NULL)
            {
                BOOL addDirOK = dir->AddDir("", newF, NULL);
                if (addDirOK)
                {
                    newF.Name = NULL; // uz je v dir, nesmi se na nej volat free() - pri chybe nize
                    newF.DosName = NULL;
                    salDir = dir->GetSalamanderDir(f->Name, FALSE); // alokujeme sal-dir pro zapis
                }
                if (salDir == NULL)
                    goto RETURN_ERROR;
            }
            else
            {
                if ((f->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0) // nalezen link na adresar, koncime...
                {
                    *containsDirLinks = 1;
                    strcpy(path, data->WorkPath);
                    SalPathRemoveBackslash(path);
                    _snprintf_s(linkName, MAX_PATH, _TRUNCATE, "%s\\%s", path, f->Name); // klidne orizneme, jen text do hlasky
                    break;
                }
            }

            strcpy(path, data->WorkPath);
            if (!_ReadDirectoryTree(parent, path, f->Name, salDir, errorOccured, getLinkTgtFileSize,
                                    &errGetFileSizeOfLnkTgtIgnAll, containsDirLinks, linkName))
            {
                goto RETURN_ERROR;
            }
        }
        else // soubor
        {
            if (dir != NULL)
            {
                if (getLinkTgtFileSize && (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                { // u symlinku na soubor zjistime velikost ciloveho souboru
                    CQuadWord size;
                    strcpy(path, data->WorkPath);
                    if (SalPathAppend(path, newF.Name, _countof(path)))
                    { // jen neni-li prilis dlouha cesta (pripadnou chybu ukazeme az pri enumeraci)
                        if (GetLinkTgtFileSize(parent, path, NULL, &size, &cancel, &errGetFileSizeOfLnkTgtIgnAll))
                            newF.Size = size;
                        else
                        {
                            if (cancel && errorOccured != NULL)
                                *errorOccured = SALENUM_CANCEL;
                        }
                    }
                }
                if (cancel || !dir->AddFile("", newF, NULL))
                {
                RETURN_ERROR:

                    if (errorOccured != NULL && *errorOccured == SALENUM_SUCCESS)
                        *errorOccured = SALENUM_ERROR;
                    if (newF.Name != NULL)
                        free(newF.Name);
                    if (newF.DosName != NULL)
                        free(newF.DosName);
                    if (dir != NULL)
                        delete dir;
                    return NULL;
                }
            }
        }
    }
    return dir;
}

const char* WINAPI PanelEnumDiskSelection(HWND parent, int enumFiles, const char** dosName, BOOL* isDir,
                                          CQuadWord* size, DWORD* attr, FILETIME* lastWrite, void* param,
                                          int* errorOccured)
{
    CALL_STACK_MESSAGE_NONE
    CPanelTmpEnumData* data = (CPanelTmpEnumData*)param;
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;

    if (enumFiles == -1)
    {
        if (dosName != NULL)
            *dosName = NULL;
        if (isDir != NULL)
            *isDir = FALSE;
        if (size != NULL)
            *size = CQuadWord(0, 0);
        if (attr != NULL)
            *attr = 0;
        if (lastWrite != NULL)
            memset(lastWrite, 0, sizeof(FILETIME));
        data->Reset();
        return NULL;
    }

    if (enumFiles > 0)
    {
        if (data->DiskDirectoryTree == NULL)
        {
            data->DiskDirectoryTree = ReadDirectoryTree(parent, data, errorOccured, enumFiles == 3, NULL, NULL);
            if (data->DiskDirectoryTree == NULL)
                return NULL; // chyba, koncime
        }
        const CFileData* f = NULL;
        const char* ret = _PanelSalEnumSelection(enumFiles, dosName, isDir, size, &f, data, parent, errorOccured);
        if (ret != NULL)
        {
            if (f != NULL)
            {
                if (attr != NULL)
                    *attr = f->Attr;
                if (lastWrite != NULL)
                    *lastWrite = f->LastWrite;
            }
            else
            {
                if (attr != NULL)
                    *attr = 0;
                if (lastWrite != NULL)
                    memset(lastWrite, 0, sizeof(FILETIME));
            }
        }
        return ret;
    }
    else
    {
        if (data->CurrentIndex >= data->IndexesCount)
            return NULL;
        int i = data->Indexes[data->CurrentIndex++];
        if (isDir != NULL)
            *isDir = i < data->Dirs->Count;
        CFileData* f = &(i < data->Dirs->Count ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));
        if (dosName != NULL)
            *dosName = (f->DosName == NULL) ? f->Name : f->DosName;
        if (size != NULL)
            *size = f->Size;
        if (attr != NULL)
            *attr = f->Attr;
        if (lastWrite != NULL)
            *lastWrite = f->LastWrite;
        return f->Name;
    }
}

void CFilesWindow::Pack(CFilesWindow* target, int pluginIndex, const char* pluginName, int delFilesAfterPacking)
{
    CALL_STACK_MESSAGE4("CFilesWindow::Pack(, %d, %s, %d)", pluginIndex, pluginName, delFilesAfterPacking);
    if (Files->Count + Dirs->Count == 0)
        return;

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    BeginStopRefresh(); // cmuchal si da pohov

    //---  ziskani souboru a adresaru se kterymi budeme pracovat
    char subject[MAX_PATH + 100]; // text do Unpack dialogu (co se rozpakovava)
    char path[MAX_PATH];
    char text[1000];
    BOOL nameByItem;
    CPanelTmpEnumData data;
    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    data.IndexesCount = GetSelCount();
    char expanded[MAX_PATH + 100];
    int files = 0;             // pocet oznacenych souboru
    if (data.IndexesCount > 1) // platne oznaceni
    {
        nameByItem = FALSE;
        data.Indexes = new int[data.IndexesCount];
        if (data.Indexes == NULL)
        {
            TRACE_E(LOW_MEMORY);
            EndStopRefresh(); // ted uz zase cmuchal nastartuje
            return;
        }
        else
        {
            GetSelItems(data.IndexesCount, data.Indexes);
            int i = data.IndexesCount;
            while (i--)
            {
                BOOL isDir = data.Indexes[i] < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(data.Indexes[i]) : &Files->At(data.Indexes[i] - Dirs->Count);
                if (!isDir)
                    files++;
            }
        }
        // sestavime subject pro dialog
        ExpandPluralFilesDirs(expanded, MAX_PATH + 100, files, data.IndexesCount - files, epfdmNormal, FALSE);
    }
    else // bereme vybrany soubor nebo adresar
    {
        int index;
        if (data.IndexesCount == 0)
        {
            index = GetCaretIndex();
            nameByItem = TRUE; // pro kompatibilitu se Sal2.0
        }
        else
        {
            GetSelItems(1, &index);
            nameByItem = FALSE; // pro kompatibilitu se Sal2.0
        }

        // poznamka ke kompatibilite s 2.0
        // soucasna implementace je sice nelogicka, protoze se Salamander v pripade jednoho oznaceneho
        // souboru chova jinak nez v pripade jednoho focuseneho soubory, ale ma to jednu vyhodu:
        // uzivatel ma moznost volby predhazovaneho nazvu souboru

        if (subDir && index == 0)
        {
            EndStopRefresh(); // ted uz zase cmuchal nastartuje
            return;           // neni co delat
        }
        else
        {
            data.Indexes = new int[1];
            if (data.Indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                EndStopRefresh(); // ted uz zase cmuchal nastartuje
                return;
            }
            else
            {
                data.Indexes[0] = index;
                data.IndexesCount = 1;
                // sestavime subject pro dialog
                BOOL isDir = index < Dirs->Count;
                if (!isDir)
                    files = 1;
                CFileData* f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                AlterFileName(path, f->Name, -1, Configuration.FileNameFormat, 0, index < Dirs->Count);
                strcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
            }
        }
    }
    sprintf(subject, LoadStr(IDS_PACKTOARCHIVE), expanded);
    CTruncatedString str;
    str.Set(subject, data.IndexesCount > 1 ? NULL : path);

    data.CurrentIndex = 0;
    data.ZIPPath = GetZIPPath();
    data.Dirs = Dirs;
    data.Files = Files;
    data.ArchiveDir = GetArchiveDir();
    lstrcpyn(data.WorkPath, GetPath(), MAX_PATH);
    data.EnumLastDir = NULL;
    data.EnumLastIndex = -1;

    //---  pakujeme do noveho souboru, zeptame se na jeho jmeno
    char fileBuf[MAX_PATH];    // soubor, do ktereho budeme balit
    char fileBufAlt[MAX_PATH]; // alternativni nazev, ktery bude v comboboxu Pack dialogu

    if (nameByItem) // pokud jde jen o jeden soubor/adresar, prebira archiv jeho jmeno
    {
        char* ext = strrchr(path, '.');
        if (data.Indexes[0] < Dirs->Count || ext == NULL) // ".cvspass" ve Windows je pripona ...
                                                          //  if (data.Indexes[0] < Dirs->Count || ext == NULL || ext == path)
        {                                                 // podadresar nebo bez pripony
            strcpy(fileBuf, path);
            strcat(fileBuf, ".");
        }
        else
        {
            memcpy(fileBuf, path, ext + 1 - path);
            fileBuf[ext + 1 - path] = 0;
        }
    }
    else
    {
        // sestaveni default jmena archivu
        const char* end = GetPath() + strlen(GetPath());
        if (end > GetPath() && *(end - 1) == '\\')
            end--;
        const char* dir = end;
        char root[MAX_PATH];
        GetRootPath(root, GetPath());
        const char* min = GetPath() + strlen(root);
        while (dir > min && *(dir - 1) != '\\')
            dir--;
        if (dir < end)
        {
            memcpy(fileBuf, dir, end - dir);
            fileBuf[end - dir] = '.';
            fileBuf[end - dir + 1] = 0;
        }
        else
            strcpy(fileBuf, LoadStr(IDS_NEW_ARCHIVE));
    }

    if (pluginIndex != -1)
    {
        int i;
        for (i = 0; i < PackerConfig.GetPackersCount(); i++)
        {
            if (PackerConfig.GetPackerType(i) == -pluginIndex - 1)
            {
                PackerConfig.SetPreferedPacker(i);
                break;
            }
        }
        if (i == PackerConfig.GetPackersCount()) // hledany plugin nebyl nalezen
        {
            sprintf(subject, LoadStr(IDS_PLUGINPACKERNOTFOUND), pluginName);
            SalMessageBox(HWindow, subject, LoadStr(IDS_PACKTITLE), MB_OK | MB_ICONEXCLAMATION);
            delete[] (data.Indexes);
            EndStopRefresh(); // ted uz zase cmuchal nastartuje
            return;
        }
    }

    if (PackerConfig.GetPreferedPacker() == -1)
    { // pokud neni zadny prefered, vybereme prvni (aby useri blbe necumeli na prazdny combo)
        PackerConfig.SetPreferedPacker(0);
    }
    if (PackerConfig.GetPreferedPacker() != -1) // nutne i po Set (nemuselo se povest -> stale vraci -1)
        strcat(fileBuf, PackerConfig.GetPackerExt(PackerConfig.GetPreferedPacker()));

    strcpy(fileBufAlt, fileBuf);

    if (target->Is(ptDisk))
    {
        // na zaklade konfigurace upravime jedenu z cest tak, aby sla do target panelu
        char* buff = Configuration.UseAnotherPanelForPack ? fileBuf : fileBufAlt;

        if (Configuration.UseAnotherPanelForPack)
            target->UserWorkedOnThisPath = TRUE; // default akce = prace s cestou v cilovem panelu

        int l = (int)strlen(target->GetPath());
        if (l > 0 && target->GetPath()[l - 1] == '\\')
            l--;
        int ll = (int)strlen(buff);
        if (l + 2 + ll < MAX_PATH)
        {
            memmove(buff + l + 1, buff, ll + 1);
            buff[l] = '\\';
            memcpy(buff, target->GetPath(), l);
        }
    }

    // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
    char temporarySelected[MAX_PATH];
    temporarySelected[0] = 0;
    SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

    if (delFilesAfterPacking == 1)
        PackerConfig.Move = TRUE;
    if (delFilesAfterPacking == 0 || // Petr: zmena defaultu: mazani si musi user vzdy zapnout, je to moc nebezpecne
        delFilesAfterPacking == 2)
    {
        PackerConfig.Move = FALSE;
    }

    BOOL first = TRUE;

_PACK_AGAIN:

    CPackDialog dlg(HWindow, fileBuf, fileBufAlt, &str, &PackerConfig);

    // Od Windows Vista MS zavedli velice zadanou vec: quick rename implicitne oznaci pouze nazev bez tecky a pripony
    // stejny kod je jeste na druhem miste
    int selectionEnd = -1;
    if (first)
    {
        if (!Configuration.QuickRenameSelectAll)
        {
            const char* dot = strrchr(fileBuf, '.');
            if (dot != NULL && dot > fileBuf) // sice plati, ze ".cvspass" ve Windows je pripona, ale Explorer pro ".cvspass" oznacuje cele jmeno, tak to delame take
                                              //    if (dot != NULL)
                selectionEnd = (int)(dot - fileBuf);
            dlg.SetSelectionEnd(selectionEnd);
        }
        first = FALSE; // po chybe jiz dostaneme plny nazev souboru, takze ho oznacime komplet
    }

    if (dlg.Execute() == IDOK)
    {
        UpdateWindow(MainWindow->HWindow);
        //--- upravime jmeno archivu na full-name
        int errTextID;
        BOOL empty = FALSE;
        char nextFocus[MAX_PATH];
        nextFocus[0] = 0;
        if (SalGetFullName(fileBuf, &errTextID, Is(ptDisk) ? GetPath() : NULL, nextFocus))
        {
            //---  hledani linku na adresar ve zdroji baleni, nelze totiz kombinovat s "delete files after packing"
            BOOL performPack = TRUE;
            if (PackerConfig.Move)
            {
                int containsDirLinks = 0;
                SetCurrentDirectory(GetPath());
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help
                CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 3000, TRUE, NULL);

                // zkusime najit 1. link na adresar, pokud najdeme, simulujeme chybu pro ukonceni hledani
                char linkName[MAX_PATH];
                linkName[0] = 0;
                ReadDirectoryTree(NULL /* tichy rezim */, &data, NULL, FALSE, &containsDirLinks, linkName);

                DestroySafeWaitWindow();

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                SetCurrentDirectoryToSystem();

                // obsahuje link na adresar = nelze kombinovat s "delete files after packing":
                // neumim resit situaci, kdy by se nepodarilo zabalit jeden soubor z adresare (staci aby
                // byl soubor uzamceny nebo jsem k nemu nemel prava), kam jsem prosel pres link - smazat cely
                // link je spatne, protoze nebude videt, ze se neco nepovedlo zabalit a smazat vse az na
                // jeden soubor po pruchodu linku je taky spatne, protoze to ovlivni obsah puvodniho
                // adresare, kde to pak budou hlasit jako bugu, protoze je to necekane
                if (containsDirLinks == 1)
                {
                    _snprintf_s(text, _TRUNCATE, LoadStr(IDS_DELFILESAFTERPACKINGNOLINKS), linkName);
                    SalMessageBox(HWindow, text, LoadStr(IDS_PACKTITLE), MB_OK | MB_ICONEXCLAMATION);
                    PackerConfig.Move = FALSE;
                    goto _PACK_AGAIN;
                }
                if (containsDirLinks == 2) // uzivatel nacitani prerusil (ESC nebo kliknul na krizek wait okna)
                    performPack = FALSE;
            }

            //--- konfirmace na pridani (update) do existujiciho archivu
            if (performPack && Configuration.CnfrmAddToArchive && FileExists(fileBuf))
            {
                BOOL dontShow = !Configuration.CnfrmAddToArchive;

                char filesDirs[200];
                ExpandPluralFilesDirs(filesDirs, 200, files, data.IndexesCount - files, epfdmNormal, FALSE);

                char buff[3 * MAX_PATH];
                sprintf(buff, LoadStr(IDS_CONFIRM_ADDTOARCHIVE), "%s", filesDirs);

                char* namePart = strrchr(fileBuf, '\\');
                if (namePart != NULL)
                    namePart++;
                else
                    namePart = fileBuf;
                CTruncatedString str2;
                str2.Set(buff, namePart);

                char alias[200];
                sprintf(alias, "%d\t%s\t%d\t%s",
                        DIALOG_YES, LoadStr(IDS_CONFIRM_ADDTOARCHIVE_ADD),
                        DIALOG_NO, LoadStr(IDS_CONFIRM_ADDTOARCHIVE_OVER));
                CMessageBox msgBox(HWindow,
                                   MSGBOXEX_YESNOCANCEL | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT,
                                   LoadStr(IDS_QUESTION),
                                   &str2,
                                   LoadStr(IDS_CONFIRM_ADDTOARCHIVE_NOASK),
                                   &dontShow,
                                   NULL, 0, NULL, alias, NULL, NULL);
                int msgBoxRed = msgBox.Execute();
                performPack = (msgBoxRed == IDYES);
                if (msgBoxRed == IDNO) // OVERWRITE
                {
                    ClearReadOnlyAttr(fileBuf); // aby sel smazat ...
                    if (!DeleteFile(fileBuf))
                    {
                        DWORD err;
                        err = GetLastError();
                        SalMessageBox(HWindow, GetErrorText(err), LoadStr(IDS_ERROROVERWRITINGFILE), MB_OK | MB_ICONEXCLAMATION);
                        // propadneme do _PACK_AGAIN
                    }
                    else
                        performPack = TRUE;
                }
                Configuration.CnfrmAddToArchive = !dontShow;
                if (!performPack)
                    goto _PACK_AGAIN;
            }

            //---  vlastni zapakovani
            if (performPack)
            {
                SetCurrentDirectory(GetPath());
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                if (PackerConfig.ExecutePacker(this, fileBuf, PackerConfig.Move, GetPath(),
                                               PanelEnumDiskSelection, &data))
                { // zapakovani se povedlo
                    // if (nextFocus[0] != 0) strcpy(NextFocusName, nextFocus);
                    FocusFirstNewItem = TRUE; // fokusne i plug-inem prejmenovany archiv (treba SFX -> archiv.exe)

                    SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                }
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                SetCurrentDirectoryToSystem();

                //---  refresh neautomaticky refreshovanych adresaru
                // zmeny v adresari, ve kterem je umisten vznikajici archiv
                CutDirectory(fileBuf); // nemusi jit, ale tento pripad neresime (refresh navic nevadi)
                MainWindow->PostChangeOnPathNotification(fileBuf, FALSE);
                // presun z disku do archivu -> zmena i na disku (smazani souboru/adresaru)
                if (PackerConfig.Move)
                {
                    // zmeny v aktualnim adresari v panelu vcetne jeho podadresaru
                    MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
                }
            }
        }
        else
        {
            SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_PACKTITLE), MB_OK | MB_ICONEXCLAMATION);
            goto _PACK_AGAIN;
        }
    }

    // pokud jsme nejakou polozku vybrali, zase ji odvyberem
    UnselectItemWithName(temporarySelected);

    UpdateWindow(MainWindow->HWindow);
    delete[] (data.Indexes);

    //---  pokud je aktivni nejake okno salamandra, konci suspend mode
    EndStopRefresh();
}

void CFilesWindow::Unpack(CFilesWindow* target, int pluginIndex, const char* pluginName, const char* unpackMask)
{
    CALL_STACK_MESSAGE4("CFilesWindow::Unpack(, %d, %s, %s)", pluginIndex, pluginName, unpackMask);

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    int i = GetCaretIndex();
    if (i >= Dirs->Count && i < Dirs->Count + Files->Count)
    {
        BeginStopRefresh();

        CFileData* file = &Files->At(i - Dirs->Count);
        char path[MAX_PATH];
        char pathAlt[MAX_PATH]; // alternativni cesta, ktera bude v comboboxu UnPack dialogu
        char mask[MAX_PATH];
        char subject[MAX_PATH + 100];
        path[0] = 0;
        pathAlt[0] = 0;
        if (target->Is(ptDisk))
        {
            if (Configuration.UseAnotherPanelForUnpack)
            {
                target->UserWorkedOnThisPath = TRUE; // default akce = prace s cestou v cilovem panelu
                strcpy(path, target->GetPath());
            }
            else
                strcpy(pathAlt, target->GetPath());
        }
        char fileName[MAX_PATH];
        AlterFileName(fileName, file->Name, -1, Configuration.FileNameFormat, 0, FALSE);
        if (Configuration.UseSubdirNameByArchiveForUnpack)
        {
            int i2;
            for (i2 = 0; i2 < 2; i2++) // pro path a pathAlt
            {
                char* buff = (i2 == 0) ? path : pathAlt;
                int l = (int)strlen(buff);
                if (l + (l > 0 && buff[l - 1] != '\\' ? 1 : 0) + (file->Ext - file->Name) - (*file->Ext == 0 ? 0 : 1) < MAX_PATH)
                {
                    if (l > 0 && buff[l - 1] != '\\')
                        buff[l++] = '\\';
                    memcpy(buff + l, fileName, file->Ext - file->Name);
                    buff[l + (file->Ext - file->Name) - (*file->Ext == 0 ? 0 : 1)] = 0;
                }
                else
                    TRACE_E("CFilesWindow::Unpack(): too long path to add archive name!");
            }
        }
        if (unpackMask != NULL)
            lstrcpyn(mask, unpackMask, MAX_PATH);
        else
            strcpy(mask, "*.*");
        CTruncatedString str;
        str.Set(LoadStr(IDS_UNPACKARCHIVE), fileName);

        if (pluginIndex != -1)
        {
            int i2;
            for (i2 = 0; i2 < UnpackerConfig.GetUnpackersCount(); i2++)
            {
                if (UnpackerConfig.GetUnpackerType(i2) == -pluginIndex - 1)
                {
                    UnpackerConfig.SetPreferedUnpacker(i2);
                    break;
                }
            }
            if (i2 == UnpackerConfig.GetUnpackersCount()) // hledany plugin nebyl nalezen
            {
                sprintf(subject, LoadStr(IDS_PLUGINUNPACKERNOTFOUND), pluginName);
                SalMessageBox(HWindow, subject, LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
                EndStopRefresh(); // ted uz zase cmuchal nastartuje
                return;
            }
        }
        else // vyber unpackeru podle pripony
        {
            CMaskGroup tmpmask;
            if (UnpackerConfig.GetPreferedUnpacker() == -1)
            { // pokud neni zadny prefered, vybereme prvni (aby useri blbe necumeli na prazdny combo)
                UnpackerConfig.SetPreferedUnpacker(0);
            }
            if (UnpackerConfig.GetPreferedUnpacker() != -1)
            {
                tmpmask.SetMasksString(UnpackerConfig.GetUnpackerExt(UnpackerConfig.GetPreferedUnpacker()), TRUE);
            }
            else
            {
                tmpmask.SetMasksString("", TRUE);
            }
            int errpos = 0;
            tmpmask.PrepareMasks(errpos);
            if (!tmpmask.AgreeMasks(file->Name, file->Ext))
            {
                int i2;
                for (i2 = 0; i2 < UnpackerConfig.GetUnpackersCount(); i2++)
                {
                    tmpmask.SetMasksString(UnpackerConfig.GetUnpackerExt(i2), TRUE);
                    tmpmask.PrepareMasks(errpos);
                    if (tmpmask.AgreeMasks(file->Name, file->Ext))
                    {
                        UnpackerConfig.SetPreferedUnpacker(i2);
                        break;
                    }
                }
            }
        }
        BOOL delArchiveWhenDone = FALSE;
    DO_AGAIN:

        if (CUnpackDialog(HWindow, path, pathAlt, mask, &str, &UnpackerConfig, &delArchiveWhenDone).Execute() == IDOK)
        {
            UpdateWindow(MainWindow->HWindow);
            //--- upravime jmeno archivu na full-name
            int errTextID;
            char nextFocus[MAX_PATH];
            nextFocus[0] = 0;
            const char* text = NULL;
            if (!SalGetFullName(path, &errTextID, Is(ptDisk) ? GetPath() : NULL, nextFocus))
            {
                if (errTextID == IDS_EMPTYNAMENOTALLOWED)
                    strcpy(path, GetPath());
                else
                    text = LoadStr(errTextID);
            }
            if (text == NULL)
            {
                int l = (int)strlen(GetPath());
                if (l > 0 && GetPath()[l - 1] == '\\')
                    l--;
                memcpy(subject, GetPath(), l);
                sprintf(subject + l, "\\%s", file->Name);
                char newDir[MAX_PATH];
                if (CheckAndCreateDirectory(path, NULL, TRUE, NULL, 0, newDir, FALSE, TRUE))
                {
                    // spustime unpacker
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    CDynamicStringImp archiveVolumes;
                    if (!UnpackerConfig.ExecuteUnpacker(MainWindow->HWindow, this, subject, mask,
                                                        path, delArchiveWhenDone,
                                                        delArchiveWhenDone ? &archiveVolumes : NULL))
                    {
                        if (newDir[0] != 0)
                            RemoveEmptyDirs(newDir);
                    }
                    else // vypakovani se povedlo (zadny Cancel, Skip mozna byl)
                    {
                        if (delArchiveWhenDone && archiveVolumes.Length > 0)
                        {
                            char* name = archiveVolumes.Text;
                            BOOL skipAll = FALSE;
                            do
                            {
                                if (*name != 0)
                                {
                                    while (1)
                                    {
                                        ClearReadOnlyAttr(name); // aby sel smazat i read-only...
                                        if (!DeleteFile(name) && !skipAll)
                                        {
                                            DWORD err = GetLastError();
                                            if (err == ERROR_FILE_NOT_FOUND)
                                                break; // pokud uz user stihl soubor smazat sam, je vse OK
                                            int res = (int)CFileErrorDlg(MainWindow->HWindow, LoadStr(IDS_ERRORDELETINGFILE), name, GetErrorText(err)).Execute();
                                            if (res == IDB_SKIPALL)
                                                skipAll = TRUE;
                                            if (res == IDB_SKIPALL || res == IDB_SKIP)
                                                break;           // skip
                                            if (res == IDCANCEL) // cancel
                                            {
                                                name = archiveVolumes.Text + archiveVolumes.Length - 1;
                                                nextFocus[0] = 0; // nechame kurzor na archivu, neskocime na adresar s vybalenym archivem
                                                break;
                                            }
                                            // IDRETRY nechame zkusit dalsi pruchod cyklem
                                        }
                                        else
                                            break; // smazano
                                    }
                                }
                                name = name + strlen(name) + 1;
                            } while (name - archiveVolumes.Text < archiveVolumes.Length);
                        }
                        if (nextFocus[0] != 0)
                        {
                            strcpy(NextFocusName, nextFocus);
                            PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0); // jde o adresar, musi byt
                        }
                    }
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    //---  refresh neautomaticky refreshovanych adresaru
                    // zmena v adresari, kde je umisteny archiv (pri unpacku by nemelo nastat, ale
                    // radsi refreshneme)
                    MainWindow->PostChangeOnPathNotification(GetPath(), FALSE);
                    if (newDir[0] != 0) // na ceste se vytvarely nejake nove podadresare
                    {
                        CutDirectory(newDir); // melo by jit vzdy (cesta k prvnimu nove vytvorenemu adresari)
                        // zmeny v adresari, kde byl vytvoren prvni novy podadresar
                        MainWindow->PostChangeOnPathNotification(newDir, TRUE);
                    }
                    else
                    {
                        // zmeny v adresari, kam se vypakovavalo
                        MainWindow->PostChangeOnPathNotification(path, TRUE);
                    }
                }
                else
                {
                    if (newDir[0] != 0)
                    {
                        CutDirectory(newDir); // melo by jit vzdy (cesta k prvnimu nove vytvorenemu adresari)

                        //---  refresh neautomaticky refreshovanych adresaru
                        // pokud selhalo vytvareni adresaru, provedeme hlaseni o zmenach hned (uzivatel muze
                        // zvolit priste uplne jinou cestu); nove vytvorena cesta se nerusi (temer dead-code)
                        MainWindow->PostChangeOnPathNotification(newDir, TRUE);
                    }
                    goto DO_AGAIN;
                }
            }
            else
            {
                SalMessageBox(HWindow, text, LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
                goto DO_AGAIN;
            }
        }
        UpdateWindow(MainWindow->HWindow);

        //---  pokud je aktivni nejake okno salamandra, konci suspend mode
        EndStopRefresh();
    }
}

// countSizeMode - 0 normalni vypocet, 1 vypocet pro vybranou polozku, 2 vypocet pro vsechny podadresare
void CFilesWindow::CalculateOccupiedZIPSpace(int countSizeMode)
{
    CALL_STACK_MESSAGE2("CFilesWindow::CalculateOccupiedZIPSpace(%d)", countSizeMode);
    if (Is(ptZIPArchive) && (ValidFileData & VALID_DATA_SIZE)) // jen pokud je platne CFileData::Size (velikosti definovane pres plugin-data jsou u archivu dost nepravdepodobne, takze je v teto funkci zatim neresime...)
    {
        BeginStopRefresh(); // cmuchal bude cekat

        TDirectArray<CQuadWord> sizes(200, 400);
        CQuadWord totalSize(0, 0); // pocitana velikost
        int files = 0;
        int dirs = 0;

        int selIndex = -1;
        BOOL upDir;
        if (Dirs->Count > 0)
            upDir = (strcmp(Dirs->At(0).Name, "..") == 0);
        else
            upDir = FALSE;
        int count = GetSelCount();
        if (countSizeMode == 0 && count != 0 || countSizeMode == 2) // platne oznaceni
        {
            if (countSizeMode == 2)
                count = Dirs->Count - (upDir ? 1 : 0);
            int* indexes = new int[count];
            if (indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                EndStopRefresh(); // ted uz zase cmuchal nastartuje
                return;
            }
            else
            {
                if (countSizeMode == 2)
                {
                    int i = (upDir ? 1 : 0);
                    int j;
                    for (j = 0; j < count; j++)
                        indexes[j] = i++;
                }
                else
                    GetSelItems(count, indexes);
                int i = count;
                while (i--)
                {
                    CFileData* f = (indexes[i] < Dirs->Count) ? &Dirs->At(indexes[i]) : &Files->At(indexes[i] - Dirs->Count);
                    if (indexes[i] < Dirs->Count)
                    {
                        f->SizeValid = 1;
                        f->Size = GetArchiveDir()->GetDirSize(GetZIPPath(), f->Name, &dirs, &files, &sizes);
                        totalSize += f->Size;
                        dirs++;
                    }
                    else
                    {
                        sizes.Add(f->Size); // chyba pridani je osetrena az na urovni vystupniho dialogu
                        totalSize += f->Size;
                        files++;
                    }
                }
            }
            delete[] (indexes);
        }
        else // bereme vybrany soubor nebo adresar
        {
            selIndex = GetCaretIndex();
            if (upDir && selIndex == 0)
            {
                EndStopRefresh(); // ted uz zase cmuchal nastartuje
                return;           // neni co delat
            }
            else
            {
                if (countSizeMode == 0)
                {
                    SetSel(TRUE, selIndex);
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                }
                CFileData* f = (selIndex < Dirs->Count) ? &Dirs->At(selIndex) : &Files->At(selIndex - Dirs->Count);
                if (selIndex < Dirs->Count)
                {
                    f->SizeValid = 1;
                    f->Size = GetArchiveDir()->GetDirSize(GetZIPPath(), f->Name, &dirs, &files, &sizes);
                    totalSize += f->Size;
                    dirs++;
                }
                else
                {
                    sizes.Add(f->Size); // chyba pridani je osetrena az na urovni vystupniho dialogu
                    totalSize += f->Size;
                    files++;
                }
            }
        }

        // radime pokud jsme pocitali pres vic jak jeden oznaceny adresar nebo pres vsechny
        if (((countSizeMode == 0 && dirs > 1) || countSizeMode == 2) && SortType == stSize)
        {
            ChangeSortType(stSize, FALSE, TRUE);
        }
        RefreshListBox(-1, -1, FocusedIndex, FALSE, FALSE); // prepocet sirek sloupcu
        if (countSizeMode == 0)
        {
            CSizeResultsDlg(HWindow, totalSize, CQuadWord(-1, -1), CQuadWord(-1, -1),
                            files, dirs, &sizes)
                .Execute();
            //      CZIPSizeResultsDlg(HWindow, totalSize, files, dirs).Execute();
        }
        if (selIndex != -1 && countSizeMode == 0)
        {
            SetSel(FALSE, selIndex);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
        }
        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
        UpdateWindow(MainWindow->HWindow);

        EndStopRefresh(); // ted uz zase cmuchal nastartuje
    }
}

void CFilesWindow::AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE3("CFilesWindow::AcceptChangeOnPathNotification(%s, %d)",
                        path, includingSubdirs);

    BOOL refresh = FALSE;
    if ((Is(ptDisk) || Is(ptZIPArchive)) && (!AutomaticRefresh || GetNetworkDrive()))
    {
        // otestujeme shodnost cest nebo aspon jejich prefixu (zajimaji nas jen diskove cesty,
        // FS cesty v 'path' se vylouci automaticky, protoze se nemuzou nikdy shodovat s GetPath())
        char path1[MAX_PATH];
        char path2[MAX_PATH];
        lstrcpyn(path1, path, MAX_PATH);
        lstrcpyn(path2, GetPath(), MAX_PATH); // u archivu je zde cesta, na ktere je archiv
        SalPathRemoveBackslash(path1);
        SalPathRemoveBackslash(path2);
        int len1 = (int)strlen(path1);
        refresh = !includingSubdirs && StrICmp(path1, path2) == 0 ||       // presna shoda
                  includingSubdirs && StrNICmp(path1, path2, len1) == 0 && // shoda prefixu
                      (path2[len1] == 0 || path2[len1] == '\\');
        if (Is(ptDisk) && !refresh && CutDirectory(path1)) // u archivu nema smysl
        {
            SalPathRemoveBackslash(path1);
            // na NTFS se meni i datum posledniho podadresare v ceste (projevi se bohuzel az po vstupu
            // do tohoto podadresare, ale treba to casem opravi, proto budeme preventivne refreshovat)
            refresh = StrICmp(path1, path2) == 0;
        }
        if (refresh)
        {
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }
    }
    else
    {
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty()) // poslani notifikace do FS
        {
            // sekce EnterPlugin+LeavePlugin musi byt vyvezena az sem (neni v zapouzdreni ifacu)
            EnterPlugin();
            GetPluginFS()->AcceptChangeOnPathNotification(GetPluginFS()->GetPluginFSName(), path, includingSubdirs);
            LeavePlugin();
        }
    }

    if (Is(ptDisk) && !refresh &&           // jen disky maji free-space (archivy ne a FS se resi jinde)
        HasTheSameRootPath(path, GetPath()) // stejny root -> mozna zmena velikosti volneho mista na disku

        /* && (!AutomaticRefresh ||  // zakomentovano, protoze pri zmenach v podadresarich na auto-refreshovane ceste nechodi notifikace a tudiz udaj o volnem miste zustaval neplatny
       !IsTheSamePath(path, GetPath()))*/
        ) // tato cesta se nemonitoruje na zmeny -> refresh urcite neprijde
    {
        RefreshDiskFreeSpace(TRUE, TRUE);
    }
}

void CFilesWindow::IconOverlaysChangedOnPath(const char* path)
{
    //  if ((int)(GetTickCount() - NextIconOvrRefreshTime) < 0)
    //    TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: skipping notification for: " << path);
    if ((int)(GetTickCount() - NextIconOvrRefreshTime) >= 0 &&             // refresh icon-overlays probehne az v case NextIconOvrRefreshTime, pred timto okamzikem nema smysl sledovat zmeny
        !IconOvrRefreshTimerSet && !NeedIconOvrRefreshAfterIconsReading && // refresh icon-overlays jeste neni naplanovany
        Configuration.EnableCustomIconOverlays && Is(ptDisk) &&
        (UseSystemIcons || UseThumbnails) && IconCache != NULL &&
        IsTheSamePath(path, GetPath()))
    {
        DWORD elapsed = GetTickCount() - LastIconOvrRefreshTime;
        if (elapsed < ICONOVR_REFRESH_PERIOD) // vyckame pred dalsim refreshem icon overlayu, abysme to nedelali prilis casto
        {
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: setting timer for refresh");
            if (SetTimer(HWindow, IDT_ICONOVRREFRESH, max(200, ICONOVR_REFRESH_PERIOD - elapsed), NULL))
            {
                IconOvrRefreshTimerSet = TRUE;
                return;
            }
            // pri chybe timeru zkusime provest refresh okamzite...
        }
        // refresh zkusime provest okamzite (neprisel prilis brzy po predchozim)
        if (!IconCacheValid) // provedeme ho az se dokonci nacitani ikon (muzou, ale nemusi byt nacetne spravne)
        {
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: delaying refresh till end of reading of icons");
            NeedIconOvrRefreshAfterIconsReading = TRUE;
        }
        else // provedeme refresh ihned
        {
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: doing refresh: sleeping icon reader");
            SleepIconCacheThread();
            WaitOneTimeBeforeReadingIcons = 200; // po tuto dobu ceka icon-reader pred zahajenim nacitani icon-overlays, takze dalsi notifikace pro tento panel od Tortoise SVN, ktere prijdou v nasledujicich 200ms neni potreba resit...
            LastIconOvrRefreshTime = GetTickCount();
            NextIconOvrRefreshTime = LastIconOvrRefreshTime + WaitOneTimeBeforeReadingIcons;
            WakeupIconCacheThread();
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: doing refresh: icon reader is awake again");
        }
    }
}
