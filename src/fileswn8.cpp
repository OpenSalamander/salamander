// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "dialogs.h"
#include "snooper.h"
#include "worker.h"
#include "pack.h"
#include "mapi.h"

//
// ****************************************************************************
// CFilesWindow
//

int DeleteThroughRecycleBinAux(SHFILEOPSTRUCT* fo)
{
    __try
    {
        return SHFileOperation(fo);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 5))
    {
        FGIExceptionHasOccured++;
    }
    return 1; // != 0
}

BOOL PathContainsValidComponents(char* path, BOOL cutPath)
{
    char* s = path;
    while (*s != 0)
    {
        char* slash = strchr(s, '\\');
        if (slash == NULL)
            s += strlen(s);
        else
            s = slash;
        if (s > path && (*(s - 1) <= ' ' || *(s - 1) == '.'))
        {
            if (cutPath)
                *s = 0;
            return FALSE;
        }
        if (slash != NULL)
            s++;
    }
    return TRUE;
}

BOOL CFilesWindow::DeleteThroughRecycleBin(int* selection, int selCount, CFileData* oneFile)
{
    CALL_STACK_MESSAGE2("CFilesWindow::DeleteThroughRecycleBin(, %d,)", selCount);

    int i = 0;
    char path[MAX_PATH];
    strcpy(path, GetPath());
    SalPathAddBackslash(path, MAX_PATH);
    int pathLen = (int)strlen(path);
    // overim, jestli v ceste nejsou komponenty zakoncene mezerou/teckou, z toho se
    // Recycle Bin zblazni a maze na jine ceste (orezava tise ty mezery/tecky)
    if (!PathContainsValidComponents(path, TRUE))
    {
        char textBuf[2 * MAX_PATH + 200];
        sprintf(textBuf, LoadStr(IDS_RECYCLEBINERROR), path);
        SalMessageBox(MainWindow->HWindow, textBuf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE; // quick dirty bloody hack - Recycle Bin proste s tema jmenama koncicima na mezeru/tecku neumi (maze jine jmeno vznikle oriznutim mezer/tecek, to rozhodne nechceme)
    }
    CDynamicStringImp names;
    do
    {
        if (selCount > 0)
        {
            oneFile = (selection[i] < Dirs->Count) ? &Dirs->At(selection[i]) : &Files->At(selection[i] - Dirs->Count);
            i++;
        }
        if (oneFile->Name[oneFile->NameLen - 1] <= ' ' || oneFile->Name[oneFile->NameLen - 1] == '.')
        {
            char textBuf[2 * MAX_PATH + 200];
            sprintf(textBuf, LoadStr(IDS_RECYCLEBINERROR), oneFile->Name);
            SalMessageBox(MainWindow->HWindow, textBuf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            return FALSE; // quick dirty bloody hack - Recycle Bin proste s tema jmenama koncicima na mezeru/tecku neumi (maze jine jmeno vznikle oriznutim mezer/tecek, to rozhodne nechceme)
        }
        // oneFile ukazuje na oznacenou nebo caret polozku ve fileboxu
        if (!names.Add(path, pathLen) ||
            !names.Add(oneFile->Name, oneFile->NameLen + 1))
            return FALSE;
    } while (i < selCount);
    if (!names.Add("\0", 2))
        return FALSE; // dve nuly pro pripad prazdneho listu ("always false")

    SetCurrentDirectory(GetPath()); // pro rychlejsi operaci

    CShellExecuteWnd shellExecuteWnd;
    SHFILEOPSTRUCT fo;
    fo.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: CFilesWindow::DeleteThroughRecycleBin");
    fo.wFunc = FO_DELETE;
    fo.pFrom = names.Text;
    fo.pTo = NULL;
    fo.fFlags = FOF_ALLOWUNDO;
    fo.fAnyOperationsAborted = FALSE;
    fo.hNameMappings = NULL;
    fo.lpszProgressTitle = "";
    // provedeme samotne mazani - uzasne snadne, bohuzel jim sem tam pada ;-)
    CALL_STACK_MESSAGE1("CFilesWindow::DeleteThroughRecycleBin::SHFileOperation");
    BOOL ret = DeleteThroughRecycleBinAux(&fo) == 0;
    SetCurrentDirectoryToSystem();

    return FALSE; /*ret && !fo.fAnyOperationsAborted*/
    ;             // zatim proste nerusime oznaceni, navratovky nefukncni
}

void PluginFSConvertPathToExternal(char* path)
{
    char fsName[MAX_PATH];
    char* fsUserPart;
    int index;
    int fsNameIndex;
    if (IsPluginFSPath(path, fsName, (const char**)&fsUserPart) &&
        Plugins.IsPluginFS(fsName, index, fsNameIndex))
    {
        CPluginData* plugin = Plugins.Get(index);
        if (plugin != NULL && plugin->InitDLL(MainWindow->HWindow, FALSE, TRUE, FALSE)) // plugin nemusi byt naloadeny, pripadne ho nechame naloadit
            plugin->GetPluginInterfaceForFS()->ConvertPathToExternal(fsName, fsNameIndex, fsUserPart);
    }
}

// countSizeMode - 0 normalni vypocet, 1 vypocet pro vybranou polozku, 2 vypocet pro vsechny podadresare
void CFilesWindow::FilesAction(CActionType type, CFilesWindow* target, int countSizeMode)
{
    CALL_STACK_MESSAGE3("CFilesWindow::FilesAction(%d, , %d)", type, countSizeMode);
    if (Dirs->Count + Files->Count == 0)
        return;

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    BOOL invertRecycleBin = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (!FilesActionInProgress)
    {
        if (CheckPath(TRUE) != ERROR_SUCCESS)
            return; // zdroj je vzdycky aktualni adresar

        if (type != atCountSize && countSizeMode != 0)
            countSizeMode = 0; // pro jistotu (dale se testuje uz jen countSizeMode)

        FilesActionInProgress = TRUE;

        BeginSuspendMode(); // cmuchal si da pohov
        BeginStopRefresh(); // jen aby se nedistribuovaly zpravy o zmenach na cestach

        //---  zjisteni kolik je oznacenych adresaru a souboru
        BOOL subDir;
        if (Dirs->Count > 0)
            subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
        else
            subDir = FALSE;

        int* indexes = NULL;
        int files = 0;
        int dirs = 0;
        int count = GetSelCount();
        if (countSizeMode == 0 && count > 0)
        {
            indexes = new int[count];
            if (indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                FilesActionInProgress = FALSE;
                EndStopRefresh();
                EndSuspendMode(); // ted uz zase cmuchal nastartuje
                return;
            }
            else
            {
                GetSelItems(count, indexes);
                int i = count;
                while (i--)
                {
                    if (indexes[i] < Dirs->Count)
                        dirs++;
                    else
                        files++;
                }
            }
        }
        else
        {
            if (countSizeMode == 2) // vypocet velikosti vsech podadresaru
            {
                count = Dirs->Count;
                if (subDir)
                    count--;
                if (count > 0)
                {
                    indexes = new int[count];
                    if (indexes == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh();
                        EndSuspendMode(); // ted uz zase cmuchal nastartuje
                        return;
                    }
                    else
                    {
                        int i = (subDir ? 1 : 0);
                        int j;
                        for (j = 0; j < count; j++)
                            indexes[j] = i++;
                    }
                }
            }
            else
                count = 0;
        }

#ifndef _WIN64
        if (Windows64Bit && (type == atMove || type == atDelete))
        {
            int oneIndex;
            if (count == 0)
            {
                oneIndex = GetCaretIndex();
                if (oneIndex == 0 && subDir)
                    oneIndex = -1;
            }
            char redirectedDir[MAX_PATH];
            if ((count > 0 || oneIndex != -1) &&
                ContainsWin64RedirectedDir(this, count > 0 ? indexes : &oneIndex,
                                           count > 0 ? count : 1, redirectedDir, FALSE))
            {
                char msg[300 + MAX_PATH];
                _snprintf_s(msg, _TRUNCATE,
                            LoadStr(type == atMove ? IDS_ERRMOVESELCONTW64ALIAS : IDS_ERRDELETESELCONTW64ALIAS),
                            redirectedDir);
                SalMessageBox(HWindow, msg, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);

                if (indexes != NULL)
                    delete[] (indexes);
                FilesActionInProgress = FALSE;
                EndStopRefresh();
                EndSuspendMode(); // ted uz zase cmuchal nastartuje
                return;
            }
        }
#endif // _WIN64

        //---  sestaveni cilove cesty pro copy/move
        char path[2 * MAX_PATH + 200]; // + 200 je rezerva (Windows delaji cesty delsi nez MAX_PATH)
        target->GetGeneralPath(path, 2 * MAX_PATH + 200);
        if (target->Is(ptDisk))
        {
            SalPathAppend(path, "*.*", 2 * MAX_PATH + 200);
        }
        else
        {
            if (target->Is(ptZIPArchive))
            {
                SalPathAddBackslash(path, 2 * MAX_PATH + 200);

                // pokud nelze pakovat do archivu ve vedlejsim panelu, nechame cestu prazdnou
                int format = PackerFormatConfig.PackIsArchive(target->GetZIPArchive());
                if (format != 0) // nasli jsme podporovany archiv
                {
                    if (!PackerFormatConfig.GetUsePacker(format - 1)) // nema edit -> prazdny cil operace
                    {
                        path[0] = 0;
                    }
                }
            }
            else
            {
                if (target->Is(ptPluginFS) && (type == atCopy || type == atMove))
                {
                    if (target->GetPluginFS()->NotEmpty() &&
                        (type == atCopy && target->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMDISKTOFS) ||
                         type == atMove && target->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMDISKTOFS)))
                    {
                        // jde jen o upravu textu cilove cesty v plug-inu -> nema smysl snizovat prioritu threadu
                        int selFiles = 0;
                        int selDirs = 0;
                        if (count > 0) // nejake soubory jsou oznacene
                        {
                            selFiles = files;
                            selDirs = count - files;
                        }
                        else // bereme focus
                        {
                            int index = GetCaretIndex();
                            if (index >= Dirs->Count)
                                selFiles = 1;
                            else
                                selDirs = 1;
                        }
                        if (!target->GetPluginFS()->CopyOrMoveFromDiskToFS(type == atCopy, 1,
                                                                           target->GetPluginFS()->GetPluginFSName(),
                                                                           HWindow, NULL, NULL, NULL,
                                                                           selFiles, selDirs, path, NULL))
                        {
                            path[0] = 0; // chyba pri ziskavani cilove cesty
                        }
                        else
                        {
                            // prevedeme cestu na externi format (pred zobrazenim v dialogu)
                            PluginFSConvertPathToExternal(path);
                        }
                    }
                    else
                    {
                        path[0] = 0; // nema copy/move from disk to FS
                    }
                }
            }
        }
        if (path[0] != 0)
        {
            target->UserWorkedOnThisPath = TRUE; // default akce = prace s cestou v cilovem panelu
        }
        //---
        int recycle = 0;
        BOOL canUseRecycleBin = TRUE;
        if (type == atDelete)
        {
            if (MyGetDriveType(GetPath()) != DRIVE_FIXED)
            {
                recycle = 0;              // none
                canUseRecycleBin = FALSE; // nejde to, protoze to Windows neumi
            }
            else
            {
                if (invertRecycleBin)
                {
                    if (Configuration.UseRecycleBin == 0)
                        recycle = 1; // all
                    else
                        recycle = 0; // none
                }
                else
                    recycle = Configuration.UseRecycleBin;
            }
        }

        CFileData* f = NULL;
        char formatedFileName[MAX_PATH + 200]; // + 200 je rezerva (Windows delaji cesty delsi nez MAX_PATH)
        char expanded[200];
        BOOL deleteLink = FALSE;
        if (count <= 1) // jedna oznacena polozka nebo zadna
        {
            if (countSizeMode != 2) // neni vypocet velikosti vsech podadresaru (count je pocet oznacenych polozek)
            {
                int i;
                if (count == 0)
                    i = GetCaretIndex();
                else
                    GetSelItems(1, &i);

                if (i < 0 || i >= Dirs->Count + Files->Count || // spatny index (zadne soubory)
                    i == 0 && subDir)                           // se ".." nepracujem
                {
                    if (indexes != NULL)
                        delete[] (indexes);
                    FilesActionInProgress = FALSE;
                    EndStopRefresh();
                    EndSuspendMode(); // ted uz zase cmuchal nastartuje
                    return;
                }
                BOOL isDir = i < Dirs->Count;
                f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                expanded[0] = 0;
                if (type == atDelete && recycle != 1 && (f->Attr & FILE_ATTRIBUTE_REPARSE_POINT))
                { // jde o link (junction nebo symlink nebo volume mount point)
                    lstrcpyn(formatedFileName, GetPath(), MAX_PATH + 200);
                    ResolveSubsts(formatedFileName);
                    int repPointType;
                    if (SalPathAppend(formatedFileName, f->Name, MAX_PATH + 200) &&
                        GetReparsePointDestination(formatedFileName, NULL, 0, &repPointType, TRUE))
                    {
                        lstrcpy(expanded, LoadStr(repPointType == 1 /* MOUNT POINT */ ? IDS_QUESTION_VOLMOUNTPOINT : repPointType == 2 /* JUNCTION POINT */ ? IDS_QUESTION_JUNCTION
                                                                                                                                                            : IDS_QUESTION_SYMLINK));
                        deleteLink = TRUE;
                    }
                }
                AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);
                if (expanded[0] == 0)
                    lstrcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
            }
            else
                expanded[0] = 0; // nepouziva se
        }
        else // count-files adresaru a files souboru
        {
            ExpandPluralFilesDirs(expanded, 200, files, count - files, epfdmNormal, FALSE);
        }

        int resID = 0;
        switch (type)
        {
        case atCopy:
            resID = IDS_COPYTO;
            break;
        case atMove:
            resID = IDS_MOVETO;
            break;
        case atDelete:
        {
            switch (recycle)
            {
            case 0:
                resID = IDS_CONFIRM_DELETE;
                break;
            case 1:
                break; // tato hlaska se nezobrazi, protoze confirmation resi DeleteThroughRecycleBin()
            case 2:
                resID = deleteLink ? IDS_CONFIRM_DELETE : IDS_CONFIRM_DELETE2;
                break;
            }
            break;
        }
        }
        CTruncatedString str;
        char subject[MAX_PATH + 100 + 200]; // + 200 je rezerva (Windows delaji cesty delsi nez MAX_PATH)
        if (resID != 0)
        {
            sprintf(subject, LoadStr(resID), expanded);
            str.Set(subject, count > 1 ? NULL : formatedFileName);
        }

        //---
        int res;
        DWORD clusterSize = 0;
        CChangeCaseData changeCaseData;
        CCriteriaData criteria;
        if (CopyMoveOptions.Get() != NULL) // pokud existuji, vytahneme defaults
            criteria = *CopyMoveOptions.Get();
        CCriteriaData* criteriaPtr = NULL; // ukazatel na 'criteria'; pokud je NULL, ignoruji se
        BOOL copyToExistingDir = FALSE;
        char nextFocus[MAX_PATH + 200]; // + 200 je rezerva (Windows delaji cesty delsi nez MAX_PATH)
        nextFocus[0] = 0;
        char* mask = NULL;
        switch (type)
        {
        case atCopy:
        case atMove:
        {
            BOOL havePermissions = FALSE;
            BOOL supportsADS = IsPathOnVolumeSupADS(GetPath(), NULL);
            DWORD dummy1, flags;
            char dummy2[MAX_PATH];
            if (MyGetVolumeInformation(GetPath(), NULL, NULL, NULL, NULL, 0, NULL, &dummy1, &flags, dummy2, MAX_PATH))
                havePermissions = (flags & FS_PERSISTENT_ACLS) != 0;
            while (1)
            {
                if (strlen(path) >= 2 * MAX_PATH)
                    path[0] = 0; // cesta je moc dlouha, tohle neni idealni reseni, ale na lepsi ted nemam nervy :(
                res = (int)CCopyMoveMoreDialog(HWindow, path, 2 * MAX_PATH,
                                               (type == atCopy) ? LoadStr(IDS_COPY) : LoadStr(IDS_MOVE), &str,
                                               (type == atCopy) ? IDD_COPYDIALOG : IDD_MOVEDIALOG,
                                               Configuration.CopyHistory, COPY_HISTORY_SIZE,
                                               &criteria, havePermissions, supportsADS)
                          .Execute();
                if (!havePermissions)
                    criteria.CopySecurity = FALSE;
                if (res != IDOK)
                    break;
                criteriaPtr = criteria.IsDirty() ? &criteria : NULL;
                UpdateWindow(MainWindow->HWindow);

                if (!IsPluginFSPath(path) &&
                    (path[0] != 0 && path[1] == ':' ||                                             // cesty typu X:...
                     (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\') || // UNC cesty
                     Is(ptDisk) || Is(ptZIPArchive)))                                              // disk+archiv relativni cesty
                {                                                                                  // jde o diskovou cestu (absolutni nebo relativni) - otocime vsechny '/' na '\\' a zahodime zdvojene '\\'
                    SlashesToBackslashesAndRemoveDups(path);
                }

                int len = (int)strlen(path);
                BOOL backslashAtEnd = (len > 0 && path[len - 1] == '\\'); // cesta konci na backslash -> nutne adresar
                BOOL mustBePath = (len == 2 && LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                                   path[1] == ':'); // cesta typu "c:" musi byt i po expanzi cesta (ne soubor)

                int pathType;
                BOOL pathIsDir;
                char* secondPart;
                char textBuf[2 * MAX_PATH + 200];
                if (ParsePath(path, pathType, pathIsDir, secondPart,
                              type == atCopy ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                              count <= 1 ? nextFocus : NULL, NULL, 2 * MAX_PATH))
                {
                    // misto konstrukce 'switch' pouzijeme 'if', aby fungovaly 'break' + 'continue'
                    if (pathType == PATH_TYPE_WINDOWS) // Windows cesta (disk + UNC)
                    {
                        if (strlen(path) >= MAX_PATH)
                        {
                            SalMessageBox(HWindow, LoadStr(IDS_TOOLONGPATH),
                                          (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                          MB_OK | MB_ICONEXCLAMATION);
                            continue;
                        }

                        CFileData* dir = (count == 0) ? f : ((indexes[0] < Dirs->Count) ? &Dirs->At(indexes[0]) : &Files->At(indexes[0] - Dirs->Count));

                        if (SalSplitWindowsPath(HWindow, LoadStr(type == atCopy ? IDS_COPY : IDS_MOVE),
                                                LoadStr(type == atCopy ? IDS_ERRORCOPY : IDS_ERRORMOVE),
                                                count, path, secondPart, pathIsDir, backslashAtEnd || mustBePath,
                                                dir->Name, GetPath(), mask))
                        {
                            if (nextFocus[0] != 0 && secondPart[0] == 0)
                                copyToExistingDir = TRUE;
                            break; // opustime smycku Copy/Move a jdeme provest operaci
                        }
                        else
                        {
                            continue; // znovu do Copy/Move dialogu
                        }
                    }
                    else
                    {
                        if (pathType == PATH_TYPE_ARCHIVE) // cesta do archivu
                        {
                            if (strlen(secondPart) >= MAX_PATH) // neni prilis dlouha cesta v archivu?
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_TOOLONGPATH),
                                              (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                              MB_OK | MB_ICONEXCLAMATION);
                                continue;
                            }

                            if (criteriaPtr != NULL && Configuration.CnfrmCopyMoveOptionsNS) // archivy nepodporuji options z Copy/Move dialogu
                            {
                                MSGBOXEX_PARAMS params;
                                memset(&params, 0, sizeof(params));
                                params.HParent = HWindow;
                                params.Flags = MB_OK | MB_ICONINFORMATION | MSGBOXEX_HINT;
                                params.Caption = LoadStr(IDS_INFOTITLE);
                                params.Text = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED);
                                params.CheckBoxText = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED_AGAIN);
                                int dontShow = !Configuration.CnfrmCopyMoveOptionsNS;
                                params.CheckBoxValue = &dontShow;
                                SalMessageBoxEx(&params);
                                Configuration.CnfrmCopyMoveOptionsNS = !dontShow;
                            }

                            CPanelTmpEnumData data;
                            int oneIndex = -1;
                            if (count > 0) // nejake soubory jsou oznacene
                            {
                                data.IndexesCount = count;
                                data.Indexes = indexes; // dealokuje se pres 'indexes'
                            }
                            else // bereme focus
                            {
                                oneIndex = GetCaretIndex();
                                data.IndexesCount = 1;
                                data.Indexes = &oneIndex; // nedealokuje se
                            }
                            data.CurrentIndex = 0;
                            data.ZIPPath = GetZIPPath();
                            data.Dirs = Dirs;
                            data.Files = Files;
                            data.ArchiveDir = GetArchiveDir();
                            lstrcpyn(data.WorkPath, GetPath(), MAX_PATH);
                            data.EnumLastDir = NULL;
                            data.EnumLastIndex = -1;

                            //---  zjistime jestli je to nulovy soubor
                            BOOL nullFile;
                            BOOL hasPath = *secondPart != 0;
                            if (hasPath && !backslashAtEnd && !mustBePath) // zkontrolujeme, jestli nepouzili operacni masku -> neumime
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_MOVECOPY_OPMASKSNOTSUP),
                                              (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                              MB_OK | MB_ICONEXCLAMATION);
                                continue; // znovu do copy/move dialogu
                            }

                            *secondPart = 0; // v 'path' je nazev souboru archivu
                            BOOL haveSize = FALSE;
                            CQuadWord size;
                            DWORD err;
                            HANDLE hFile = HANDLES_Q(CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL));
                            if (hFile != INVALID_HANDLE_VALUE)
                            {
                                haveSize = SalGetFileSize(hFile, size, err);
                                HANDLES(CloseHandle(hFile));
                            }
                            else
                                err = GetLastError();
                            if (haveSize)
                            {
                                nullFile = (size == CQuadWord(0, 0));

                                //---  je-li to nulovy soubor, musime ho zrusit, archivatory s nimi neumi delat
                                DWORD nullFileAttrs;
                                if (nullFile)
                                {
                                    nullFileAttrs = SalGetFileAttributes(path);
                                    ClearReadOnlyAttr(path, nullFileAttrs); // aby sel smazat i read-only
                                    DeleteFile(path);
                                }
                                //---  vlastni pakovani
                                SetCurrentDirectory(GetPath());
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                                if (PackCompress(HWindow, this, path, hasPath ? secondPart + 1 : "",
                                                 type == atMove, GetPath(), PanelEnumDiskSelection, &data))
                                {                   // pakovani se povedlo
                                    if (nullFile && // nulovy soubor mohl mit jiny compressed atribut, nastavime archiv na stejny
                                        nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                    {
                                        HANDLE hFile2 = HANDLES_Q(CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                                                                             0, NULL, OPEN_EXISTING,
                                                                             0, NULL));
                                        if (hFile2 != INVALID_HANDLE_VALUE)
                                        {
                                            // restorneme "compressed" flag, na FAT a FAT32 se to proste nepovede
                                            USHORT state = (nullFileAttrs & FILE_ATTRIBUTE_COMPRESSED) ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
                                            ULONG length;
                                            DeviceIoControl(hFile2, FSCTL_SET_COMPRESSION, &state,
                                                            sizeof(USHORT), NULL, 0, &length, FALSE);
                                            HANDLES(CloseHandle(hFile2));
                                            SetFileAttributes(path, nullFileAttrs);
                                        }
                                    }
                                    SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                                }
                                else
                                {
                                    if (nullFile) // nepovedlo se, musime ho zase vytvorit
                                    {
                                        HANDLE hFile2 = HANDLES_Q(CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                                                                             0, NULL, OPEN_ALWAYS,
                                                                             0, NULL));
                                        if (hFile2 != INVALID_HANDLE_VALUE)
                                        {
                                            if (nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                            {
                                                // restorneme "compressed" flag, na FAT a FAT32 se to proste nepovede
                                                USHORT state = (nullFileAttrs & FILE_ATTRIBUTE_COMPRESSED) ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
                                                ULONG length;
                                                DeviceIoControl(hFile2, FSCTL_SET_COMPRESSION, &state,
                                                                sizeof(USHORT), NULL, 0, &length, FALSE);
                                            }
                                            HANDLES(CloseHandle(hFile2));
                                            if (nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                                SetFileAttributes(path, nullFileAttrs);
                                        }
                                    }
                                }
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                                SetCurrentDirectoryToSystem();

                                UpdateWindow(MainWindow->HWindow);

                                //---  refresh neautomaticky refreshovanych adresaru
                                // zmena v adresari s cilovym archivem (meni se soubor archivu)
                                CutDirectory(path); // 'path' je jmeno archivu -> musi vzdy uspet
                                MainWindow->PostChangeOnPathNotification(path, FALSE);
                                if (type == atMove)
                                {
                                    // zmeny na zdrojove ceste (pri presunu souboru do archivu melo probehnout
                                    // mazani souboru/adresaru)
                                    MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
                                }
                            }
                            else
                            {
                                sprintf(textBuf, LoadStr(IDS_FILEERRORFORMAT), path, GetErrorText(err));
                                SalMessageBox(HWindow, textBuf,
                                              (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                              MB_OK | MB_ICONEXCLAMATION);
                                if (hasPath)
                                    *secondPart = '\\'; // obnova cesty - budeme editovat v Copy/Move dialogu
                                if (backslashAtEnd || mustBePath)
                                    SalPathAddBackslash(path, 2 * MAX_PATH + 200);
                                continue; // znovu do copy/move dialogu
                            }

                            if (indexes != NULL)
                                delete[] (indexes);
                            //---  pokud je aktivni nejaky okno salamandra, konci suspend mode
                            EndStopRefresh();
                            EndSuspendMode();
                            FilesActionInProgress = FALSE;
                            return;
                        }
                        else
                        {
                            if (pathType == PATH_TYPE_FS) // cesta na file-system
                            {
                                if (strlen(secondPart) >= MAX_PATH) // neni prilis dlouhy user-part cesty na FS?
                                {
                                    SalMessageBox(HWindow, LoadStr(IDS_TOOLONGPATH),
                                                  (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                                  MB_OK | MB_ICONEXCLAMATION);
                                    continue;
                                }

                                if (criteriaPtr != NULL && Configuration.CnfrmCopyMoveOptionsNS) // FS nepodporuji options z Copy/Move dialogu
                                {
                                    MSGBOXEX_PARAMS params;
                                    memset(&params, 0, sizeof(params));
                                    params.HParent = HWindow;
                                    params.Flags = MB_OK | MB_ICONINFORMATION | MSGBOXEX_HINT;
                                    params.Caption = LoadStr(IDS_INFOTITLE);
                                    params.Text = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED);
                                    params.CheckBoxText = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED_AGAIN);
                                    int dontShow = !Configuration.CnfrmCopyMoveOptionsNS;
                                    params.CheckBoxValue = &dontShow;
                                    SalMessageBoxEx(&params);
                                    Configuration.CnfrmCopyMoveOptionsNS = !dontShow;
                                }

                                // pripravime data pro enumeraci souboru a adresaru z panelu
                                CPanelTmpEnumData data;
                                int oneIndex = -1;
                                int selFiles = 0;
                                int selDirs = 0;
                                if (count > 0) // nejake soubory jsou oznacene
                                {
                                    selFiles = files;
                                    selDirs = count - files;
                                    data.IndexesCount = count;
                                    data.Indexes = indexes; // dealokuje se pres 'indexes'
                                }
                                else // bereme focus
                                {
                                    oneIndex = GetCaretIndex();
                                    if (oneIndex >= Dirs->Count)
                                        selFiles = 1;
                                    else
                                        selDirs = 1;
                                    data.IndexesCount = 1;
                                    data.Indexes = &oneIndex; // nedealokuje se
                                }
                                data.CurrentIndex = 0;
                                data.ZIPPath = GetZIPPath();
                                data.Dirs = Dirs;
                                data.Files = Files;
                                data.ArchiveDir = GetArchiveDir();
                                lstrcpyn(data.WorkPath, GetPath(), MAX_PATH);
                                data.EnumLastDir = NULL;
                                data.EnumLastIndex = -1;

                                // ziskame jmeno file-systemu
                                char fsName[MAX_PATH];
                                memcpy(fsName, path, (secondPart - path) - 1);
                                fsName[(secondPart - path) - 1] = 0;

                                // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                                // vybereme FS, ktery operaci provede (poradi: aktivni, odlozeny, novy)
                                BOOL invalidPath = FALSE;
                                BOOL unselect = FALSE;
                                char targetPath[2 * MAX_PATH];
                                CDetachedFSList* list = MainWindow->DetachedFSList;
                                int i;
                                for (i = -1; i < list->Count; i++)
                                {
                                    CPluginFSInterfaceEncapsulation* fs = NULL;
                                    if (i == -1) // nejdrive zkusime aktivni FS v cilovem panelu
                                    {
                                        if (target->Is(ptPluginFS))
                                            fs = target->GetPluginFS();
                                    }
                                    else
                                        fs = list->At(i); // pak odpojene FS

                                    int fsNameIndex;
                                    if (fs != NULL && fs->NotEmpty() &&                          // iface je platny
                                        fs->IsFSNameFromSamePluginAsThisFS(fsName, fsNameIndex)) // jmeno FS je ze stejneho pluginu (jinak ani nema cenu zkouset)
                                    {
                                        BOOL invalidPathOrCancel;
                                        lstrcpyn(targetPath, path, 2 * MAX_PATH); // hrozi i vetsi nez 2 * MAX_PATH
                                        // prevedeme cestu na interni format
                                        fs->GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex,
                                                                                             targetPath + strlen(fsName) + 1);
                                        if (fs->CopyOrMoveFromDiskToFS(type == atCopy, 2, fs->GetPluginFSName(),
                                                                       HWindow, GetPath(), PanelEnumDiskSelection, &data,
                                                                       selFiles, selDirs, targetPath, &invalidPathOrCancel))
                                        {
                                            unselect = !invalidPathOrCancel;
                                            break; // hotovo
                                        }
                                        else // chyba
                                        {
                                            // pred dalsim pouzitim musime resetnout (aby enumeroval opet od zacatku)
                                            data.Reset();

                                            if (invalidPathOrCancel)
                                            {
                                                // prevedeme cestu na externi format (pred zobrazenim v dialogu)
                                                PluginFSConvertPathToExternal(targetPath);
                                                strcpy(path, targetPath);
                                                invalidPath = TRUE;
                                                break; // musime znovu do copy/move dialogu
                                            }
                                            // zkusime jiny FS
                                        }
                                    }
                                }
                                if (i == list->Count) // aktivni ani odpojene FS to nezvladly, vytvorime novy FS
                                {
                                    int index;
                                    int fsNameIndex;
                                    if (Plugins.IsPluginFS(fsName, index, fsNameIndex)) // zjistime index pluginu
                                    {
                                        // ziskame plug-in s FS
                                        CPluginData* plugin = Plugins.Get(index);
                                        if (plugin != NULL)
                                        {
                                            // otevreme novy FS
                                            // load plug-inu pred ziskanim DLLName, Version a plug-in ifacu
                                            CPluginFSInterfaceAbstract* auxFS = plugin->OpenFS(fsName, fsNameIndex);
                                            CPluginFSInterfaceEncapsulation pluginFS(auxFS, plugin->DLLName, plugin->Version,
                                                                                     plugin->GetPluginInterfaceForFS()->GetInterface(),
                                                                                     plugin->GetPluginInterface()->GetInterface(),
                                                                                     fsName, fsNameIndex, -1, 0, plugin->BuiltForVersion);
                                            if (pluginFS.NotEmpty())
                                            {
                                                Plugins.SetWorkingPluginFS(&pluginFS);
                                                BOOL invalidPathOrCancel;
                                                lstrcpyn(targetPath, path, 2 * MAX_PATH); // hrozi i vetsi nez 2 * MAX_PATH
                                                // prevedeme cestu na interni format
                                                pluginFS.GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex,
                                                                                                          targetPath + strlen(fsName) + 1);
                                                if (pluginFS.CopyOrMoveFromDiskToFS(type == atCopy, 2,
                                                                                    pluginFS.GetPluginFSName(),
                                                                                    HWindow, GetPath(),
                                                                                    PanelEnumDiskSelection, &data,
                                                                                    selFiles, selDirs, targetPath,
                                                                                    &invalidPathOrCancel))
                                                { // hotovo/cancel
                                                    unselect = !invalidPathOrCancel;
                                                }
                                                else // chyba syntaxe/chyba plug-inu
                                                {
                                                    if (invalidPathOrCancel)
                                                    {
                                                        // prevedeme cestu na externi format (pred zobrazenim v dialogu)
                                                        PluginFSConvertPathToExternal(targetPath);
                                                        strcpy(path, targetPath);
                                                        invalidPath = TRUE; // musime znovu do copy/move dialogu
                                                    }
                                                    else // chyba plug-inu (novy FS, ale vraci chybu "v tomto FS nelze provest pozadovanou operaci")
                                                    {
                                                        TRACE_E("CopyOrMoveFromDiskToFS on new (empty) FS may not return error 'unable to process operation'.");
                                                    }
                                                }

                                                pluginFS.ReleaseObject(HWindow);
                                                plugin->GetPluginInterfaceForFS()->CloseFS(pluginFS.GetInterface());
                                                Plugins.SetWorkingPluginFS(NULL);
                                            }
                                            else
                                                TRACE_E("Plugin has refused to open FS (maybe it even does not start).");
                                        }
                                        else
                                            TRACE_E("Unexpected situation in CFilesWindow::FilesAction() - unable to work with plugin.");
                                    }
                                    else
                                    {
                                        TRACE_E("Unexpected situation in CFilesWindow::FilesAction() - file-system " << fsName << " was not found.");
                                    }
                                }

                                // opet zvysime prioritu threadu, operace dobehla
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                                SetCurrentDirectoryToSystem(); // pro vsechny pripady obnovime i cur-dir

                                if (invalidPath)
                                    continue; // znovu do copy/move dialogu

                                if (unselect) // odznacime soubory/adresare v panelu
                                {
                                    SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                                }

                                if (indexes != NULL)
                                    delete[] (indexes);
                                //---  pokud je aktivni nejaky okno salamandra, konci suspend mode
                                EndStopRefresh();
                                EndSuspendMode();
                                FilesActionInProgress = FALSE;
                                return;
                            }
                        }
                    }
                }
            }
            break;
        }

        case atDelete:
        {
            if (Configuration.CnfrmFileDirDel && recycle != 1)
            {                                                                                                           // ptame se jen pokud chce a pokud nepouzijeme API funkci SHFileOperation na mazani
                HICON hIcon = (HICON)HANDLES(LoadImage(Shell32DLL, MAKEINTRESOURCE(WindowsVistaAndLater ? 16777 : 161), // delete icon
                                                       IMAGE_ICON, 32, 32, IconLRFlags));
                int myRes = CMessageBox(HWindow, MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_SILENT,
                                        LoadStr(IDS_CONFIRM_DELETE_TITLE), &str, NULL,
                                        NULL, hIcon, 0, NULL, NULL, NULL, NULL)
                                .Execute();
                HANDLES(DestroyIcon(hIcon));
                res = (myRes == IDYES ? IDOK : IDCANCEL);
                UpdateWindow(MainWindow->HWindow);
            }
            else
                res = IDOK;
            break;
        }

        case atCountSize:
            res = IDOK;
            break;

        case atChangeCase:
        {
            CChangeCaseDlg dlg(HWindow, SelectionContainsDirectory());
            res = (int)dlg.Execute();
            UpdateWindow(MainWindow->HWindow);
            changeCaseData.FileNameFormat = dlg.FileNameFormat;
            changeCaseData.Change = dlg.Change;
            changeCaseData.SubDirs = dlg.SubDirs;
            break;
        }
        }
        if (res == IDOK && // zacne prace na disku
            CheckPath(TRUE) == ERROR_SUCCESS)
        {
            if (type == atDelete && recycle == 1)
            {
                if (DeleteThroughRecycleBin(indexes, count, f))
                {
                    SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                    UpdateWindow(MainWindow->HWindow);
                }

                //---  refresh neautomaticky refreshovanych adresaru
                // zmena v adresari zobrazenem v panelu a v jeho podadresarich
                MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
            }
            else
            {
                COperations* script;
                if (type == atCopy || type == atMove)
                {
                    if (count <= 1)
                    {
                        char format[300];
                        sprintf(format, LoadStr(type == atCopy ? IDS_COPYDLGTITLE : IDS_MOVEDLGTITLE), expanded);
                        sprintf(subject, format, formatedFileName);
                    }
                    else
                        sprintf(subject, LoadStr(type == atCopy ? IDS_COPYDLGTITLE : IDS_MOVEDLGTITLE), expanded);
                    script = new COperations(1000, 500, DupStr(subject), DupStr(GetPath()), DupStr(path));
                }
                else
                    script = new COperations(1000, 500, NULL, NULL, NULL);
                if (script == NULL)
                    TRACE_E(LOW_MEMORY);
                else
                {
                    const char* caption;
                    switch (type)
                    {
                    case atCopy:
                    {
                        caption = LoadStr(IDS_COPY);
                        script->ShowStatus = TRUE;
                        script->IsCopyOperation = TRUE;
                        script->IsCopyOrMoveOperation = TRUE;
                        break;
                    }

                    case atMove:
                    {
                        BOOL sameRootPath = HasTheSameRootPath(GetPath(), path);
                        script->SameRootButDiffVolume = sameRootPath && !HasTheSameRootPathAndVolume(GetPath(), path);
                        script->ShowStatus = !sameRootPath || script->SameRootButDiffVolume;
                        script->IsCopyOperation = FALSE;
                        script->IsCopyOrMoveOperation = TRUE;
                        caption = LoadStr(IDS_MOVE);
                        break;
                    }

                    case atDelete:
                        caption = LoadStr(IDS_DELETE);
                        break;
                    case atChangeCase:
                        caption = LoadStr(IDS_CHANGECASE);
                        break;
                    default:
                        caption = "";
                    }
                    if (criteriaPtr != NULL && criteriaPtr->UseSpeedLimit)
                        script->SetSpeedLimit(TRUE, criteriaPtr->SpeedLimit);
                    char captionBuf[50];
                    lstrcpyn(captionBuf, caption, 50); // jinak dochazi k prepisu LoadStr bufferu jeste pred nakopirovanim do lokalniho bufferu dialogu
                    caption = captionBuf;
                    HWND hFocusedWnd = GetFocus();
                    CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 1000, TRUE, MainWindow->HWindow);
                    MainWindow->StartAnimate(); // priklad pouziti
                    EnableWindow(MainWindow->HWindow, FALSE);

                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    script->InvertRecycleBin = invertRecycleBin;
                    script->CanUseRecycleBin = canUseRecycleBin;

                    char* auxTargetPath = NULL;
                    if (type == atCopy || type == atMove)
                        auxTargetPath = path;
                    BOOL res2 = BuildScriptMain(script, type, auxTargetPath, mask, count, indexes,
                                                f, NULL, &changeCaseData, countSizeMode != 0,
                                                criteriaPtr);
                    // pokud neni co delat, nebudeme vybalovat progress dialog
                    BOOL emptyScript = script->Count == 0 && type != atCountSize;

                    // prohozeno kvuli umozneni aktivace hlavniho okna (nesmi byt disable), jinak prepina do jine app
                    EnableWindow(MainWindow->HWindow, TRUE);
                    DestroySafeWaitWindow();
                    if (type == atCountSize) // jsou napocitane dalsi velikosti adresaru
                    {
                        // radime pokud jsme pocitali pres vic jak jeden oznaceny adresar nebo pres vsechny
                        if (((countSizeMode == 0 && dirs > 1) || countSizeMode == 2) && SortType == stSize)
                        {
                            ChangeSortType(stSize, FALSE, TRUE);
                        }

                        //            DirectorySizesHolder.Store(this); // ulozime jmena a velikosti adresaru

                        RefreshListBox(-1, -1, FocusedIndex, FALSE, FALSE); // prepocet sirek sloupcu
                    }

                    // pokud je aktivni Salamander, zavolame SetFocus na zapamatovane okno (SetFocus nefunguje
                    // pokud je hl. okno disablovane - po deaktivaci/aktivaci disablovaneho hl. okna aktivni panel
                    // nema fokus)
                    HWND hwnd = GetForegroundWindow();
                    while (hwnd != NULL && hwnd != MainWindow->HWindow)
                        hwnd = GetParent(hwnd);
                    if (hwnd == MainWindow->HWindow)
                        SetFocus(hFocusedWnd);

                    SetCursor(oldCur);

                    BOOL cancel = FALSE;
                    if (!emptyScript && res2 && (type == atCopy || type == atMove))
                    {
                        BOOL occupiedSpTooBig = script->OccupiedSpace != CQuadWord(0, 0) &&
                                                script->BytesPerCluster != 0 && // mame informace o disku
                                                script->OccupiedSpace > script->FreeSpace &&
                                                !IsSambaDrivePath(path); // Samba vraci nesmyslny cluster-size, takze muzeme pocitat jedine s TotalFileSize

                        if (occupiedSpTooBig ||
                            script->BytesPerCluster != 0 && // mame informace o disku
                                script->TotalFileSize > script->FreeSpace)
                        {
                            char buf1[50];
                            char buf2[50];
                            char buf3[200];
                            sprintf(buf3, LoadStr(IDS_NOTENOUGHSPACE),
                                    NumberToStr(buf1, occupiedSpTooBig ? script->OccupiedSpace : script->TotalFileSize),
                                    NumberToStr(buf2, script->FreeSpace));
                            cancel = SalMessageBox(HWindow, buf3,
                                                   caption, MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED) != IDYES;
                        }
                    }

                    if (!cancel)
                    {
                        // pripravime refresh neautomaticky refreshovanych adresaru
                        if (!emptyScript && type != atCountSize)
                        {
                            if (type == atDelete || type == atChangeCase || type == atMove)
                            {
                                // zmena v adresari zobrazenem v panelu a v jeho podadresarich
                                script->SetWorkPath1(GetPath(), TRUE);
                            }
                            if (type == atCopy)
                            {
                                // zmena v cilovem adresari a v jeho podadresarich
                                script->SetWorkPath1(path, TRUE);
                            }
                            if (type == atMove)
                            {
                                // zmena v cilovem adresari a v jeho podadresarich
                                script->SetWorkPath2(path, TRUE);
                            }
                        }

                        if (!emptyScript &&
                            (!res2 || type == atCountSize ||
                             !StartProgressDialog(script, caption, NULL, NULL)))
                        {
                            if (res2 && type == atCountSize && countSizeMode == 0)
                            {
                                CSizeResultsDlg result(MainWindow->HWindow, script->TotalSize,
                                                       script->CompressedSize, script->OccupiedSpace,
                                                       script->FilesCount, script->DirsCount,
                                                       &script->Sizes);
                                result.Execute();
                            }
                            else
                                UpdateWindow(MainWindow->HWindow);
                            if (!script->IsGood())
                                script->ResetState();
                            FreeScript(script);
                        }
                        else // odstraneni sel. indexu
                        {
                            if (res2)
                            {
                                SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                            }
                            if (!emptyScript && nextFocus[0] != 0)
                            {
                                strcpy(NextFocusName, nextFocus);
                                DontClearNextFocusName = TRUE;
                                if (type == atCopy && copyToExistingDir)
                                { // jedna-li se o copy do adresare, je potreba (RefreshDirectory nemusi probehnout)
                                    PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0);
                                }
                            }
                            if (emptyScript) // pokud je skript prazdny, musime ho dealokovat
                            {
                                if (!script->IsGood())
                                    script->ResetState();
                                FreeScript(script);
                            }
                            UpdateWindow(MainWindow->HWindow);
                        }
                    }
                    else
                    {
                        if (!script->IsGood())
                            script->ResetState();
                        FreeScript(script);
                    }
                    MainWindow->StopAnimate(); // priklad pouziti
                }
            }
        }
        //---
        if (indexes != NULL)
            delete[] (indexes);
        //---  pokud je aktivni nejaky okno salamandra, konci suspend mode
        EndStopRefresh();
        EndSuspendMode();
        FilesActionInProgress = FALSE;
    }
}

