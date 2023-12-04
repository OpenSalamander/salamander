// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CRenameScriptEntry
//

struct CRenameScriptEntry
{
    CSourceFile* Source;
    char* NewName;
    char* NewPart;
    int Blocks : 28;            // stavba skriptu: index polozky zavisle na teto
                                // zpracovani skriptu: nasledujici polozka zavisi
                                // na teto
    unsigned int Overwrite : 1; // uzivatel potvrdil prepis existujiciho souboru

    // pomocne promene pro stavbu skriptu
    unsigned int Skip : 1;    // nebude se prejmenovavat
    unsigned int Done : 1;    // polozka uz byla pridana do skriptu
    unsigned int Blocked : 1; // polozka zavisi na nejake dalsi polozce

    CRenameScriptEntry()
    {
        Source = NULL;
        NewName = NULL;
        Blocks = -1;
        Overwrite = 0;
        Skip = 0;
        Done = 0;
        Blocked = 0;
    }
    ~CRenameScriptEntry()
    {
        if (NewName)
            free(NewName);
    }
    static int __cdecl CompareOldNames(const void* elem1, const void* elem2)
    {
        return SG->StrICmp(
            ((CRenameScriptEntry*)elem1)->Source->FullName,
            ((CRenameScriptEntry*)elem2)->Source->FullName);
    }
    static int __cdecl CompareOldName(const void* key, const void* elem2)
    {
        return SG->StrICmp((const char*)key, ((CRenameScriptEntry*)elem2)->Source->FullName);
    }
    static int __cdecl CompareNewNames(const void* elem1, const void* elem2)
    {
        if (((CRenameScriptEntry*)elem1)->NewName && ((CRenameScriptEntry*)elem2)->NewName)
        {
            return SG->StrICmp(
                ((CRenameScriptEntry*)elem1)->NewName,
                ((CRenameScriptEntry*)elem2)->NewName);
        }
        else
        {
            if (((CRenameScriptEntry*)elem1)->NewName)
                return 1;
            if (((CRenameScriptEntry*)elem2)->NewName)
                return -1;
            return 0;
        }
    }
};

// ****************************************************************************
//
// CUndoStackEntry
//

CUndoStackEntry::CUndoStackEntry(char* source, char* target, CSourceFile* renamedFile,
                                 BOOL isDir, BOOL blocks)
{
    CALL_STACK_MESSAGE_NONE
    Source = SG->DupStr(source);
    if (target)
        Target = SG->DupStr(target);
    else
        Target = NULL;
    RenamedFile = renamedFile;
    IsDir = isDir ? 1 : 0;
    Blocks = blocks ? 1 : 0;
    Independent = 0;
}

CUndoStackEntry::~CUndoStackEntry()
{
    CALL_STACK_MESSAGE_NONE
    if (Source)
        free(Source);
    if (Target)
        free(Target);
}

// ****************************************************************************
//
// CRenamerDialog
//

void CRenamerDialog::Rename(BOOL validate)
{
    CALL_STACK_MESSAGE2("CRenamerDialog::Rename(%d)", validate);
    Progress = NULL;

    CRenameScriptEntry* script;
    int count;
    BOOL somethingToDo;
    if (BuildScript(script, count, validate, somethingToDo))
    {
        if (validate || !somethingToDo)
        {
            if (Progress)
            {
                EnableWindow(HWindow, TRUE);
                DestroyWindow(Progress->HWindow);
            }
            if (validate)
            {
                SG->SalMessageBox(HWindow, LoadStr(IDS_VALIDATEOK),
                                  LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION);
            }
            else
            {
                delete[] script;
                SG->SalMessageBox(HWindow, LoadStr(IDS_NOTTODO),
                                  LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION);
            }
            return;
        }

        Errors = FALSE;
        // undo stack mazeme vzdycky
        // if (ProcessRenamed)
        // {
        RenamedFiles.DestroyMembers();
        UndoStack.DestroyMembers();
        // }

        ExecuteScript(script, count);

        delete[] script;

        NotRenamedFiles.DestroyMembers();
        if (Errors || count < SourceFiles.Count)
        {
            SG->SalMessageBox(HWindow, LoadStr(IDS_SOMEERRORS),
                              LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION);
            int i;
            for (i = 0; i < SourceFiles.Count; i++)
                if (SourceFiles[i]->State == 0)
                    NotRenamedFiles.Add(new CSourceFile(SourceFiles[i]));

            ProcessRenamed = FALSE;
            ProcessNotRenamed = TRUE;
        }
        else
        {
            ProcessRenamed = TRUE;
            ProcessNotRenamed = FALSE;
            strcpy(DefMask, "*.*");
            ResetOptions(TRUE);
        }

        if (RenamerOptions.Spec == rsFullPath)
        {
            Root[0] = 0;
            RootLen = 0;
        }

        ReloadSourceFiles();
    }

    if (Progress)
    {
        EnableWindow(HWindow, TRUE);
        DestroyWindow(Progress->HWindow);
    }

    if (MinBeepWhenDone && IsIconic(HWindow))
        MessageBeep(0);
}

BOOL CRenamerDialog::BuildScript(CRenameScriptEntry*& script, int& count,
                                 BOOL validate, BOOL& somethingToDo)
{
    CALL_STACK_MESSAGE4("CRenamerDialog::BuildScript(, %d, %d, %d)", count,
                        validate, somethingToDo);
    BOOL ret = FALSE;
    CRenameScriptEntry* tmpScript = NULL;
    script = NULL;
    char newName[MAX_PATH];
    char* newPart;
    BOOL skip;
    BOOL skipAllLongNames = FALSE,
         skipAllBadNames = FALSE,
         skipAllDuplicateNames = FALSE,
         skipDifferentDirRoots = FALSE;
    SkipAllFileDir = FALSE;
    SkipAllDependingNames = FALSE;
    Silent = ::Silent;
    somethingToDo = FALSE;
    int i = 0;
    BOOL usrBreak = FALSE;

    // nastavime options
    CRenamer renamer(Root, RootLen);
    if (!ManualMode && !renamer.SetOptions(&RenamerOptions))
    {
        int error, errorPos1, errorPos2;
        CRenamerErrorType errorType;
        renamer.GetError(error, errorPos1, errorPos2, errorType);
        Error(error);
        int id;
        switch (errorType)
        {
        case retNewName:
            id = IDC_NEWNAME;
            break;
        case retBMSearch:
            id = IDC_SEARCH;
            break;
        case retRegExp:
            id = IDC_SEARCH;
            break;
        case retReplacePattern:
            id = IDC_REPLACE;
            break;
        default:
            id = -1;
            break;
        }
        if (id != -1)
        {
            HWND ctrl = GetDlgItem(HWindow, id);
            HWND wnd = GetFocus();
            while (wnd != NULL && wnd != ctrl)
                wnd = GetParent(wnd);
            if (wnd == NULL) // fokusime jen pokud neni ctrl predek GetFocusu
            {                // jako napr. edit-line v combo-boxu
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)ctrl, TRUE);
            }
            SendMessage(ctrl, CB_SETEDITSEL, 0, MAKELONG(errorPos1, errorPos2));
        }
        goto LBUILD_SCRIPT_ERROR;
    }

    // vytvorime progress
    Progress = new CProgressDialog(HWindow);
    Progress->Create();
    EnableWindow(HWindow, FALSE);
    Progress->SetText(LoadStr(IDS_PREPARING));
    Progress->EmptyMessageLoop();

    // vytvorim pomocny skript
    tmpScript = new CRenameScriptEntry[SourceFiles.Count];
    for (i = 0; i < SourceFiles.Count; i++)
    {
        tmpScript[i].Source = SourceFiles[i];
        // vytvorime nove jmeno
        int l = ManualMode ? GetManualModeNewName(SourceFiles[i], i, newName, newPart) : renamer.Rename(SourceFiles[i], i, newName, &newPart);
        if (l < 0)
        {
            FileError(HWindow, SourceFiles[i]->FullName, IDS_EXP_SMALLBUFFER,
                      FALSE, &skip, &skipAllLongNames, IDS_ERROR);
            if (!skip)
                goto LBUILD_SCRIPT_ERROR;
            tmpScript[i].Skip = 1;
            continue;
        }
        // overime spravnost jmena
        if (!ValidateFileName(newPart, l, RenamerOptions.Spec, &skip, &skipAllBadNames))
        {
            if (!skip)
                goto LBUILD_SCRIPT_ERROR;
            tmpScript[i].Skip = 1;
            continue;
        }
        CutTrailingDots(newPart, l, RenamerOptions.Spec);
        tmpScript[i].NewName = SG->DupStr(newName);
        tmpScript[i].NewPart = tmpScript[i].NewName + (newPart - newName);
        somethingToDo = somethingToDo || strcmp(SourceFiles[i]->FullName, newName);
    }

    // vyhazeme duplicitni jmena
    qsort(tmpScript, SourceFiles.Count, sizeof(*tmpScript), CRenameScriptEntry::CompareNewNames);
    for (i = 1; i < SourceFiles.Count; i++)
        if (!tmpScript[i - 1].Skip && !tmpScript[i].Skip &&
            SG->StrICmp(tmpScript[i - 1].NewName, tmpScript[i].NewName) == 0)
        {
            FileError(HWindow, tmpScript[i].NewName, IDS_DUPLICATENAME,
                      FALSE, &skip, &skipAllDuplicateNames, IDS_ERROR);
            if (!skip)
                goto LBUILD_SCRIPT_ERROR;
            tmpScript[i - 1].Skip = 1;
            do
                tmpScript[i].Skip = 1;
            while (++i < SourceFiles.Count &&
                   SG->StrICmp(tmpScript[i - 1].NewName, tmpScript[i].NewName) == 0);
            continue;
        }

    // seradime skript podle puvodniho jmena (pro binarni vyhledavani)
    qsort(tmpScript, SourceFiles.Count, sizeof(*tmpScript), CRenameScriptEntry::CompareOldNames);

    // otestujeme spravnost pomocneho skriptu
    for (i = 0; i < SourceFiles.Count; i++)
    {
        if (!tmpScript[i].Skip)
        {
            // overime, ze adresare maji stejny koren (neumime rekurzivni kopie adresaru)
            if (tmpScript[i].Source->IsDir &&
                !SG->HasTheSameRootPath(tmpScript[i].Source->FullName, tmpScript[i].NewName))
            {
                FileError(HWindow, tmpScript[i].Source->FullName, IDS_DIRNOTSAMEROOT,
                          FALSE, &skip, &skipDifferentDirRoots, IDS_ERROR);
                if (!skip)
                    goto LBUILD_SCRIPT_ERROR;
                tmpScript[i].Skip = 1;
                continue;
            }
            // overime, ze cilova jmena neexistuji, a nechame si potvrdit prepisy
            DWORD attr = SG->SalGetFileAttributes(tmpScript[i].NewName);
            if (attr != 0xFFFFFFFF)
            {
                if ((attr & FILE_ATTRIBUTE_DIRECTORY) ||
                    !(Silent & (SILENT_OVERWRITE_FILE_EXIST | SILENT_SKIP_FILE_EXIST)) ||
                    (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) &&
                        !(Silent & (SILENT_OVERWRITE_FILE_SYSHID | SILENT_SKIP_FILE_SYSHID)))
                {
                    // nove jmeno neni mezi soubory, ktere budou prejmenovany
                    if (!bsearch(tmpScript[i].NewName, tmpScript, SourceFiles.Count,
                                 sizeof(*tmpScript), CRenameScriptEntry::CompareOldName))
                    {
                        if (attr & FILE_ATTRIBUTE_DIRECTORY) // nelze prepsat adresar
                        {
                            FileError(HWindow, tmpScript[i].Source->FullName,
                                      tmpScript[i].Source->IsDir ? IDS_DIRDIR : IDS_FILEDIR,
                                      FALSE, &skip, &SkipAllFileDir, IDS_ERROR);
                            if (!skip)
                                goto LBUILD_SCRIPT_ERROR;
                            tmpScript[i].Skip = 1;
                            continue;
                        }
                        if (!FileOverwrite(HWindow, tmpScript[i].NewName, NULL,
                                           tmpScript[i].Source->FullName, NULL, attr,
                                           IDS_CNFRM_SHOVERWRITE, IDS_OVEWWRITETITLE, &skip, &Silent))
                        {
                            if (!skip)
                                goto LBUILD_SCRIPT_ERROR;
                            tmpScript[i].Skip = 1;
                            continue;
                        }
                        tmpScript[i].Overwrite = 1;
                    }
                }
                else
                {
                    if (Silent & SILENT_SKIP_FILE_EXIST)
                    {
                        tmpScript[i].Skip = 1;
                        continue;
                    }
                    if ((attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) &&
                        (Silent & SILENT_SKIP_FILE_SYSHID))
                    {
                        tmpScript[i].Skip = 1;
                        continue;
                    }
                    tmpScript[i].Overwrite = 1;
                }
            }
        }
        if ((usrBreak = Progress->Update(min((i + 1) * 1000 / SourceFiles.Count, 990))) != 0)
            goto LBUILD_SCRIPT_ERROR;
    }

    // nalezneme optimalni poradi pro provedeni rename operace
    // a vytvorime skript podle, ktereho budeme prejmenovavat
    if (!validate)
        script = new CRenameScriptEntry[SourceFiles.Count];
    count = 0;
    int skipped;
    skipped = 0;
    for (i = 0; i < SourceFiles.Count; i++) // detekce retezcu zavislosti
    {
        if (tmpScript[i].Skip)
            skipped++;
        else
        {
            CRenameScriptEntry* blocker = (CRenameScriptEntry*)bsearch(
                tmpScript[i].NewName, tmpScript, SourceFiles.Count, sizeof(*tmpScript),
                CRenameScriptEntry::CompareOldName);
            if (blocker && blocker != tmpScript + i)
            {
                blocker->Blocks = i;
                tmpScript[i].Blocked = 1;
            }
        }
    }
    for (i = 0; i < SourceFiles.Count; i++) // stavba skriptu
    {
        // nalezneme zacatek retezce zavislosti a pridame jeho cleny
        // do skriptu
        if (!tmpScript[i].Skip && !tmpScript[i].Done && !tmpScript[i].Blocked)
        {
            int prev = i;
            do
            {
                if (!validate)
                {
                    script[count].Source = tmpScript[prev].Source;
                    script[count].NewName = tmpScript[prev].NewName;
                    script[count].NewPart = tmpScript[prev].NewPart;
                    script[count].Blocks = tmpScript[prev].Blocks != -1;
                    script[count].Overwrite = tmpScript[prev].Overwrite;
                    tmpScript[prev].NewName = NULL;
                }
                tmpScript[prev].Done = 1;
                count++;
                prev = tmpScript[prev].Blocks;
            } while (prev != -1);
        }
    }

    // jsou-li zde polozky, ktere nejdou prejmenovat, zobrazime je
    if (count < SourceFiles.Count - skipped)
    {
        for (i = 0; i < SourceFiles.Count; i++)
        {
            if (!tmpScript[i].Skip && !tmpScript[i].Done)
            {
                FileError(HWindow, tmpScript[i].Source->FullName, IDS_DEPENDENCE,
                          FALSE, &skip, &SkipAllDependingNames, IDS_ERROR);
                if (!skip)
                    goto LBUILD_SCRIPT_ERROR;
            }
        }
    }

    ret = TRUE; // dosli jsem az sem, vse je OK
    Progress->Update(1000);