// vytahne z adresare 'path' vsechny podadresare a vola sama sebe
// soubory prida do mapi
// vrati TRUE, pokud vse probehlo dobre; jinak vrati FALSE
BOOL EmailFilesAddDirectory(CSimpleMAPI* mapi, const char* path, BOOL* errGetFileSizeOfLnkTgtIgnAll)
{
    WIN32_FIND_DATA file;
    char myPath[MAX_PATH + 4];
    int l = (int)strlen(path);
    memmove(myPath, path, l);
    if (myPath[l - 1] != '\\')
        myPath[l++] = '\\';
    char* name = myPath + l;
    strcpy(name, "*");
    HANDLE find = HANDLES_Q(FindFirstFile(myPath, &file));
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
        {
            char text[2 * MAX_PATH + 100];
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            if (SalMessageBox(MainWindow->HWindow, text, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
                SetCursor(LoadCursor(NULL, IDC_WAIT));
                return FALSE; // user chce koncit
            }
            SetCursor(LoadCursor(NULL, IDC_WAIT));
        }
        return TRUE; // user chce pokracovat
    }
    BOOL ok = TRUE;
    do
    {
        if (file.cFileName[0] != 0 &&
            (file.cFileName[0] != '.' ||
             (file.cFileName[1] != 0 && (file.cFileName[1] != '.' || file.cFileName[2] != 0))))
        {
            strcpy(name, file.cFileName);
            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!EmailFilesAddDirectory(mapi, myPath, errGetFileSizeOfLnkTgtIgnAll))
                {
                    ok = FALSE;
                    break;
                }
            }
            else
            {
                // linky: size == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
                BOOL cancel = FALSE;
                CQuadWord size(file.nFileSizeLow, file.nFileSizeHigh);
                if ((file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                    !GetLinkTgtFileSize(MainWindow->HWindow, myPath, NULL, &size, &cancel, errGetFileSizeOfLnkTgtIgnAll))
                {
                    size.Set(file.nFileSizeLow, file.nFileSizeHigh);
                }
                if (cancel || !mapi->AddFile(myPath, &size))
                {
                    ok = FALSE;
                    break;
                }
            }
        }
    } while (FindNextFile(find, &file));
    HANDLES(FindClose(find));
    return ok;
}