LBUILD_SCRIPT_ERROR:

    if (!ret && !usrBreak)
    {
        // fokusime polozku z chybou
        ListView_SetItemState(Preview->HWindow, i,
                              LVIS_FOCUSED | LVIS_SELECTED,
                              LVIS_FOCUSED | LVIS_SELECTED);
        ListView_EnsureVisible(Preview->HWindow, i, FALSE);

        // nastavime cursor na radku z chybou
        if (ManualMode)
        {
            int charIndex = (int)SendMessage(ManualEdit->HWindow, EM_LINEINDEX, i, 0);
            if (charIndex >= 0)
            {
                HWND wnd = GetFocus();
                if (wnd != ManualEdit->HWindow)
                {
                    if (Progress) // aby sel nastavit fokus na edit
                    {
                        EnableWindow(HWindow, TRUE);
                        DestroyWindow(Progress->HWindow);
                        Progress = NULL;
                    }

                    SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)ManualEdit->HWindow, TRUE);
                }
                SendMessage(ManualEdit->HWindow, EM_SETSEL, charIndex, charIndex);
                SendMessage(ManualEdit->HWindow, EM_SCROLLCARET, 0, 0);
            }
        }
    }

    if (tmpScript)
        delete[] tmpScript;
    if (!ret && script)
        delete[] script;

    return ret;
}