// vytahne selection z panelu a vsechny polozky probehne:
// pokud jde o adresar, vola pro nej funkci EmailFilesAddDirectory
// pokud jde o soubor, prida ho do mapi
// pokud vse probehne dobre, necha vytvorit email
void CFilesWindow::EmailFiles()
{
    CALL_STACK_MESSAGE1("CFilesWindow::EmailFiles()");
    if (Dirs->Count + Files->Count == 0)
        return;
    if (!Is(ptDisk))
        return;

    BeginStopRefresh(); // cmuchal si da pohov

    if (!FilesActionInProgress)
    {
        FilesActionInProgress = TRUE;

        CSimpleMAPI* mapi = new CSimpleMAPI;
        if (mapi != NULL)
        {
            if (mapi->Init(HWindow))
            {
                BOOL send = TRUE;
                int selCount = GetSelCount();
                int alloc = (selCount == 0 ? 1 : selCount);

                int* indexes;
                indexes = new int[alloc];
                if (indexes == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                }
                else
                {
                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    if (selCount > 0)
                        GetSelItems(selCount, indexes);
                    else
                        indexes[0] = GetCaretIndex();

                    CFileData* f;
                    char path[MAX_PATH];
                    int l = (int)strlen(GetPath());
                    memmove(path, GetPath(), l);
                    if (path[l - 1] != '\\')
                        path[l++] = '\\';
                    char* name = path + l;
                    BOOL errGetFileSizeOfLnkTgtIgnAll = FALSE;
                    for (int i = 0; i < alloc; i++)
                    {
                        if (indexes[i] >= 0 && indexes[i] < Dirs->Count + Files->Count)
                        {
                            BOOL isDir = indexes[i] < Dirs->Count;
                            f = (indexes[i] < Dirs->Count) ? &Dirs->At(indexes[i]) : &Files->At(indexes[i] - Dirs->Count);
                            strcpy(name, f->Name);
                            if (isDir)
                            {
                                if (!EmailFilesAddDirectory(mapi, path, &errGetFileSizeOfLnkTgtIgnAll))
                                {
                                    send = FALSE;
                                    break;
                                }
                            }
                            else
                            {
                                // linky: f->Size == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
                                BOOL cancel = FALSE;
                                CQuadWord size = f->Size;
                                if ((f->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                                    !GetLinkTgtFileSize(HWindow, path, NULL, &size, &cancel, &errGetFileSizeOfLnkTgtIgnAll))
                                {
                                    size = f->Size;
                                }
                                if (cancel || !mapi->AddFile(path, &size))
                                {
                                    send = FALSE;
                                    break;
                                }
                            }
                        }
                    }
                    delete[] (indexes);
                    SetCursor(oldCur);
                }
                if (send && mapi->GetFilesCount() == 0)
                {
                    // zadny soubor k posilani, zobrazime informaci a vypadneme
                    SalMessageBox(HWindow, LoadStr(IDS_WANTEMAIL_NOFILE), LoadStr(IDS_INFOTITLE),
                                  MB_OK | MB_ICONINFORMATION);
                    send = FALSE;
                }
                if (send)
                {
                    int ret = IDYES;
                    BOOL dontShow = !Configuration.CnfrmSendEmail;
                    if (!dontShow)
                    {
                        char expanded[200];
                        char totalSize[100];
                        char text[300];
                        ExpandPluralFilesDirs(expanded, 200, mapi->GetFilesCount(), 0, epfdmNormal, FALSE);
                        CQuadWord size;
                        mapi->GetTotalSize(&size);
                        PrintDiskSize(totalSize, size, 0);
                        sprintf(text, LoadStr(IDS_CONFIRM_EMAIL), expanded, totalSize);

                        MSGBOXEX_PARAMS params;
                        memset(&params, 0, sizeof(params));
                        params.HParent = HWindow;
                        params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                        params.Caption = LoadStr(IDS_QUESTION);
                        params.Text = text;
                        params.CheckBoxText = LoadStr(IDS_DONTSHOWAGAINSE);
                        params.CheckBoxValue = &dontShow;
                        ret = SalMessageBoxEx(&params);
                        Configuration.CnfrmSendEmail = !dontShow;
                    }
                    if (ret == IDYES)
                    {
                        SimpleMAPISendMail(mapi); // vlastni thread; postara se o destrukci objektu 'mapi'
                    }
                    else
                        delete mapi;
                }
                else
                    delete mapi;
            }
            else
                delete mapi;
        }
        else
            TRACE_E(LOW_MEMORY);
        FilesActionInProgress = FALSE;
    }
    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

BOOL CFilesWindow::OpenFocusedInOtherPanel(BOOL activate)
{
    CFilesWindow* otherPanel = (this == MainWindow->LeftPanel) ? MainWindow->RightPanel : MainWindow->LeftPanel;
    if (otherPanel == NULL)
        return FALSE;

    // povolime otevrit up-dir
    //  if (FocusedIndex == 0 && FocusedIndex < Dirs->Count &&
    //      strcmp(Dirs->At(0).Name, "..") == 0) return FALSE;   // up-dir nebereme
    if (FocusedIndex < 0 || FocusedIndex >= Files->Count + Dirs->Count)
        return FALSE; // nesmyslny index nebereme

    char buff[2 * MAX_PATH];
    buff[0] = 0;

    CFileData* file = (FocusedIndex < Dirs->Count) ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);

    if (Is(ptDisk) || Is(ptZIPArchive))
    {
        GetGeneralPath(buff, 2 * MAX_PATH);
        BOOL nethoodPath = FALSE;
        if (FocusedIndex == 0 && 0 < Dirs->Count && strcmp(Dirs->At(0).Name, "..") == 0 &&
            IsUNCRootPath(buff)) // up-dir na UNC root ceste => prechod na Nethood
        {
            char* s = buff + 2;
            while (*s != 0 && *s != '\\')
                s++;
            CPluginData* nethoodPlugin = NULL;
            char doublePath[2 * MAX_PATH];
            if (*s == '\\' && Plugins.GetFirstNethoodPluginFSName(doublePath, &nethoodPlugin))
            {
                nethoodPath = TRUE;
                *s = 0;
                int len = (int)strlen(doublePath);
                memmove(buff + len + 1, buff, strlen(buff) + 1);
                memcpy(buff, doublePath, len);
                buff[len] = ':';
            }
        }
        if (!nethoodPath)
        {
            SalPathAddBackslash(buff, 2 * MAX_PATH);
            int l = (int)strlen(buff);
            lstrcpyn(buff + l, file->Name, 2 * MAX_PATH - l);
        }
    }
    else if (Is(ptPluginFS) && GetPluginFS()->NotEmpty())
    {
        strcpy(buff, GetPluginFS()->GetPluginFSName());
        strcat(buff, ":");
        int l = (int)strlen(buff);
        int isDir;
        if (FocusedIndex == 0 && 0 < Dirs->Count && strcmp(Dirs->At(0).Name, "..") == 0)
            isDir = 2;
        else
            isDir = (FocusedIndex < Dirs->Count ? 1 : 0);
        if (!GetPluginFS()->GetFullName(*file, isDir, buff + l, 2 * MAX_PATH - l))
            buff[0] = 0;
    }

    if (buff[0] != 0)
    {
        int failReason;
        BOOL ret = otherPanel->ChangeDir(buff, -1, NULL, 3 /* change-dir */, &failReason, FALSE);
        if (activate && this == MainWindow->GetActivePanel())
        {
            if (ret || (failReason == CHPPFR_SUCCESS ||
                        failReason == CHPPFR_SHORTERPATH ||
                        failReason == CHPPFR_FILENAMEFOCUSED))
            {
                // pokud se cesta ve druhem panelu zmenila, aktivujeme jej
                MainWindow->ChangePanel();
                return TRUE;
            }
        }
    }
    return FALSE;
}

void CFilesWindow::ChangePathToOtherPanelPath()
{
    CFilesWindow* panel = (this == MainWindow->LeftPanel) ? MainWindow->RightPanel : MainWindow->LeftPanel;
    if (panel == NULL)
        return;

    if (panel->Is(ptDisk))
    {
        ChangePathToDisk(HWindow, panel->GetPath());
    }
    else
    {
        if (panel->Is(ptZIPArchive))
        {
            ChangePathToArchive(panel->GetZIPArchive(), panel->GetZIPPath());
        }
        else
        {
            if (panel->Is(ptPluginFS))
            {
                char path[2 * MAX_PATH];
                lstrcpyn(path, panel->GetPluginFS()->GetPluginFSName(), MAX_PATH - 1);
                strcat(path, ":");
                if (panel->GetPluginFS()->GetCurrentPath(path + strlen(path)))
                    ChangeDir(path, -1, NULL, 3 /*change-dir*/, NULL, FALSE);
            }
        }
    }
}