int CRenamerDialog::GetManualModeNewName(CSourceFile* file, int index, char* newName, char*& newPart)
{
    CALL_STACK_MESSAGE_NONE
    int pathLen = 0;
    switch (RenamerOptions.Spec)
    {
    case rsFileName:
    {
        pathLen = (int)(file->Name - file->FullName);
        memcpy(newName, file->FullName, pathLen);
        break;
    }
    case rsRelativePath:
    {
        pathLen = RootLen;
        memcpy(newName, Root, pathLen);
        if (newName[pathLen - 1] != '\\')
            newName[pathLen++] = '\\';
        break;
    }
    case rsFullPath:
        break;
    }
    newName += pathLen;
    newPart = newName;

    int charIndex = (int)SendMessage(ManualEdit->HWindow, EM_LINEINDEX, index, 0);
    if (charIndex < 0) // tohle by nemelo nastat -- neprojde CRenamerDialog::Validate
    {
        *newName = 0;
        return 0;
    }
    else
    {
        int l = (int)SendMessage(ManualEdit->HWindow, EM_LINELENGTH, charIndex, 0);
        if (l >= MAX_PATH - pathLen)
        {
            return -1;
        }
        else
        {
            *LPWORD(newName) = MAX_PATH - pathLen;
            int l2 = (int)SendMessage(ManualEdit->HWindow, EM_GETLINE, index, (LPARAM)newName);
            newName[l2] = 0; // pro jistotu
        }
        return l;
    }
}

void CRenamerDialog::ExecuteScript(CRenameScriptEntry* script, int count)
{
    CALL_STACK_MESSAGE2("CRenamerDialog::ExecuteScript(, %d)", count);
    BOOL skip;
    SkipAllOverwrite = SkipAllMove = SkipAllDeleteErr = SkipAllBadWrite =
        SkipAllBadRead = SkipAllOpenOut = SkipAllOpenIn =
            SkipAllDirChangeCase = SkipAllCreateDir = FALSE;

    Progress->SetText(LoadStr(IDS_RENAMING));

    // provedeme rename
    BOOL blocked = FALSE;
    BOOL success = TRUE;
    int i;
    for (i = 0; i < count; i++)
    {
        if (success || !blocked)
        {
            success = MoveFile(script[i].Source->FullName, script[i].NewName, script[i].NewPart,
                               script[i].Overwrite, script[i].Source->IsDir, skip);
        }
        else
        {
            FileError(HWindow, script[i].Source->FullName, IDS_DEPENDENCE,
                      FALSE, &skip, &SkipAllDependingNames, IDS_ERROR);
        }
        if (success)
        {
            if (RemoveSourcePath)
            {
                char dir[MAX_PATH];
                strcpy(dir, script[i].Source->FullName);
                do
                {
                    SG->CutDirectory(dir);
                    SG->ClearReadOnlyAttr(dir); // aby sel smazat
                } while (RemoveDirectory(dir));
            }
            script[i].Source->State = 1;
            CSourceFile* f = new CSourceFile(script[i].Source, script[i].NewName);
            RenamedFiles.Add(f);
            UndoStack.Add(new CUndoStackEntry(script[i].NewName, script[i].Source->FullName,
                                              f, script[i].Source->IsDir, blocked));
        }
        else
        {
            Errors = TRUE;
            if (!skip)
                return;
        }
        blocked = script[i].Blocks;
        if (Progress->Update((i + 1) * 1000 / count))
        {
            Errors = TRUE;
            break;
        }
    }
}

void CRenamerDialog::Undo()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::Undo()");
    Undoing = TRUE;
    SkipAllOverwrite = SkipAllMove = SkipAllDeleteErr = SkipAllBadWrite =
        SkipAllBadRead = SkipAllOpenOut = SkipAllOpenIn =
            SkipAllDirChangeCase = SkipAllCreateDir = SkipAllFileDir =
                SkipAllDependingNames = SkipAllRemoveDir = FALSE;

    // vytvorime progress
    Progress = new CProgressDialog(HWindow);
    Progress->Create();
    EnableWindow(HWindow, FALSE);
    Progress->SetText(LoadStr(IDS_UNDOING));
    Progress->EmptyMessageLoop();

    int total = UndoStack.Count;
    int done = 0;
    BOOL skip;
    BOOL blocked = FALSE;
    BOOL success = TRUE;
    BOOL pathSuccess = TRUE;
    int i;
    for (i = UndoStack.Count - 1; i >= 0; i--, done++)
    {
        CUndoStackEntry* entry = UndoStack[i];
        if (entry->RenamedFile)
        {
            if (Progress->Update(done * 1000 / total))
                break;

            // undo move file
            if (success || !blocked)
            {
                success = MoveFile(entry->Source, entry->Target, entry->Target,
                                   FALSE, entry->IsDir, skip);
            }
            else
            {
                FileError(HWindow, entry->Source, IDS_DEPENDENCE,
                          FALSE, &skip, &SkipAllDependingNames, IDS_ERROR);
            }
            if (success)
            {
                int j;
                for (j = RenamedFiles.Count - 1; j >= 0; j--)
                {
                    if (RenamedFiles[j] == entry->RenamedFile)
                    {
                        RenamedFiles.Detach(j);
                        NotRenamedFiles.Add(entry->RenamedFile->SetName(entry->Target));
                        break;
                    }
                }
                UndoStack.Delete(i);
            }
            else
            {
                if (!skip)
                    goto LUNDONE;
            }
            blocked = entry->Blocks;
            pathSuccess = success;
        }
        else
        {
            if (Progress->Update(done * 1000 / total))
            {
                if (pathSuccess)
                    entry->Independent = 1; // aby bylo mozne pozdeji pokracovat
                break;
            }

            // uklid cesty jen kdyz se podarilo uklidit soubor
            if (pathSuccess || entry->Independent)
            {
                if (entry->Target)
                {
                    // undo change directory case
                    while (1)
                    {
                        pathSuccess = SG->SalMoveFile(entry->Source, entry->Target, NULL);
                        if (pathSuccess)
                        {
                            UndoStack.Delete(i);
                            break;
                        }

                        if (!FileError(HWindow, entry->Source, IDS_DIRCASEERROR,
                                       TRUE, &skip, &SkipAllDirChangeCase, IDS_ERROR))
                        {
                            entry->Independent = 1; // aby bylo mozne pozdeji pokracovat
                            if (!skip)
                                goto LUNDONE;
                            break;
                        }
                    }
                }
                else
                {
                    // undo create directory
                    while (1)
                    {
                        pathSuccess = RemoveDirectory(entry->Source);
                        if (pathSuccess)
                        {
                            UndoStack.Delete(i);
                            break;
                        }

                        if (!FileError(HWindow, entry->Source, IDS_REMOVEDIR,
                                       TRUE, &skip, &SkipAllRemoveDir, IDS_ERROR))
                        {
                            entry->Independent = 1; // aby bylo mozne pozdeji pokracovat
                            if (!skip)
                                goto LUNDONE;
                            break;
                        }
                    }
                }
            }
        }
    }

    Progress->Update(done * 1000 / total);

LUNDONE:

    EnableWindow(HWindow, TRUE);
    DestroyWindow(Progress->HWindow);
    Undoing = FALSE;

    if (NotRenamedFiles.Count)
    {
        ProcessRenamed = FALSE;
        ProcessNotRenamed = TRUE;
    }
    ReloadSourceFiles();
}
