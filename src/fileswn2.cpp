// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "dialogs.h"
#include "stswnd.h"
#include "snooper.h"
#include "zip.h"
#include "pack.h"
#include "cache.h"
#include "toolbar.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "shellib.h"

//
// ****************************************************************************
// CFilesWindow
//

void CFilesWindow::HandsOff(BOOL off)
{
    CALL_STACK_MESSAGE2("CFilesWindow::HandsOff(%d)", off);
    if (GetMonitorChanges())
    {
        if (off)
        {
            DetachDirectory((CFilesWindow*)this);
        }
        else
        {
            ChangeDirectory((CFilesWindow*)this, GetPath(), MyGetDriveType(GetPath()) == DRIVE_REMOVABLE);
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RefreshDiskFreeSpace(TRUE, TRUE);
        }
    }
}

void CFilesWindow::Execute(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::Execute(%d)", index);
    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    if (index < 0 || index >= Dirs->Count + Files->Count)
        return;

    char path[MAX_PATH];
    char fullName[MAX_PATH + 10];
    char doublePath[2 * MAX_PATH];
    WIN32_FIND_DATA data;

    BeginStopRefresh();

    if (Is(ptDisk))
    {
        if (index >= Dirs->Count) // soubor
        {
            if (CheckPath(FALSE) != ERROR_SUCCESS) // aktualni cesta neni pristupna
            {
                RefreshDirectory(); // obnovime panel (bud napr. vrati disketu nebo zmenime cestu)
                EndStopRefresh();
                return;
            }

            CFileData* file = &Files->At(index - Dirs->Count);
            char* fileName = file->Name;
            char fullPath[MAX_PATH];
            char netFSName[MAX_PATH];
            netFSName[0] = 0;
            if (file->DosName != NULL)
            {
                lstrcpy(fullPath, GetPath());
                if (SalPathAppend(fullPath, file->Name, MAX_PATH) &&
                    SalGetFileAttributes(fullPath) == INVALID_FILE_ATTRIBUTES &&
                    GetLastError() == ERROR_FILE_NOT_FOUND)
                {
                    lstrcpy(fullPath, GetPath());
                    if (SalPathAppend(fullPath, file->DosName, MAX_PATH) &&
                        SalGetFileAttributes(fullPath) != INVALID_FILE_ATTRIBUTES)
                    { // kdyz neni dostupne plne jmeno (problem zpetneho prevodu z multibyte na UNICODE), pouzijeme DOS-name
                        fileName = file->DosName;
                    }
                }
            }
            BOOL linkIsDir = FALSE;  // TRUE -> short-cut na adresar -> ChangePathToDisk
            BOOL linkIsFile = FALSE; // TRUE -> short-cut na soubor -> test archivu
            BOOL linkIsNet = FALSE;  // TRUE -> short-cut na sit -> ChangePathToPluginFS
            DWORD err = ERROR_SUCCESS;
            if (StrICmp(file->Ext, "lnk") == 0) // neni to short-cut adresare?
            {
                strcpy(fullName, GetPath());
                if (!SalPathAppend(fullName, fileName, MAX_PATH))
                {
                    SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                    UpdateWindow(HWindow);
                    EndStopRefresh();
                    return;
                }
                OLECHAR oleName[MAX_PATH];
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fullName, -1, oleName, MAX_PATH);
                oleName[MAX_PATH - 1] = 0;

                HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                IShellLink* link;
                if (CoCreateInstance(CLSID_ShellLink, NULL,
                                     CLSCTX_INPROC_SERVER, IID_IShellLink,
                                     (LPVOID*)&link) == S_OK)
                {
                    IPersistFile* fileInt;
                    if (link->QueryInterface(IID_IPersistFile, (LPVOID*)&fileInt) == S_OK)
                    {
                        if (fileInt->Load(oleName, STGM_READ) == S_OK)
                        {
                            if (link->GetPath(fullName, MAX_PATH, &data, SLGP_UNCPRIORITY) == NOERROR)
                            {                                     // cvicne vytazena cesta poslouzi pro test pristupnosti, po Resolve ji vytahneme znovu
                                err = CheckPath(FALSE, fullName); // fullName je plna cesta (linky jine nepodporuji)
                                if (err != ERROR_USER_TERMINATED) // pokud user nepouzil ESC, prip. chybu ignorujeme
                                {
                                    err = ERROR_SUCCESS; // Resolve muze cestu zmenit, pak udelame test znovu
                                }
                            }
                            if (err == ERROR_SUCCESS)
                            {
                                if (link->Resolve(HWindow, SLR_ANY_MATCH | SLR_UPDATE) == NOERROR)
                                {
                                    if (link->GetPath(fullName, MAX_PATH, &data, SLGP_UNCPRIORITY) == NOERROR)
                                    {
                                        // finalni podoba fullName - otestujeme jestli je o.k.
                                        err = CheckPath(TRUE, fullName); // fullName je plna cesta (linky jine nepodporuji)
                                        if (err == ERROR_SUCCESS)
                                        {
                                            DWORD attr = SalGetFileAttributes(fullName); // ziskavame zde, protoze data.dwFileAttributes se proste neplni, smula
                                            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                                            {
                                                linkIsDir = TRUE; // o.k. zkusime change-path-to-disk
                                            }
                                            else
                                            {
                                                linkIsFile = TRUE; // o.k. zkusime jestli to neni archiv
                                            }
                                        }
                                    }
                                    else // linky primo na servery muzeme zkusit otevrit v Network pluginu (Nethoodu)
                                    {
                                        if (Plugins.GetFirstNethoodPluginFSName(netFSName))
                                        {
                                            if (link->GetPath(fullName, MAX_PATH, NULL, SLGP_RAWPATH) != NOERROR)
                                            { // cesta neni v linku ulozena textove, ale jen jako ID-list
                                                fullName[0] = 0;
                                                ITEMIDLIST* pidl;
                                                if (link->GetIDList(&pidl) == S_OK && pidl != NULL)
                                                { // ziskame ten ID-list a doptame se na jmeno posledniho IDcka v listu, ocekavame "\\\\server"
                                                    IMalloc* alloc;
                                                    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                                                    {
                                                        if (!GetSHObjectName(pidl, SHGDN_FORPARSING | SHGDN_FORADDRESSBAR, fullName, MAX_PATH, alloc))
                                                            fullName[0] = 0;
                                                        if (alloc->DidAlloc(pidl) == 1)
                                                            alloc->Free(pidl);
                                                        alloc->Release();
                                                    }
                                                }
                                            }
                                            if (fullName[0] == '\\' && fullName[1] == '\\' && fullName[2] != '\\')
                                            { // zkusime jestli nejde o link na server (obsahuje cestu "\\\\server")
                                                char* backslash = fullName + 2;
                                                while (*backslash != 0 && *backslash != '\\')
                                                    backslash++;
                                                if (*backslash == '\\')
                                                    backslash++;
                                                if (*backslash == 0)  // bereme jen cesty "\\\\", "\\\\server", "\\\\server\\"
                                                    linkIsNet = TRUE; // o.k. zkusime change-path-to-FS
                                            }
                                        }
                                    }
                                }
                                else
                                    err = ERROR_USER_TERMINATED; // ve Windows "Missing Shortcut"
                            }
                        }
                        fileInt->Release();
                    }
                    link->Release();
                }
                SetCursor(oldCur);
            }
            if (err != ERROR_SUCCESS)
            {
                EndStopRefresh();
                return; // chyba cesty nebo preruseni
            }
            if (linkIsDir || linkIsNet) // link vede na sit nebo do adresare, cesta je o.k., prepneme se na ni
            {
                TopIndexMem.Clear(); // dlouhy skok
                if (linkIsDir)
                    ChangePathToDisk(HWindow, fullName);
                else
                    ChangePathToPluginFS(netFSName, fullName);
                UpdateWindow(HWindow);
                EndStopRefresh();
                return;
            }

            if (PackerFormatConfig.PackIsArchive(linkIsFile ? fullName : fileName)) // neni to archiv?
            {
                // zaloha udaju pro TopIndexMem
                strcpy(path, GetPath());
                int topIndex = ListBox->GetTopIndex();

                if (!linkIsFile)
                {
                    // konstrukce plneho jmena archivu pro ChangePathToArchive
                    strcpy(fullName, GetPath());
                    if (!SalPathAppend(fullName, fileName, MAX_PATH))
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                      MB_OK | MB_ICONEXCLAMATION);
                        UpdateWindow(HWindow);
                        EndStopRefresh();
                        return;
                    }
                }
                BOOL noChange;
                if (ChangePathToArchive(fullName, "", -1, NULL, FALSE, &noChange)) // podarilo se vlezt do archivu
                {
                    if (linkIsFile)
                        TopIndexMem.Clear(); // dlouhy skok
                    else
                        TopIndexMem.Push(path, topIndex); // zapamatujeme top-index pro navrat
                }
                else // archiv neni pristupny
                {
                    if (!noChange)
                        TopIndexMem.Clear(); // neuspech + nejsme na puvodni ceste -> dlouhy skok
                }
                UpdateWindow(HWindow);
                EndStopRefresh();
                return;
            }

            UserWorkedOnThisPath = TRUE;

            // nize umistene ExecuteAssociation umi zmenit cestu v panelu pri rekurzivnim
            // volani (obsahuje message-loopu), proto si ulozime plne jmeno souboru uz tady
            lstrcpy(fullPath, GetPath());
            if (!SalPathAppend(fullPath, fileName, MAX_PATH))
                fullPath[0] = 0;

            // spusteni default polozky z kontextoveho menu (asociace)
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            MainWindow->SetDefaultDirectories(); // aby startujici process zdedil spravne akt. adresare
            ExecuteAssociation(GetListBoxHWND(), GetPath(), fileName);

            // pridame soubor do historie
            if (fullPath[0] != 0)
                MainWindow->FileHistory->AddFile(fhitOpen, 0, fullPath);

            SetCursor(oldCur);
        }
        else // adresar
        {
            strcpy(path, GetPath());
            CFileData* dir = &Dirs->At(index);
            if (index == 0 && strcmp(dir->Name, "..") == 0) // ".. <Up>"
            {
                char* prevDir = NULL;
                if (!CutDirectory(path, &prevDir))
                {
                    if (path[0] == '\\' && path[1] == '\\')
                    {
                        char* s = path + 2;
                        while (*s != 0 && *s != '\\')
                            s++;
                        CPluginData* nethoodPlugin = NULL;
                        if (*s == '\\' && Plugins.GetFirstNethoodPluginFSName(doublePath, &nethoodPlugin))
                        {
                            *s++ = 0;
                            char* focusName = s;
                            while (*s != 0 && *s != '\\')
                                s++;
                            if (*s == '\\')
                                *s = 0;
                            nethoodPlugin->EnsureShareExistsOnServer(HWindow, this == MainWindow->LeftPanel ? PANEL_LEFT : PANEL_RIGHT,
                                                                     path + 2, focusName);
                            ChangePathToPluginFS(doublePath, path, -1, focusName);
                            if (Is(ptPluginFS))
                            {
                                TopIndexMem.Clear(); // pokud jsme nezustali na diskove ceste (v rootu UNC), jde o dlouhy skok
                                UpdateWindow(HWindow);
                            }
                        }
                    }
                    EndStopRefresh();
                    return; // neni kam zkracovat nebo uz jsme na Nethood ceste
                }
                int topIndex; // pristi top-index, -1 -> neplatny
                if (!TopIndexMem.FindAndPop(path, topIndex))
                    topIndex = -1;
                if (!ChangePathToDisk(HWindow, path, topIndex, prevDir))
                { // nepodarilo se zkratit cestu - dlouhy skok
                    TopIndexMem.Clear();
                }
            }
            else // podadresar
            {
                // zaloha udaju pro TopIndexMem (path + topIndex)
                int topIndex = ListBox->GetTopIndex();

                // zaloha caretu pro pripad access-denied adresare
                int caretIndex = GetCaretIndex();

                // nova cesta
                strcpy(fullName, path);
                if (!SalPathAppend(fullName, dir->Name, MAX_PATH))
                {
                    SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                    EndStopRefresh();
                    return;
                }

                // Vista: resime nelistovatelny junction pointy: zmena cesty do cile junction pointu
                char junctTgtPath[MAX_PATH];
                int repPointType;
                if (GetPathDriveType() == DRIVE_FIXED && (dir->Attr & FILE_ATTRIBUTE_REPARSE_POINT) &&
                    GetReparsePointDestination(fullName, junctTgtPath, MAX_PATH, &repPointType, TRUE) &&
                    repPointType == 2 /* JUNCTION POINT */ &&
                    SalPathAppend(fullName, "*", MAX_PATH + 10))
                {
                    WIN32_FIND_DATA fileData;
                    HANDLE search = HANDLES_Q(FindFirstFile(fullName, &fileData));
                    DWORD err = GetLastError();
                    CutDirectory(fullName);
                    if (search != INVALID_HANDLE_VALUE)
                        HANDLES(FindClose(search));
                    else
                    {
                        if (err == ERROR_ACCESS_DENIED)
                        {
                            TopIndexMem.Clear(); // dlouhy skok
                            ChangePathToDisk(HWindow, junctTgtPath);
                            UpdateWindow(HWindow);
                            EndStopRefresh();
                            return;
                        }
                    }
                }

                BOOL noChange;
                BOOL refresh = TRUE;
                if (ChangePathToDisk(HWindow, fullName, -1, NULL, &noChange, FALSE))
                {
                    TopIndexMem.Push(path, topIndex); // zapamatujeme top-index pro navrat
                }
                else // neuspech
                {
                    if (!IsTheSamePath(path, GetPath())) // nejsme na puvodni ceste -> dlouhy skok
                    {                                    // podminka "!noChange" nestaci - rika "zmena cesty nebo jeji znovunacteni - access-denied-dir"
                        TopIndexMem.Clear();
                    }
                    else // nedoslo ke zmene cesty (cesta se ihned zkratila na puvodni)
                    {
                        refresh = FALSE;
                        if (!noChange) // access-denied-dir: doslo k refreshi listingu, ale cesta je shodna
                        {              // vratime listbox na puvodni indexy a dokoncime "refresh" za ChangePathToDisk
                            RefreshListBox(0, topIndex, caretIndex, FALSE, FALSE);
                        }
                    }
                }
                if (refresh)
                {
                    RefreshListBox(0, -1, -1, FALSE, FALSE);
                }
            }
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            if (index >= Dirs->Count)
            {
                UserWorkedOnThisPath = TRUE;
                ExecuteFromArchive(index); // soubor
            }
            else // adresar
            {
                CFileData* dir = &Dirs->At(index);
                if (index == 0 && strcmp(dir->Name, "..") == 0) // ".. <Up>"
                {
                    if (GetZIPPath()[0] == 0) // vypadneme z archivu
                    {
                        const char* s = strrchr(GetZIPArchive(), '\\'); // zip-archive neobsahuje zbytecny '\\' nakonci
                        if (s != NULL)                                  // "always true"
                        {
                            strcpy(path, s + 1); // prev-dir

                            int topIndex; // pristi top-index, -1 -> neplatny
                            if (!TopIndexMem.FindAndPop(GetPath(), topIndex))
                                topIndex = -1;

                            // samotna zmena cesty
                            BOOL noChange;
                            if (!ChangePathToDisk(HWindow, GetPath(), topIndex, path, &noChange))
                            { // nepodarilo se zkratit cestu - reject-close-archive nebo dlouhy skok
                                if (!noChange)
                                    TopIndexMem.Clear(); // dlouhy skok
                                else
                                {
                                    if (topIndex != -1) // pokud se podaril top-index ziskat
                                    {
                                        TopIndexMem.Push(GetPath(), topIndex); // vratime top-index pro priste
                                    }
                                }
                            }
                        }
                    }
                    else // zkracujeme cestu uvnitr archivu
                    {
                        // rozdelime zip-path na novou zip-path a prev-dir
                        strcpy(path, GetZIPPath());
                        char* prevDir;
                        char* s = strrchr(path, '\\'); // zip-path neobsahuje zbytecne backslashe (zacatek/konec)
                        if (s != NULL)                 // format: "beg-path\\dir"
                        {
                            *s = 0;
                            prevDir = s + 1;
                        }
                        else // format: "dir"
                        {
                            memmove(path + 1, path, strlen(path) + 1);
                            *path = 0;
                            prevDir = path + 1;
                        }

                        // sestavime zkracenou cestu do archivu a podle ni ziskame top-index
                        strcpy(doublePath, GetZIPArchive());
                        SalPathAppend(doublePath, path, 2 * MAX_PATH);
                        int topIndex; // pristi top-index, -1 -> neplatny
                        if (!TopIndexMem.FindAndPop(doublePath, topIndex))
                            topIndex = -1;

                        // samotna zmena cesty
                        if (!ChangePathToArchive(GetZIPArchive(), path, topIndex, prevDir)) // "always false"
                        {                                                                   // nepodarilo se zkratit cestu - dlouhy skok
                            TopIndexMem.Clear();
                        }
                    }
                }
                else // podadresar
                {
                    // zaloha udaju pro TopIndexMem (doublePath + topIndex)
                    strcpy(doublePath, GetZIPArchive());
                    SalPathAppend(doublePath, GetZIPPath(), 2 * MAX_PATH);
                    int topIndex = ListBox->GetTopIndex();

                    // nova cesta
                    strcpy(fullName, GetZIPPath());
                    if (!SalPathAppend(fullName, dir->Name, MAX_PATH))
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                      MB_OK | MB_ICONEXCLAMATION);
                    }
                    else
                    {
                        BOOL noChange;
                        if (ChangePathToArchive(GetZIPArchive(), fullName, -1, NULL, FALSE, &noChange)) // "always true"
                        {
                            TopIndexMem.Push(doublePath, topIndex); // zapamatujeme top-index pro navrat
                        }
                        else
                        {
                            if (!noChange)
                                TopIndexMem.Clear(); // neuspech + nejsme na puvodni ceste -> dlouhy skok
                        }
                    }
                }
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                BOOL isDir = index < Dirs->Count ? 1 : 0;
                if (isDir && index == 0 && strcmp(Dirs->At(0).Name, "..") == 0)
                    isDir = 2; // up-dir
                CFileData* file = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                CPluginInterfaceForFSEncapsulation* ifaceForFS = GetPluginFS()->GetPluginInterfaceForFS();
                char fsNameBuf[MAX_PATH]; // GetPluginFS() muze prestt existovat, radsi dame fsName do lokalniho bufferu
                lstrcpyn(fsNameBuf, GetPluginFS()->GetPluginFSName(), MAX_PATH);
                ifaceForFS->ExecuteOnFS(MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT,
                                        GetPluginFS()->GetInterface(), fsNameBuf,
                                        GetPluginFS()->GetPluginFSNameIndex(), *file, isDir);
            }
        }
    }
    UpdateWindow(HWindow);
    EndStopRefresh();
}

void CFilesWindow::ChangeSortType(CSortType newType, BOOL reverse, BOOL force)
{
    CALL_STACK_MESSAGE3("CFilesWindow::ChangeSortType(%d, %d)", newType, reverse);
    if (!force && SortType == newType && !reverse)
        return;
    if (SortType != newType)
    {
        SortType = newType;
        ReverseSort = FALSE;

        //    EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);    // jen pri zmene razeni  // zakomentovano, nevim proc to tu je: Petr
    }
    else
    {
        if (reverse)
        {
            ReverseSort = !ReverseSort;

            //      EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);  // jen pri zmene razeni  // zakomentovano, nevim proc to tu je: Petr
        }
    }

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    //---  ulozeni focusle polozky a setrideni starych polozek dle jmena
    int focusIndex = GetCaretIndex();
    CFileData d1;
    if (focusIndex >= 0 && focusIndex < Dirs->Count + Files->Count)
        d1 = (focusIndex < Dirs->Count) ? Dirs->At(focusIndex) : Files->At(focusIndex - Dirs->Count);
    else
        d1.Name = NULL;
    //---  setrideni
    if (UseSystemIcons || UseThumbnails)
        SleepIconCacheThread();
    SortDirectory();
    if (UseSystemIcons || UseThumbnails)
        WakeupIconCacheThread();
    //---  vyber polozky pro focus + konecne setrideni
    CLessFunction lessDirs;  // pro porovnani co je mensi; potrebne, viz optimalizace v hledacim cyklu
    CLessFunction lessFiles; // pro porovnani co je mensi; potrebne, viz optimalizace v hledacim cyklu
    switch (SortType)
    {
    case stName:
        lessDirs = lessFiles = LessNameExt;
        break;
    case stExtension:
        lessDirs = lessFiles = LessExtName;
        break;

    case stTime:
    {
        if (Configuration.SortDirsByName)
            lessDirs = LessNameExt;
        else
            lessDirs = LessTimeNameExt;
        lessFiles = LessTimeNameExt;
        break;
    }

    case stAttr:
        lessDirs = lessFiles = LessAttrNameExt;
        break;
    default: /*stSize*/
        lessDirs = lessFiles = LessSizeNameExt;
        break;
    }

    int i;
    int count;
    if (focusIndex < Dirs->Count) // hledame adresar
    {
        i = 0;
        count = Dirs->Count;
    }
    else
    {
        i = Dirs->Count;                    // hledame soubor
        count = Dirs->Count + Files->Count; // tady byla chyba; count nebyla inicializovana
    }

    if (d1.Name != NULL)
    {
        if (i == 0 && Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
        {
            if (strcmp(d1.Name, "..") == 0)
            {
                focusIndex = 0;
                i = count;
            }
            else
                i = 1; // ".." musime preskocit, neni zarazene
        }
        for (; i < count; i++)
        {
            if (i < Dirs->Count)
            {
                CFileData* d2 = &Dirs->At(i);
                if (!lessDirs(*d2, d1, ReverseSort)) // diky setrideni bude TRUE az na hledane polozce
                {
                    if (!lessDirs(d1, *d2, ReverseSort))
                        focusIndex = i; // podminka zbytecna, mela by byt "always true"
                    break;
                }
            }
            else
            {
                CFileData* d2 = &Files->At(i - Dirs->Count);
                if (!lessFiles(*d2, d1, ReverseSort)) // diky setrideni bude TRUE az na hledane polozce
                {
                    if (!lessFiles(d1, *d2, ReverseSort))
                        focusIndex = i; // podminka zbytecna, mela by byt "always true"
                    break;
                }
            }
        }
    }
    if (focusIndex >= count)
        focusIndex = count - 1;
    //---  pouziti ziskanych dat pro konecne nastaveni listboxu
    SetCaretIndex(focusIndex, FALSE); // focus
    IdleRefreshStates = TRUE;         // pri pristim Idle vynutime kontrolu stavovych promennych
    ListBox->PaintHeaderLine();
    RepaintListBox(DRAWFLAG_SKIP_VISTEST);
    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
}

BOOL CFilesWindow::ChangeToRescuePathOrFixedDrive(HWND parent, BOOL* noChange, BOOL refreshListBox,
                                                  BOOL canForce, int tryCloseReason, int* failReason)
{
    CALL_STACK_MESSAGE4("CFilesWindow::ChangeToRescuePathOrFixedDrive(, , %d, %d, %d,)",
                        refreshListBox, canForce, tryCloseReason);
    BOOL noChangeUsed = FALSE;
    char ifPathIsInaccessibleGoTo[MAX_PATH];
    GetIfPathIsInaccessibleGoTo(ifPathIsInaccessibleGoTo);
    if (ifPathIsInaccessibleGoTo[0] == '\\' && ifPathIsInaccessibleGoTo[1] == '\\' ||
        ifPathIsInaccessibleGoTo[0] != 0 && ifPathIsInaccessibleGoTo[1] == ':')
    {
        int failReasonInt;
        BOOL ret = ChangePathToDisk(parent, ifPathIsInaccessibleGoTo, -1, NULL, noChange, refreshListBox,
                                    canForce, FALSE, &failReasonInt, TRUE, tryCloseReason);
        if (!ret && failReasonInt != CHPPFR_CANNOTCLOSEPATH)
            OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;
        if (ret || failReasonInt == CHPPFR_SHORTERPATH)
        {
            if (failReason != NULL)
                *failReason = CHPPFR_SUCCESS; // zkraceni "zachrane" cesty nebereme jako neuspech
            return TRUE;
        }
        if (failReasonInt == CHPPFR_CANNOTCLOSEPATH)
        {
            if (failReason != NULL)
                *failReason = failReasonInt;
            return FALSE; // problem "nelze zavrit aktualni cestu v panelu" nevyresi ani prechod na fixed-drive (jen se pak hloupe dvakrat po sobe stejne ptame na disconnect)
        }
        noChangeUsed = TRUE;
    }
    else
    {
        if (!CriticalShutdown)
        {
            SalMessageBox(parent, LoadStr(IDS_INVALIDESCAPEPATH), LoadStr(IDS_ERRORCHANGINGDIR),
                          MB_OK | MB_ICONEXCLAMATION);
            OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;
        }
    }
    return ChangeToFixedDrive(parent, (!noChangeUsed || noChange != NULL && *noChange) ? noChange : NULL,
                              refreshListBox, canForce, failReason, tryCloseReason);
}

BOOL CFilesWindow::ChangeToFixedDrive(HWND parent, BOOL* noChange, BOOL refreshListBox, BOOL canForce,
                                      int* failReason, int tryCloseReason)
{
    CALL_STACK_MESSAGE4("CFilesWindow::ChangeToFixedDrive(, , %d, %d, , %d)",
                        refreshListBox, canForce, tryCloseReason);
    if (noChange != NULL)
        *noChange = TRUE;
    char sysDir[MAX_PATH];
    char root[4] = " :\\";
    if (GetWindowsDirectory(sysDir, MAX_PATH) != 0 && sysDir[0] != 0 && sysDir[1] == ':')
    {
        root[0] = sysDir[0];
        if (GetDriveType(root) == DRIVE_FIXED)
        {
            TopIndexMem.Clear(); // dlouhy skok
            return ChangePathToDisk(parent, root, -1, NULL, noChange, refreshListBox, canForce,
                                    FALSE, failReason, TRUE, tryCloseReason);
        }
    }
    DWORD disks = GetLogicalDrives();
    disks >>= 2; // preskocime A: a B:, pri formatovani disket se nekde stavaji DRIVE_FIXED
    char d = 'C';
    while (d <= 'Z')
    {
        if (disks & 1)
        {
            root[0] = d;
            if (GetDriveType(root) == DRIVE_FIXED)
            {
                TopIndexMem.Clear(); // dlouhy skok
                return ChangePathToDisk(parent, root, -1, NULL, noChange, refreshListBox, canForce,
                                        FALSE, failReason, TRUE, tryCloseReason);
            }
        }
        disks >>= 1;
        d++;
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

void CFilesWindow::ConnectNet(BOOL readOnlyUNC, const char* netRootPath, BOOL changeToNewDrive,
                              char* newlyMappedDrive)
{
    CALL_STACK_MESSAGE3("CFilesWindow::ConnectNet(%s, %d,)", netRootPath, changeToNewDrive);

    if (newlyMappedDrive != NULL)
        *newlyMappedDrive = 0;

    if (SystemPolicies.GetNoNetConnectDisconnect())
    {
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = HWindow;
        params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
        params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
        params.Text = LoadStr(IDS_POLICIESRESTRICTION);
        params.ContextHelpId = IDH_GROUPPOLICY;
        params.HelpCallback = MessageBoxHelpCallback;
        SalMessageBoxEx(&params);
        return;
    }

    BeginStopRefresh(); // cmuchal si da pohov

    DWORD disks = changeToNewDrive || newlyMappedDrive != NULL ? GetLogicalDrives() : 0;

    BOOL success;
    const char* netPath = netRootPath == NULL ? GetPath() : netRootPath;
    if (netPath[0] == '\\' && netPath[1] == '\\') // UNC cesta
    {
        CONNECTDLGSTRUCT cs;
        cs.cbStructure = sizeof(cs);
        cs.hwndOwner = HWindow;
        NETRESOURCE nr;
        memset(&nr, 0, sizeof(nr));
        char root[MAX_PATH];
        GetRootPath(root, netPath);
        root[strlen(root) - 1] = 0;
        nr.lpRemoteName = root;
        nr.dwType = RESOURCETYPE_DISK;
        cs.lpConnRes = &nr;
        cs.dwFlags = readOnlyUNC ? CONNDLG_RO_PATH : CONNDLG_USE_MRU;
        success = WNetConnectionDialog1(&cs) == WN_SUCCESS;
    }
    else
        success = WNetConnectionDialog(HWindow, RESOURCETYPE_DISK) == NO_ERROR;

    if ((changeToNewDrive || newlyMappedDrive != NULL) && success)
    {
        disks = (GetLogicalDrives() ^ disks);
        if (disks != 0)
        {
            char d = 'A';
            while ((disks >>= 1) != 0)
                d++;
            UpdateWindow(MainWindow->HWindow);
            if (d >= 'A' && d <= 'Z') // always true
            {
                if (newlyMappedDrive != NULL)
                    *newlyMappedDrive = d;
                if (changeToNewDrive)
                {
                    char root[4] = " :\\";
                    root[0] = d;
                    ChangePathToDisk(HWindow, root, -1, NULL, NULL, TRUE, FALSE, FALSE, NULL, FALSE);
                }
            }
        }
    }

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::DisconnectNet()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DisconnectNet()");

    if (SystemPolicies.GetNoNetConnectDisconnect())
    {
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = HWindow;
        params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
        params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
        params.Text = LoadStr(IDS_POLICIESRESTRICTION);
        params.ContextHelpId = IDH_GROUPPOLICY;
        params.HelpCallback = MessageBoxHelpCallback;
        SalMessageBoxEx(&params);
        return;
    }

    BeginSuspendMode(); // cmuchal si da pohov

    SetCurrentDirectoryToSystem(); // aby sel odmapovat i disk z panelu

    // odpojime se od mapovaneho disku, jinak nepujde odpojit "bez reci" (system varuje, ze je pouzivan)
    BOOL releaseLeft = MainWindow->LeftPanel->GetNetworkDrive() &&    // sitovy disk (jen ptDisk)
                       MainWindow->LeftPanel->GetPath()[0] != '\\';   // ne UNC
    BOOL releaseRight = MainWindow->RightPanel->GetNetworkDrive() &&  // sitovy disk (jen ptDisk)
                        MainWindow->RightPanel->GetPath()[0] != '\\'; // ne UNC
    if (releaseLeft)
        MainWindow->LeftPanel->HandsOff(TRUE);
    if (releaseRight)
        MainWindow->RightPanel->HandsOff(TRUE);

    //  Pod Windows XP je WNetDisconnectDialog dialog nemodalni. Uzivatelum zapadnul za Salamandera
    //  a oni se divili proc nechodi akceleratory. Pak pri zavirani Salamander padnul v teto metode
    //  protoze MainWindow bylo NULL;
    //  WNetDisconnectDialog(HWindow, RESOURCETYPE_DISK);

    CDisconnectDialog dlg(this);
    if (dlg.Execute() == IDCANCEL && dlg.NoConnection())
    {
        // dialog se ani nezobrazil, protoze obsahoval 0 resourcu -- zobrazime info
        SalMessageBox(HWindow, LoadStr(IDS_DISCONNECT_NODRIVES),
                      LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
    }

    if (releaseLeft)
        MainWindow->LeftPanel->HandsOff(FALSE);
    if (releaseRight)
        MainWindow->RightPanel->HandsOff(FALSE);

    if (MainWindow->LeftPanel->CheckPath(FALSE) != ERROR_SUCCESS)
        MainWindow->LeftPanel->ChangeToRescuePathOrFixedDrive(MainWindow->LeftPanel->HWindow);
    if (MainWindow->RightPanel->CheckPath(FALSE) != ERROR_SUCCESS)
        MainWindow->RightPanel->ChangeToRescuePathOrFixedDrive(MainWindow->RightPanel->HWindow);

    EndSuspendMode(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::DriveInfo()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DriveInfo()");
    if (Is(ptDisk) || Is(ptZIPArchive))
    {
        if (CheckPath(TRUE) != ERROR_SUCCESS)
            return;

        BeginStopRefresh(); // cmuchal si da pohov

        CDriveInfo dlg(HWindow, GetPath());
        dlg.Execute();
        UpdateWindow(MainWindow->HWindow);

        EndStopRefresh(); // ted uz zase cmuchal nastartuje
    }
    else
    {
        if (Is(ptPluginFS) &&
            GetPluginFS()->NotEmpty() &&
            GetPluginFS()->IsServiceSupported(FS_SERVICE_SHOWINFO))
        {
            GetPluginFS()->ShowInfoDialog(GetPluginFS()->GetPluginFSName(), HWindow);
            UpdateWindow(MainWindow->HWindow);
        }
    }
}

void CFilesWindow::ToggleDirectoryLine()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ToggleDirectoryLine()");
    if (HWindow == NULL)
    {
        TRACE_E("HWindow == NULL");
        return;
    }
    if (DirectoryLine->HWindow != NULL) // vypnuti
    {
        DirectoryLine->ToggleToolBar();
        DestroyWindow(DirectoryLine->HWindow);
    }
    else // zapnuti
    {
        if (!DirectoryLine->Create(CWINDOW_CLASSNAME2,
                                   "",
                                   WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                                   0, 0, 0, 0,
                                   HWindow,
                                   (HMENU)IDC_DIRECTORYLINE,
                                   HInstance,
                                   DirectoryLine))
            TRACE_E("Unable to create directory-line.");
        IdleForceRefresh = TRUE;  // forcneme update
        IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
    }
    InvalidateRect(HWindow, NULL, TRUE);
    RECT r;
    GetClientRect(HWindow, &r);
    SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                MAKELONG(r.right - r.left, r.bottom - r.top));
    if (DirectoryLine->HWindow != NULL)
    {
        UpdateDriveIcon(TRUE);
        ShowWindow(DirectoryLine->HWindow, SW_SHOW);
    }
    // pokud je zobrazena middle toolbar, dame ji prilezitost se umistit
    if (MainWindow->MiddleToolBar != NULL && MainWindow->MiddleToolBar->HWindow != NULL)
        MainWindow->LayoutWindows();
}

void CFilesWindow::ToggleStatusLine()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ToggleStatusLine()");
    if (HWindow == NULL)
    {
        TRACE_E("HWindow == NULL");
        return;
    }
    if (StatusLine->HWindow != NULL) // vypnuti
        DestroyWindow(StatusLine->HWindow);
    else // zapnuti
    {
        if (!StatusLine->Create(CWINDOW_CLASSNAME2,
                                "",
                                WS_CHILD | WS_CLIPSIBLINGS,
                                0, 0, 0, 0,
                                HWindow,
                                (HMENU)IDC_STATUSLINE,
                                HInstance,
                                StatusLine))
            TRACE_E("Unable to create information-line.");
    }
    RECT r;
    GetClientRect(HWindow, &r);
    SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                MAKELONG(r.right - r.left, r.bottom - r.top));
    if (StatusLine->HWindow != NULL)
        ShowWindow(StatusLine->HWindow, SW_SHOW);
}

void CFilesWindow::ToggleHeaderLine()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ToggleHeaderLine()");
    BOOL headerLine = !HeaderLineVisible;
    if (GetViewMode() == vmBrief)
        headerLine = FALSE;
    ListBox->SetMode(GetViewMode() == vmBrief ? vmBrief : vmDetailed, headerLine);
}

int CFilesWindow::GetViewTemplateIndex()
{
    return (int)(ViewTemplate - MainWindow->ViewTemplates.Items);
}

int CFilesWindow::GetNextTemplateIndex(BOOL forward, BOOL wrap)
{
    int oldIndex = GetViewTemplateIndex();
    int newIndex = oldIndex;
    int delta = forward ? 1 : -1;
    do
    {
        newIndex += delta;
        if (wrap)
        {
            if (forward)
            {
                if (newIndex > 9)
                    newIndex = 1; // krajni polozka byla nulova, skocime na druhou stranu seznamu
            }
            else
            {
                if (newIndex < 1)
                    newIndex = 9; // krajni polozka byla nulova, skocime na druhou stranu seznamu
            }
        }
        else
        {
            if (forward)
            {
                if (newIndex > 9)
                    newIndex = oldIndex; // krajni polozka byla nulova, vracime se na posledni validni
            }
            else
            {
                if (newIndex < 1)
                    newIndex = oldIndex; // krajni polozka byla nulova, vracime se na posledni validni
            }
        }
    } while (Parent->ViewTemplates.Items[newIndex].Name[0] == 0 && newIndex != oldIndex);
    return newIndex;
}

BOOL CFilesWindow::IsViewTemplateValid(int templateIndex)
{
    CALL_STACK_MESSAGE2("CFilesWindow::IsViewTemplateValid(%d)", templateIndex);
    if (templateIndex < 1) // tree zatim neumime
        return FALSE;
    CViewTemplate* newTemplate = &Parent->ViewTemplates.Items[templateIndex];
    if (lstrlen(newTemplate->Name) == 0)
        return FALSE;
    return TRUE;
}

BOOL CFilesWindow::SelectViewTemplate(int templateIndex, BOOL canRefreshPath,
                                      BOOL calledFromPluginBeforeListing, DWORD columnValidMask,
                                      BOOL preserveTopIndex, BOOL salamanderIsStarting)
{
    CALL_STACK_MESSAGE5("CFilesWindow::SelectViewTemplate(%d, %d, %d, 0x%X)", templateIndex,
                        canRefreshPath, calledFromPluginBeforeListing, columnValidMask);

    if (templateIndex == 0)
        return FALSE;
    CViewTemplate* newTemplate = &Parent->ViewTemplates.Items[templateIndex];
    if (lstrlen(newTemplate->Name) == 0)
    {
        // nedefinovany pohled nechceme - vnutime detailed, ktery urcite existuje
        templateIndex = 2;
        newTemplate = &Parent->ViewTemplates.Items[templateIndex];
    }

    CViewModeEnum oldViewMode = GetViewMode();
    if (!calledFromPluginBeforeListing || ViewTemplate != newTemplate)
    {
        CViewModeEnum newViewMode;
        switch (templateIndex)
        {
            //      case 0: newViewMode = vmTree; break;
        case 1:
            newViewMode = vmBrief;
            break;
        case 3:
            newViewMode = vmIcons;
            break;
        case 4:
            newViewMode = vmThumbnails;
            break;
        case 5:
            newViewMode = vmTiles;
            break;
        default:
            newViewMode = vmDetailed;
            break;
        }
        ViewTemplate = newTemplate;

        BOOL headerLine = HeaderLineVisible;
        if (newViewMode != vmDetailed)
            headerLine = FALSE;
        ListBox->SetMode(newViewMode, headerLine);
    }

    // postavime nove sloupce
    BuildColumnsTemplate();
    CopyColumnsTemplateToColumns();
    DeleteColumnsWithoutData(columnValidMask);

    if (!calledFromPluginBeforeListing)
    {
        if (PluginData.NotEmpty())
        {
            CSalamanderView view(this);
            PluginData.SetupView(this == MainWindow->LeftPanel,
                                 &view, Is(ptZIPArchive) ? GetZIPPath() : NULL,
                                 Is(ptZIPArchive) ? GetArchiveDir()->GetUpperDir(GetZIPPath()) : NULL);
        }

        // jakmile se lisi view-mode, je potreba refreshnout (je potreba nacist jinou velikost
        // ikon nebo thumbnaily) - jedina vyjimka je brief a detailed, ty maji oba stejne ikony
        BOOL needRefresh = oldViewMode != GetViewMode() &&
                           (oldViewMode != vmBrief || GetViewMode() != vmDetailed) &&
                           (oldViewMode != vmDetailed || GetViewMode() != vmBrief);
        if (canRefreshPath && needRefresh)
        {
            // az do ReadDirectory pojedeme nad simple icons, protoze se meni geometrie ikonek
            TemporarilySimpleIcons = TRUE;

            // nechame napocitat rozmery polozek a zajistime viditelnost focused polozky
            RefreshListBox(0, -1, FocusedIndex, FALSE, FALSE);

            // provedeme tvrdy refresh
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }
        else
        {
            if (needRefresh)
            {
                if (!salamanderIsStarting)
                    TRACE_E("CFilesWindow::SelectViewTemplate(): unexpected situation: refresh is needed, but it's not allowed!");
                // prozatim aspon: az do dalsiho refreshe (a tim ReadDirectory) pojedeme nad simple icons, protoze se meni geometrie ikonek
                TemporarilySimpleIcons = TRUE;
            }
            // nechame napocitat rozmery polozek a zajistime viditelnost focused polozky
            // je-li 'preserveTopIndex' TRUE, nemame nechat panel odrolvat za focusem
            RefreshListBox(-1, preserveTopIndex ? ListBox->TopIndex : -1, FocusedIndex, FALSE, FALSE);
        }
    }

    // pri zmene rezimu panelu musime vycistit pamet top-indexu (ukladaji se nekompatibilni data)
    if (oldViewMode != GetViewMode())
        TopIndexMem.Clear();

    return TRUE;
}

BOOL CFilesWindow::IsExtensionInSeparateColumn()
{
    return Columns.Count >= 2 && Columns.At(1).ID == COLUMN_ID_EXTENSION;
}

void CFilesWindow::RedrawIndex(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::RedrawIndex(%d)", index);
    if (index >= 0 && index < Dirs->Count + Files->Count)
        ListBox->PaintItem(index, DRAWFLAG_SELFOC_CHANGE);
    else if (Dirs->Count + Files->Count == 0)
        ListBox->PaintAllItems(NULL, 0); // zajistime vykresleni textu o prazdnem panelu
}

void CFilesWindow::ItemFocused(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::ItemFocused(%d)", index);
    if (GetSelCount() == 0 && index != LastFocus && index >= 0 &&
        index < Dirs->Count + Files->Count)
    {
        LastFocus = index;
        CFileData* f = (index < Dirs->Count) ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

        char buff[1000];
        DWORD varPlacements[100];
        int varPlacementsCount = 100;
        BOOL done = FALSE;
        if (Is(ptZIPArchive) || Is(ptPluginFS))
        {
            if (PluginData.NotEmpty())
            {
                if (PluginData.GetInfoLineContent(MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT,
                                                  f, index < Dirs->Count, 0, 0, TRUE, CQuadWord(0, 0), buff,
                                                  varPlacements, varPlacementsCount))
                {
                    done = TRUE;
                }
                else
                    varPlacementsCount = 100; // mohlo se poskodit
            }
        }

        if (done ||
            ExpandInfoLineItems(HWindow, Configuration.InfoLineContent, &PluginData, f,
                                index < Dirs->Count, buff, 1000,
                                varPlacements, &varPlacementsCount, ValidFileData, Is(ptDisk)))
        {
            if (StatusLine->SetText(buff))
                StatusLine->SetSubTexts(varPlacements, varPlacementsCount);
        }
    }
    IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
}

void CFilesWindow::SetValidFileData(DWORD validFileData)
{
    DWORD mask = 0xFFFFFFFF;
    if (!PluginData.NotEmpty())
        mask &= ~VALID_DATA_PL_SIZE & ~VALID_DATA_PL_DATE & ~VALID_DATA_PL_TIME;
    else
    {
        if (validFileData & VALID_DATA_SIZE)
            mask &= ~VALID_DATA_PL_SIZE;
        if (validFileData & VALID_DATA_DATE)
            mask &= ~VALID_DATA_PL_DATE;
        if (validFileData & VALID_DATA_TIME)
            mask &= ~VALID_DATA_PL_TIME;
    }
    ValidFileData = validFileData & mask;
}

BOOL CFilesWindow::PrepareCloseCurrentPath(HWND parent, BOOL canForce, BOOL canDetach, BOOL& detachFS,
                                           int tryCloseReason)
{
    CALL_STACK_MESSAGE4("CFilesWindow::PrepareCloseCurrentPath(, %d, %d, , %d)",
                        canForce, canDetach, tryCloseReason);
    char buf[2 * MAX_PATH + 100];

    if (Is(ptDisk))
    {
        detachFS = FALSE;
        return TRUE; // cesta na disku jde zavrit vzdycky
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            BOOL someFilesChanged = FALSE;
            if (AssocUsed)
            {
                // pokud to user nepotlacil, zobrazime info o zavirani archivu ve kterem jsou editovane soubory
                if (Configuration.CnfrmCloseArchive && !CriticalShutdown)
                {
                    char title[100];
                    char text[MAX_PATH + 500];
                    char checkText[200];
                    sprintf(title, LoadStr(IDS_INFOTITLE));
                    sprintf(text, LoadStr(IDS_ARCHIVECLOSEEDIT), GetZIPArchive());
                    sprintf(checkText, LoadStr(IDS_DONTSHOWAGAIN));
                    BOOL dontShow = !Configuration.CnfrmCloseArchive;

                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = parent;
                    params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                    params.Caption = title;
                    params.Text = text;
                    params.CheckBoxText = checkText;
                    params.CheckBoxValue = &dontShow;
                    SalMessageBoxEx(&params);

                    Configuration.CnfrmCloseArchive = !dontShow;
                }
                // zapakujeme zmenene soubory (jen nejde-li o critical shutdown), pripravime pro dalsi pouziti
                UnpackedAssocFiles.CheckAndPackAndClear(parent, &someFilesChanged);
                // pri critical shutdown se tvarime, ze updatle soubory neexistuji, zabalit zpet do archivu bysme
                // je nestihly, ale smazat je nesmime, po startu musi zustat uzivateli sance updatle soubory
                // rucne do archivu zabalit; vyjimka je pokud nebylo nic editovano, to lze vse smazat i pri
                // critical shutdown (to je rychle a nebudeme pri startu mast usera zbytecnymi dotazy)
                if (!someFilesChanged || !CriticalShutdown)
                {
                    SetEvent(ExecuteAssocEvent); // spustime uklid souboru
                    DiskCache.WaitForIdle();
                    ResetEvent(ExecuteAssocEvent); // ukoncime uklid souboru
                }
            }
            AssocUsed = FALSE;
            // pokud muzou byt v diskcache editovane soubory nebo tento archiv neni otevren
            // i v druhem panelu, vyhodime jeho cachovane soubory, pri dalsim otevreni se bude
            // znovu vypakovavat (archiv muze byt mezitim editovan)
            CFilesWindow* another = (MainWindow->LeftPanel == this) ? MainWindow->RightPanel : MainWindow->LeftPanel;
            if (someFilesChanged || !another->Is(ptZIPArchive) || StrICmp(another->GetZIPArchive(), GetZIPArchive()) != 0)
            {
                StrICpy(buf, GetZIPArchive()); // v disk-cache je jmeno archivu malymi pismeny (umozni case-insensitive porovnani jmena z Windows file systemu)
                DiskCache.FlushCache(buf);
            }

            // zavolame plug-inove CPluginInterfaceAbstract::CanCloseArchive
            BOOL canclose = TRUE;
            int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
            if (format != 0) // nasli jsme podporovany archiv
            {
                format--;
                BOOL userForce = FALSE;
                BOOL userAsked = FALSE;
                CPluginData* plugin = NULL;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0) // view: jde o interni zpracovani (plug-in)?
                {
                    plugin = Plugins.Get(-index - 1);
                    if (plugin != NULL)
                    {
                        if (!plugin->CanCloseArchive(this, GetZIPArchive(), CriticalShutdown)) // nechce se mu zavrit
                        {
                            canclose = FALSE;
                            if (canForce) // muzeme se zeptat usera, jestli forcovat?
                            {
                                sprintf(buf, LoadStr(IDS_ARCHIVEFORCECLOSE), GetZIPArchive());
                                userAsked = TRUE;
                                if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                                  MB_YESNO | MB_ICONQUESTION) == IDYES) // user rika "zavrit"
                                {
                                    userForce = TRUE;
                                    plugin->CanCloseArchive(this, GetZIPArchive(), TRUE); // force==TRUE
                                    canclose = TRUE;
                                }
                            }
                        }
                    }
                }
                if (PackerFormatConfig.GetUsePacker(format)) // ma edit?
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0) // jde o interni zpracovani (plug-in)?
                    {
                        if (plugin != Plugins.Get(-index - 1)) // je-li view==edit, nevolat podruhe
                        {
                            plugin = Plugins.Get(-index - 1);
                            if (plugin != NULL)
                            {
                                if (!plugin->CanCloseArchive(this, GetZIPArchive(), userForce || CriticalShutdown)) // nechce se mu zavrit
                                {
                                    canclose = FALSE;
                                    if (canForce && !userAsked) // muzeme se zeptat usera, jestli forcovat?
                                    {
                                        sprintf(buf, LoadStr(IDS_ARCHIVEFORCECLOSE), GetZIPArchive());
                                        if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                                          MB_YESNO | MB_ICONQUESTION) == IDYES) // user rika "zavrit"
                                        {
                                            plugin->CanCloseArchive(this, GetZIPArchive(), TRUE);
                                            canclose = TRUE;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return canclose; // zatim rozhoduji jen plug-inove CPluginInterfaceAbstract::CanCloseArchive
        }
        else
        {
            if (Is(ptPluginFS))
            {
                if (!canForce && !CriticalShutdown) // nemuzeme se usera ptat na force
                {
                    detachFS = FALSE; // kdyby na to plug-in nesahl, tak aby tam byla znama hodnota
                    BOOL r = GetPluginFS()->TryCloseOrDetach(FALSE, canDetach, detachFS, tryCloseReason);
                    if (!r || !canDetach)
                        detachFS = FALSE; // kontrola/korekce vystupni hodnoty
                    return r;
                }
                else // muzeme forcovat -> musime zavirat, odpojovani neni dovolene
                {
                    if (GetPluginFS()->TryCloseOrDetach(CriticalShutdown, FALSE, detachFS, tryCloseReason) || // test zavreni bez forceClose==TRUE (krome critical shutdownu)
                        CriticalShutdown)
                    {
                        detachFS = FALSE;
                        return TRUE; // zavreni o.k.
                    }
                    else // optame se usera, jestli to chce zavrit i proti vuli FS
                    {
                        char path[2 * MAX_PATH];
                        GetGeneralPath(path, 2 * MAX_PATH);
                        sprintf(buf, LoadStr(IDS_FSFORCECLOSE), path);
                        if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                          MB_YESNO | MB_ICONQUESTION) == IDYES) // user rika "zavrit"
                        {
                            GetPluginFS()->TryCloseOrDetach(TRUE, FALSE, detachFS, tryCloseReason);
                            detachFS = FALSE;
                            return TRUE; // zavreni o.k.
                        }
                        else
                        {
                            detachFS = FALSE;
                            return FALSE; // nelze zavrit
                        }
                    }
                }
            }
            else
            {
                TRACE_E("Unexpected situation in CFilesWindow::PrepareCloseCurrentPath()");
                return FALSE;
            }
        }
    }
}

void CFilesWindow::CloseCurrentPath(HWND parent, BOOL cancel, BOOL detachFS, BOOL newPathIsTheSame,
                                    BOOL isRefresh, BOOL canChangeSourceUID)
{
    CALL_STACK_MESSAGE6("CFilesWindow::CloseCurrentPath(, %d, %d, %d, %d, %d)",
                        cancel, detachFS, newPathIsTheSame, isRefresh, canChangeSourceUID);

    if (Is(ptDisk))
    {
        if (!cancel)
        {
            if (UserWorkedOnThisPath)
            {
                const char* path = GetPath();
                // HICON hIcon = GetFileOrPathIconAux(path, FALSE, TRUE); // vytahneme ikonu
                MainWindow->DirHistoryAddPathUnique(0, path, NULL, NULL /*hIcon*/, NULL, NULL);
                if (!newPathIsTheSame)
                    UserWorkedOnThisPath = FALSE;
            }

            if (!newPathIsTheSame)
            {
                // opoustime cestu
                HiddenNames.Clear();  // uvolnime skryte nazvy
                OldSelection.Clear(); // a starou selection
            }

            if (!isRefresh && canChangeSourceUID)
                EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

            ReleaseListing();
            SetValidFileData(VALID_DATA_ALL);
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            if (!cancel)
            {
                if (UserWorkedOnThisPath)
                {
                    MainWindow->DirHistoryAddPathUnique(1, GetZIPArchive(), GetZIPPath(), NULL, NULL, NULL);
                    if (!newPathIsTheSame)
                        UserWorkedOnThisPath = FALSE;
                }

                if (!newPathIsTheSame)
                {
                    // opoustime cestu
                    HiddenNames.Clear();  // uvolnime skryte nazvy
                    OldSelection.Clear(); // a starou selection
                }

                if (!isRefresh && canChangeSourceUID)
                    EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

                // pokud se data archivu hodi SalShExtPastedData, predame mu je
                if (SalShExtPastedData.WantData(GetZIPArchive(), GetArchiveDir(), PluginData,
                                                GetZIPArchiveDate(), GetZIPArchiveSize()))
                {
                    SetArchiveDir(NULL);
                    PluginData.Init(NULL, NULL, NULL, NULL, 0);
                }

                ReleaseListing();
                if (GetArchiveDir() != NULL)
                    delete GetArchiveDir();
                SetArchiveDir(NULL);
                SetPluginIface(NULL);
                // jeste par zbytecnych nulovani, jen tak pro vetsi prehlednost
                SetZIPArchive("");
                SetZIPPath("");

                SetPanelType(ptDisk); // z bezpecnostnich duvodu (disk nema zadna PluginData, atd.)
                SetValidFileData(VALID_DATA_ALL);
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                if (!cancel)
                {
                    BOOL sendDetachEvent = FALSE;
                    CPluginFSInterfaceEncapsulation* detachedFS = NULL;

                    char buf[MAX_PATH];
                    if (GetPluginFS()->GetCurrentPath(buf))
                    {
                        if (UserWorkedOnThisPath)
                        {
                            MainWindow->DirHistoryAddPathUnique(2, GetPluginFS()->GetPluginFSName(), buf, NULL,
                                                                GetPluginFS()->GetInterface(), GetPluginFS());
                            if (!newPathIsTheSame)
                                UserWorkedOnThisPath = FALSE;
                        }
                        if (!newPathIsTheSame)
                        {
                            // opoustime cestu
                            HiddenNames.Clear();  // uvolnime skryte nazvy
                            OldSelection.Clear(); // a starou selection
                        }
                    }

                    if (detachFS) // jen odpojujeme -> pridani do MainWindow->DetachedFS
                    {
                        detachedFS = new CPluginFSInterfaceEncapsulation(GetPluginFS()->GetInterface(),
                                                                         GetPluginFS()->GetDLLName(),
                                                                         GetPluginFS()->GetVersion(),
                                                                         GetPluginFS()->GetPluginInterfaceForFS()->GetInterface(),
                                                                         GetPluginFS()->GetPluginInterface(),
                                                                         GetPluginFS()->GetPluginFSName(),
                                                                         GetPluginFS()->GetPluginFSNameIndex(),
                                                                         GetPluginFS()->GetPluginFSCreateTime(),
                                                                         GetPluginFS()->GetChngDrvDuplicateItemIndex(),
                                                                         GetPluginFS()->GetBuiltForVersion());
                        MainWindow->DetachedFSList->Add(detachedFS);
                        if (!MainWindow->DetachedFSList->IsGood())
                        {
                            MainWindow->DetachedFSList->ResetState();
                            delete detachedFS;
                            detachedFS = NULL;
                        }
                        else
                            sendDetachEvent = TRUE; // volani nesmi byt primo zde, protoze FS jeste neni odpojen (je stale v panelu)
                    }

                    if (!detachFS) // zavirame FS, nechame ho dealokovat + vypsat posledni messageboxy
                    {
                        GetPluginFS()->ReleaseObject(parent);
                    }

                    if (!isRefresh && canChangeSourceUID)
                        EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

                    ReleaseListing();
                    delete GetPluginFSDir();
                    SetPluginFSDir(NULL);
                    SetPluginIconsType(pitSimple);

                    if (SimplePluginIcons != NULL)
                    {
                        delete SimplePluginIcons;
                        SimplePluginIcons = NULL;
                    }

                    if (!detachFS) // zavirame FS
                    {
                        CPluginInterfaceForFSEncapsulation plugin(GetPluginFS()->GetPluginInterfaceForFS()->GetInterface(),
                                                                  GetPluginFS()->GetPluginInterfaceForFS()->GetBuiltForVersion());
                        if (plugin.NotEmpty())
                            plugin.CloseFS(GetPluginFS()->GetInterface());
                        else
                            TRACE_E("Unexpected situation (2) in CFilesWindow::CloseCurrentPath()");
                    }
                    SetPluginFS(NULL, NULL, NULL, NULL, NULL, NULL, -1, 0, 0, 0);
                    SetPluginIface(NULL);

                    SetPanelType(ptDisk); // z bezpecnostnich duvodu (disk nema zadna PluginData, atd.)
                    SetValidFileData(VALID_DATA_ALL);

                    if (sendDetachEvent && detachedFS != NULL /* always true */)
                        detachedFS->Event(FSE_DETACHED, GetPanelCode()); // dame zpravu o uspesnem odpojeni plug-inu
                }
                else
                    GetPluginFS()->Event(FSE_CLOSEORDETACHCANCELED, GetPanelCode());
            }
            else
                TRACE_E("Unexpected situation (1) in CFilesWindow::CloseCurrentPath()");
        }
    }
}

void CFilesWindow::RefreshPathHistoryData()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RefreshPathHistoryData()");

    int index = GetCaretIndex();
    if (index >= 0 && index < Files->Count + Dirs->Count) // pokud nejsou rozjeta data
    {
        int topIndex = ListBox->GetTopIndex();
        CFileData* file = index < Dirs->Count ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

        // zkusime zapsat novy top-index a focus-name
        if (Is(ptZIPArchive))
        {
            PathHistory->ChangeActualPathData(1, GetZIPArchive(), GetZIPPath(), NULL, NULL, topIndex, file->Name);
        }
        else
        {
            if (Is(ptDisk))
            {
                PathHistory->ChangeActualPathData(0, GetPath(), NULL, NULL, NULL, topIndex, file->Name);
            }
            else
            {
                if (Is(ptPluginFS))
                {
                    char curPath[MAX_PATH];
                    if (GetPluginFS()->NotEmpty() && GetPluginFS()->GetCurrentPath(curPath))
                    {
                        PathHistory->ChangeActualPathData(2, GetPluginFS()->GetPluginFSName(), curPath,
                                                          GetPluginFS()->GetInterface(), GetPluginFS(),
                                                          topIndex, file->Name);
                    }
                }
            }
        }
    }
}

void CFilesWindow::RemoveCurrentPathFromHistory()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RemoveCurrentPathFromHistory()");

    if (Is(ptZIPArchive))
    {
        PathHistory->RemoveActualPath(1, GetZIPArchive(), GetZIPPath(), NULL, NULL);
    }
    else
    {
        if (Is(ptDisk))
        {
            PathHistory->RemoveActualPath(0, GetPath(), NULL, NULL, NULL);
        }
        else
        {
            if (Is(ptPluginFS))
            {
                char curPath[MAX_PATH];
                if (GetPluginFS()->NotEmpty() && GetPluginFS()->GetCurrentPath(curPath))
                {
                    PathHistory->RemoveActualPath(2, GetPluginFS()->GetPluginFSName(), curPath,
                                                  GetPluginFS()->GetInterface(), GetPluginFS());
                }
            }
        }
    }
}

void CFilesWindow::InvalidateChangesInPanelWeHaveNewListing()
{
    NeedRefreshAfterEndOfSM = FALSE;
    NeedRefreshAfterIconsReading = FALSE;
    HANDLES(EnterCriticalSection(&TimeCounterSection));
    RefreshAfterEndOfSMTime = MyTimeCounter++;
    RefreshAfterIconsReadingTime = MyTimeCounter++;
    HANDLES(LeaveCriticalSection(&TimeCounterSection));
    PluginFSNeedRefreshAfterEndOfSM = FALSE;
    NeedIconOvrRefreshAfterIconsReading = FALSE;
    if (IconOvrRefreshTimerSet)
    {
        KillTimer(HWindow, IDT_ICONOVRREFRESH);
        IconOvrRefreshTimerSet = FALSE;
    }
    if (InactiveRefreshTimerSet)
    {
        //    TRACE_I("Have new listing, so killing INACTIVEREFRESH timer");
        KillTimer(HWindow, IDT_INACTIVEREFRESH);
        InactiveRefreshTimerSet = FALSE;
    }
}

BOOL CFilesWindow::ChangePathToDisk(HWND parent, const char* path, int suggestedTopIndex,
                                    const char* suggestedFocusName, BOOL* noChange,
                                    BOOL refreshListBox, BOOL canForce, BOOL isRefresh, int* failReason,
                                    BOOL shorterPathWarning, int tryCloseReason)
{
    CALL_STACK_MESSAGE9("CFilesWindow::ChangePathToDisk(, %s, %d, %s, , %d, %d, %d, , %d, %d)", path,
                        suggestedTopIndex, suggestedFocusName, refreshListBox, canForce, isRefresh,
                        shorterPathWarning, tryCloseReason);

    //TRACE_I("change-to-disk: begin");

    if (strlen(path) >= MAX_PATH - 2)
    {
        SalMessageBox(parent, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }

    // udelame zalozni kopie
    char backup[MAX_PATH];
    lstrcpyn(backup, path, MAX_PATH); // nutne udelat pred UpdateDefaultDir (muze byt ukazatel do DefaultDir[])
    char backup2[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup2, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup2;
    }

    // obnovime udaje o stavu panelu (top-index + focused-name) pred pripadnym zavrenim teto cesty
    RefreshPathHistoryData();

    if (noChange != NULL)
        *noChange = TRUE;

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    // pokud jde o rel. cestu, prevedeme ji na absolutni
    int errTextID;
    //  if (!SalGetFullName(backup, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
    //                      MainWindow->GetActivePanel()->GetPath() : NULL))
    if (!SalGetFullName(backup, &errTextID, Is(ptDisk) ? GetPath() : NULL)) // kvuli FTP pluginu - rel. cesta v "target panel path" pri connectu
    {
        SalMessageBox(parent, LoadStr(errTextID), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    path = backup;

    // nahozeni hodin
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // neprejeme si zadne refreshe -> znamenaly by rekurzi

    //  BOOL firstRun = TRUE;    // zakomentovane kvuli zakomentovani forceUpdate
    BOOL fixedDrive = FALSE;
    BOOL canTryUserRescuePath = FALSE; // povoluje pouziti Configuration.IfPathIsInaccessibleGoTo tesne pred fixed-drive cestou
    BOOL openIfPathIsInaccessibleGoToCfg = FALSE;
    char ifPathIsInaccessibleGoTo[MAX_PATH];
    GetIfPathIsInaccessibleGoTo(ifPathIsInaccessibleGoTo);
    if ((ifPathIsInaccessibleGoTo[0] == '\\' && ifPathIsInaccessibleGoTo[1] == '\\' ||
         ifPathIsInaccessibleGoTo[0] != 0 && ifPathIsInaccessibleGoTo[1] == ':') &&
        !IsTheSamePath(path, ifPathIsInaccessibleGoTo))
    {
        canTryUserRescuePath = TRUE;
    }
    BOOL closeCalled = FALSE;
    // pokud jde o zmeny v ramci jednoho disku (vcetne archivu), najdeme platny adresar i za cenu
    // zmeny na "fixed-drive"
    BOOL forceUpdateInt = (Is(ptDisk) || Is(ptZIPArchive)) && HasTheSameRootPath(GetPath(), path);
    BOOL detachFS;
    if (PrepareCloseCurrentPath(parent, canForce, TRUE, detachFS, tryCloseReason))
    { // zmena v ramci "ptDisk" nebo soucasnou cestu pujde zavrit, zkusime otevrit novou cestu
        char changedPath[MAX_PATH];
        strcpy(changedPath, path);
        BOOL tryNet = !CriticalShutdown && ((!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath()));

    _TRY_AGAIN:

        DWORD err, lastErr;
        BOOL pathInvalid, cut;
        SalCheckAndRestorePathWithCut(parent, changedPath, tryNet, err, lastErr, pathInvalid, cut, FALSE);
        if (cut)
        { // zneplatnime navrhovane nastaveni listboxu (budeme listovat jinou cestu)
            suggestedTopIndex = -1;
            suggestedFocusName = NULL;
        }

        if (!pathInvalid && err == ERROR_SUCCESS)
        {
            /*    // zakomentovana optimalizace pripadu, kdy je nova cesta shodna se starou -> u disku to byl nezvyk...
      if (!forceUpdate && firstRun && Is(ptDisk) && IsTheSamePath(changedPath, GetPath()))
      {  // neni duvod menit cestu
        CloseCurrentPath(parent, TRUE, detachFS, FALSE, isRefresh, FALSE);  // "cancel" - na teto ceste zustavame
        EndStopRefresh();
        if (setWait) SetCursor(oldCur);
        if (IsTheSamePath(path, GetPath()))
        {
          return TRUE; // nova cesta se shoduje se soucasnou cestou, neni co delat
        }
        else
        {
          // zkracena cesta se shoduje se soucasnou cestou
          // nastava napr. pri pokusu o vstup do nepristupneho adresare (okamzity navrat zpet)
          CheckPath(TRUE, path, lastErr, TRUE, parent);  // ohlasime chybu, ktera vedla ke zkraceni cesty
          return FALSE;  // pozadovana cesta neni dostupna
        }
      }
      firstRun = FALSE;
*/
            BOOL updateIcon;
            updateIcon = !Is(ptDisk) || // jednoduche kvuli zakomentovani forceUpdate
                         !HasTheSameRootPath(changedPath, GetPath());
            //      updateIcon = forceUpdate || !Is(ptDisk) || !HasTheSameRootPath(changedPath, GetPath());

            if (UseSystemIcons || UseThumbnails)
                SleepIconCacheThread();

            if (!closeCalled)
            { // vleze to sem jen pri prvnim pruchodu, proto je mozne pouzit "Is(ptDisk)" a "GetPath()"
                BOOL samePath = (Is(ptDisk) && IsTheSamePath(GetPath(), changedPath));
                BOOL oldCanAddToDirHistory;
                if (samePath)
                {
                    // nebudeme si pamatovat zaviranou cestu (aby cesta nepribyla jen kvuli change-dir na stejnou cestu)
                    oldCanAddToDirHistory = MainWindow->CanAddToDirHistory;
                    MainWindow->CanAddToDirHistory = FALSE;
                }

                CloseCurrentPath(parent, FALSE, detachFS, samePath, isRefresh, !samePath); // uspech, prejdeme na novou cestu

                if (samePath)
                {
                    // prechod zpet do puvodniho rezimu pamatovani cest
                    MainWindow->CanAddToDirHistory = oldCanAddToDirHistory;
                }

                // schovame throbbera a ikonu zabezpeceni, na disku o ne nestojime
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                closeCalled = TRUE;
            }
            //--- nastavime panel na "disk" cestu
            SetPanelType(ptDisk);
            SetPath(changedPath);
            if (updateIcon ||
                !GetNetworkDrive()) // aby zobrazovalo korektne ikony pri prechodu na mounted-volume (na lokale nezpomaluje, takze snad zadny problem)
            {
                UpdateDriveIcon(FALSE);
            }
            if (noChange != NULL)
                *noChange = FALSE; // listing se uvolnil
            forceUpdateInt = TRUE; // tak a ted uz se neco musi povest nacist (jinak: prazdny panel)

            // nechame nacist obsah nove cesty
            BOOL cannotList;
            cannotList = !CommonRefresh(parent, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);
            if (isRefresh && !cannotList && GetMonitorChanges() && !AutomaticRefresh)
            {                                                                                                                // chyba auto-refreshe, overime jestli to neni kvuli vymazu adresare zobrazeneho v panelu (delalo mi pri mazani po siti z jine masiny) ... neprijemne je, ze pokud tuhle chybu ignorujeme, obsah panelu uz se proste neobnovi (protoze auto-refresh nefunguje)
                Sleep(400);                                                                                                  // dame si chvilku pauzu, aby mohlo pokrocit mazani adresare (aby uz byl dost smazanej na to, aby nesel listovat, dobrej humus...)
                                                                                                                             //        TRACE_I("Calling CommonRefresh again... (unable to receive change notifications, first listing was OK, but maybe current directory is being deleted)");
                cannotList = !CommonRefresh(parent, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh); // zopakneme si listovani, tohle uz by melo selhat
            }
            if (cannotList)
            { // zvolena cesta nejde vylistovat ("access denied" nebo low_memory) nebo jiz byla smazana

            FIXED_DRIVE:

                BOOL change = FALSE;
                if (fixedDrive || !CutDirectory(changedPath)) // pokusime se cestu zkratit
                {
                    if (canTryUserRescuePath) // nejdrive zkusime "unikovou cestu", kterou si preje user
                    {
                        canTryUserRescuePath = FALSE; // nebudeme ji zkouset vickrat
                        openIfPathIsInaccessibleGoToCfg = TRUE;
                        fixedDrive = FALSE; // umoznime zmenu na fixed-drive (mozna uz se zkouselo, ale prednost dostala cesta usera)
                        GetIfPathIsInaccessibleGoTo(changedPath);
                        shorterPathWarning = TRUE; // chceme videt chyby "rescue" cesty
                        change = TRUE;
                    }
                    else
                    {
                        if (openIfPathIsInaccessibleGoToCfg)
                            OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;

                        // nelze zkratit, najdeme systemovy nebo prvni fixed-drive (nas "unikovy disk")
                        char sysDir[MAX_PATH];
                        char root[4] = " :\\";
                        BOOL done = FALSE;
                        if (GetWindowsDirectory(sysDir, MAX_PATH) != 0 && sysDir[0] != 0 && sysDir[1] == ':')
                        {
                            root[0] = sysDir[0];
                            if (GetDriveType(root) == DRIVE_FIXED)
                                done = TRUE;
                        }
                        if (!done)
                        {
                            DWORD disks = GetLogicalDrives();
                            disks >>= 2; // preskocime A: a B:, pri formatovani disket se nekde stavaji DRIVE_FIXED
                            char d = 'C';
                            while (d <= 'Z')
                            {
                                if (disks & 1)
                                {
                                    root[0] = d;
                                    if (GetDriveType(root) == DRIVE_FIXED)
                                        break; // mame nas "unikovy disk"
                                }
                                disks >>= 1;
                                d++;
                            }
                            if (d <= 'Z')
                                done = TRUE; // nas "unikovy disk" byl nalezen
                        }
                        if (done)
                        {
                            if (LowerCase[root[0]] != LowerCase[changedPath[0]]) // opatreni proti nekonecnemu cyklu
                            {                                                    // UNC nebo jiny disk (typu "c:\")
                                strcpy(changedPath, root);                       // zkusime nas "unikovy disk"
                                change = TRUE;
                            }
                        }
                    }
                }
                else
                    change = TRUE; // zkracena cesta

                if (change) // jen pri nove ceste, jinak nechame panel prazdny (snad nehrozi, fixed-drive to jisti)
                {
                    // zneplatnime navrhovane nastaveni listboxu (budeme listovat jinou cestu)
                    suggestedTopIndex = -1;
                    suggestedFocusName = NULL;
                    // dochazi ke zmene "nove" cesty (v CloseCurrentPath mohlo prezit UserWorkedOnThisPath == TRUE)
                    UserWorkedOnThisPath = FALSE;
                    // zkusime vylistovat upravenou cestu
                    goto _TRY_AGAIN;
                }
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
            }
            else
            {
                // prave jsme obdrzeli novy listing, pokud jsou hlaseny nejake zmeny v panelu, stornujeme je
                InvalidateChangesInPanelWeHaveNewListing();

                if (lastErr != ERROR_SUCCESS && (!isRefresh || openIfPathIsInaccessibleGoToCfg) && shorterPathWarning)
                {                        // pokud nejde o refresh a maji se vypisovat hlaseni o zkraceni cesty ...
                    if (!refreshListBox) // budeme vypisovat hlaseni, musime provest refresh-list-box
                    {
                        RefreshListBox(0, -1, -1, FALSE, FALSE);
                    }
                    // ohlasime chybu, ktera vedla ke zkraceni cesty
                    char errBuf[2 * MAX_PATH + 100];
                    sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT),
                            openIfPathIsInaccessibleGoToCfg ? ifPathIsInaccessibleGoTo : path,
                            GetErrorText(lastErr));
                    SalMessageBox(parent, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                    if (openIfPathIsInaccessibleGoToCfg)
                        OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;
                }
                if (failReason != NULL)
                    *failReason = CHPPFR_SUCCESS;
            }
            //---  obnova DefaultDir
            MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);
        }
        else
        {
            if (err == ERROR_NOT_READY) // jde-li o nepripravenou mechaniku (removable medium)
            {
                char text[100 + MAX_PATH];
                char drive[MAX_PATH];
                UINT drvType;
                if (changedPath[0] == '\\' && changedPath[1] == '\\')
                {
                    drvType = DRIVE_REMOTE;
                    GetRootPath(drive, changedPath);
                    drive[strlen(drive) - 1] = 0; // nestojime o posledni '\\'
                }
                else
                {
                    drive[0] = changedPath[0];
                    drive[1] = 0;
                    drvType = MyGetDriveType(changedPath);
                }
                if (drvType != DRIVE_REMOTE)
                {
                    GetCurrentLocalReparsePoint(changedPath, CheckPathRootWithRetryMsgBox);
                    if (strlen(CheckPathRootWithRetryMsgBox) > 3)
                    {
                        lstrcpyn(drive, CheckPathRootWithRetryMsgBox, MAX_PATH);
                        SalPathRemoveBackslash(drive);
                    }
                }
                else
                    GetRootPath(CheckPathRootWithRetryMsgBox, changedPath);
                sprintf(text, LoadStr(IDS_NODISKINDRIVE), drive);
                int msgboxRes = (int)CDriveSelectErrDlg(parent, text, changedPath).Execute();
                if (msgboxRes == IDCANCEL && CutDirectory(CheckPathRootWithRetryMsgBox))
                { // aby se dalo dostat do rootu cesty pri namountenem volume (F:\DRIVE_CD -> F:\)
                    lstrcpyn(changedPath, CheckPathRootWithRetryMsgBox, MAX_PATH);
                    msgboxRes = IDRETRY;
                }
                CheckPathRootWithRetryMsgBox[0] = 0;
                UpdateWindow(MainWindow->HWindow);
                if (msgboxRes == IDRETRY)
                    goto _TRY_AGAIN;
            }
            else
            {
                if (!pathInvalid &&               // o neuspechu pri ozivovani UNC cesty uz user vi
                    err != ERROR_USER_TERMINATED) // o preruseni (ESC) uz take user vi
                {
                    CheckPath(TRUE, changedPath, err, TRUE, parent); // ostatni chyby - pouze vypis chyby
                }
            }

            if (forceUpdateInt && !fixedDrive) // je-li nutny update, zkusime jeste fixed-drive
            {
                fixedDrive = TRUE; // obrana proti cykleni + zmena na fixed-drive
                goto FIXED_DRIVE;
            }
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
        }

        if (!closeCalled)
            CloseCurrentPath(parent, TRUE, detachFS, FALSE, isRefresh, FALSE); // neuspech, zustaneme na puvodni ceste
    }
    else
    {
        if (failReason != NULL)
            *failReason = CHPPFR_CANNOTCLOSEPATH;
    }

    EndStopRefresh();
    if (setWait)
        SetCursor(oldCur);
    BOOL ret = Is(ptDisk) && IsTheSamePath(GetPath(), path);
    if (!ret && failReason != NULL && *failReason == CHPPFR_SUCCESS)
    {
        *failReason = CHPPFR_SHORTERPATH;
    }
    //TRACE_I("change-to-disk: end");
    return ret;
}

BOOL CFilesWindow::ChangePathToArchive(const char* archive, const char* archivePath,
                                       int suggestedTopIndex, const char* suggestedFocusName,
                                       BOOL forceUpdate, BOOL* noChange, BOOL refreshListBox,
                                       int* failReason, BOOL isRefresh, BOOL canFocusFileName,
                                       BOOL isHistory)
{
    CALL_STACK_MESSAGE10("CFilesWindow::ChangePathToArchive(%s, %s, %d, %s, %d, , %d, , %d, %d, %d)",
                         archive, archivePath, suggestedTopIndex, suggestedFocusName,
                         forceUpdate, refreshListBox, isRefresh, canFocusFileName, isHistory);

    // udelame zalozni kopie
    char backup1[MAX_PATH];
    lstrcpyn(backup1, archive, MAX_PATH);
    char backup2[MAX_PATH];
    lstrcpyn(backup2, archivePath, MAX_PATH);
    archivePath = backup2;
    char backup3[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup3, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup3;
    }

    // obnovime udaje o stavu panelu (top-index + focused-name) pred pripadnym zavrenim teto cesty
    RefreshPathHistoryData();

    if (noChange != NULL)
        *noChange = TRUE;

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    // pokud je archive rel. cesta, prevedeme ji na absolutni
    int errTextID;
    //  if (!SalGetFullName(backup1, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
    //                      MainWindow->GetActivePanel()->GetPath() : NULL))
    if (!SalGetFullName(backup1, &errTextID, Is(ptDisk) ? GetPath() : NULL)) // konzistence s ChangePathToDisk()
    {
        SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    archive = backup1;

    //---  nahozeni hodin
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // neprejeme si zadne refreshe

    BOOL nullFile;         // TRUE pokud je archive nulovy soubor (size==0)
    FILETIME archiveDate;  // datum&cas souboru archivu
    CQuadWord archiveSize; // velikost souboru archivu

    char text[MAX_PATH + 500];
    char path[MAX_PATH];
    BOOL sameArch;
    BOOL checkPath = TRUE;
    BOOL forceUpdateInt = FALSE; // zmena cesty nutna? (prip. i na disk)
    BOOL tryPathWithArchiveOnError = isHistory;
    if (!Is(ptZIPArchive) || StrICmp(GetZIPArchive(), archive) != 0) // neni archiv nebo jiny archiv
    {

    _REOPEN_ARCHIVE:

        sameArch = FALSE;
        BOOL detachFS;
        if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
        { // soucasnou cestu pujde zavrit, zkusime otevrit novou cestu
            // test pristupnosti cesty, na ktere archiv lezi
            strcpy(path, archive);
            if (!CutDirectory(path, NULL))
            {
                TRACE_E("Unexpected situation in CFilesWindow::ChangePathToArchive.");
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                tryPathWithArchiveOnError = FALSE; // nesmyslna chyba, neresime

            ERROR_1:

                CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, isRefresh, FALSE); // neuspech, zustaneme na puvodni ceste

                if (forceUpdateInt) // nutna zmena cesty; do archivu se to nepovedlo, jdeme na disk
                {                   // jsme jiste v archivu (jde o "refresh" archivu v panelu)
                    // pokud to pujde, vystoupime z archivu (pripadne az na "fixed-drive")
                    ChangePathToDisk(HWindow, GetPath(), -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }
                else
                {
                    if (tryPathWithArchiveOnError) // zkusime zmenu cesty na cestu co nejblize k archivu
                        ChangePathToDisk(HWindow, path, -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }

                EndStopRefresh();
                if (setWait)
                    SetCursor(oldCur);

                return FALSE;
            }

            // sitove cesty nebudeme testovat, pokud jsme na ne zrovna pristupovali
            BOOL tryNet = (!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath());
            DWORD err, lastErr;
            BOOL pathInvalid, cut;
            if (!SalCheckAndRestorePathWithCut(HWindow, path, tryNet, err, lastErr, pathInvalid, cut, FALSE) ||
                cut)
            { // cesta neni pristupna nebo je oriznuta (archiv neni mozne otevrit)
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                if (tryPathWithArchiveOnError)
                    tryPathWithArchiveOnError = (err == ERROR_SUCCESS && !pathInvalid); // kratsi cesta je pristupna, zkusime ji
                if (!isRefresh)                                                         // pri refreshi se hlasky o zkracovani cesty nevypisuji
                {
                    sprintf(text, LoadStr(IDS_FILEERRORFORMAT), archive, GetErrorText(lastErr));
                    SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
                goto ERROR_1;
            }

            if (PackerFormatConfig.PackIsArchive(archive)) // je to archiv?
            {
                // zjistime informace o souboru (existuje?, size, date&time)
                DWORD err2 = NO_ERROR;
                HANDLE file = HANDLES_Q(CreateFile(archive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
                if (file != INVALID_HANDLE_VALUE)
                {
                    GetFileTime(file, NULL, NULL, &archiveDate);
                    SalGetFileSize(file, archiveSize, err2); // vraci "uspech?" - ignorujeme, testujeme pozdeji 'err2'
                    nullFile = archiveSize == CQuadWord(0, 0);
                    HANDLES(CloseHandle(file));
                }
                else
                    err2 = GetLastError();

                if (err2 != NO_ERROR)
                {
                    if (!isRefresh) // pri refreshi se hlasky o neexistenci cesty nevypisuji
                        DialogError(HWindow, BUTTONS_OK, archive, GetErrorText(err2), LoadStr(IDS_ERROROPENINGFILE));
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDPATH;
                    goto ERROR_1; // chyba
                }

                CSalamanderDirectory* newArchiveDir = new CSalamanderDirectory(FALSE);

                // uplatnime optimalizovane pridavani do 'newArchiveDir'
                newArchiveDir->AllocAddCache();

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                CPluginDataInterfaceAbstract* pluginData = NULL;
                CPluginData* plugin = NULL;
                if (!nullFile)
                    CreateSafeWaitWindow(LoadStr(IDS_LISTINGARCHIVE), NULL, 2000, FALSE, MainWindow->HWindow);
                if (nullFile || PackList(this, archive, *newArchiveDir, pluginData, plugin))
                {
                    // uvolnime cache, at v objektu zbytecne nestrasi
                    newArchiveDir->FreeAddCache();

                    if (!nullFile)
                        DestroySafeWaitWindow();
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    BOOL isTheSamePath = FALSE; // TRUE = nedochazi ke zmene cesty
                    if (Is(ptZIPArchive) && StrICmp(GetZIPArchive(), archive) == 0)
                    {
                        char buf[MAX_PATH];
                        strcpy(buf, *archivePath == '\\' ? archivePath + 1 : archivePath);
                        char* end = buf + strlen(buf);
                        if (end > buf && *(end - 1) == '\\')
                            *--end = 0;

                        if (GetArchiveDir()->SalDirStrCmp(buf, GetZIPPath()) == 0)
                            isTheSamePath = TRUE;
                    }

                    // uspech, prejdeme na novou cestu - z duvodu mozne casove narocnosti listovani
                    // archivu dojde ke zmene cesty i pokud cilova cesta neexistuje - tyka se prikazu
                    // Change Directory (Shift+F7), ktery jinak v teto situaci cestu nemeni
                    CloseCurrentPath(HWindow, FALSE, detachFS, isTheSamePath, isRefresh, !isTheSamePath);

                    // prave jsme obdrzeli novy listing, pokud jsou hlaseny nejake zmeny v panelu, stornujeme je
                    InvalidateChangesInPanelWeHaveNewListing();

                    // schovame throbbera a ikonu zabezpeceni, v archivech o ne nestojime
                    if (DirectoryLine != NULL)
                        DirectoryLine->HideThrobberAndSecurityIcon();

                    SetPanelType(ptZIPArchive);
                    SetPath(path);
                    UpdateDriveIcon(FALSE);
                    SetArchiveDir(newArchiveDir);
                    SetPluginIface(plugin != NULL ? plugin->GetPluginInterface()->GetInterface() : NULL);
                    SetZIPArchive(archive);
                    SetZIPArchiveDate(archiveDate);
                    SetZIPArchiveSize(archiveSize);
                    if (plugin != NULL)
                    {
                        PluginData.Init(pluginData, plugin->DLLName, plugin->Version,
                                        plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
                    }
                    else
                        PluginData.Init(NULL, NULL, NULL, NULL, 0); // pouzivaji jen plug-iny, Salamander ne
                    SetValidFileData(nullFile ? VALID_DATA_ALL_FS_ARC : GetArchiveDir()->GetValidData());
                    checkPath = FALSE;
                    if (noChange != NULL)
                        *noChange = FALSE;
                    // ZIPPath, Files a Dirs se nastavi pozdeji, az se nastavi archivePath ...
                    if (failReason != NULL)
                        *failReason = CHPPFR_SUCCESS;
                }
                else
                {
                    DestroySafeWaitWindow(); // nullFile musi byt FALSE, proto chybi test...
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                    TRACE_I("Unable to open file " << archive << ".");
                    delete newArchiveDir;
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDARCHIVE;
                    goto ERROR_1;
                }
            }
            else
            {
                TRACE_I("File " << archive << " is no longer archive file.");
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDARCHIVE;
                goto ERROR_1;
            }
        }
        else // soucasna cesta nejde zavrit
        {
            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);
            if (failReason != NULL)
                *failReason = CHPPFR_CANNOTCLOSEPATH;
            return FALSE;
        }
    }
    else // jiz otevreny archiv
    {
        if (forceUpdate) // ma se zkontrolovat jestli se archiv nezmenil?
        {
            DWORD err;
            if ((err = CheckPath(!isRefresh)) == ERROR_SUCCESS) // zde neni treba obnovovat sitova spojeni ...
            {
                HANDLE file = HANDLES_Q(CreateFile(archive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   NULL, OPEN_EXISTING, 0, NULL));
                if (file != INVALID_HANDLE_VALUE)
                {
                    SalGetFileSize(file, archiveSize, err);
                    nullFile = archiveSize == CQuadWord(0, 0);
                    FILETIME zipArchiveDate = GetZIPArchiveDate();
                    BOOL change = (err != NO_ERROR ||                                     // nejde ziskat velikost
                                   !GetFileTime(file, NULL, NULL, &archiveDate) ||        // nejde ziskat datum&cas
                                   CompareFileTime(&archiveDate, &zipArchiveDate) != 0 || // nebo se lisi datum&cas
                                   !IsSameZIPArchiveSize(archiveSize));                   // nebo se lisi velikost souboru
                    HANDLES(CloseHandle(file));

                    if (change) // soubor se zmenil
                    {
                        if (AssocUsed) // mame neco z archivu rozeditovaneho?
                        {
                            // oznamime, ze doslo ke zmenam, ze by si mel zavrit editory
                            char buf[MAX_PATH + 200];
                            sprintf(buf, LoadStr(IDS_ARCHIVEREFRESHEDIT), GetZIPArchive());
                            SalMessageBox(HWindow, buf, LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                        }
                        forceUpdateInt = TRUE; // neni kam se vracet, zmena cesty nutna (prip. i na disk)
                        goto _REOPEN_ARCHIVE;
                    }
                }
                else
                {
                    err = GetLastError(); // soubor archivu nelze otevrit
                    if (!isRefresh)       // pri refreshi se hlasky o neexistenci cesty nevypisuji
                    {
                        sprintf(text, LoadStr(IDS_FILEERRORFORMAT), archive, GetErrorText(err));
                        SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                }
            }
            if (err != ERROR_SUCCESS) // prejdeme na existujici cestu
            {
                if (err != ERROR_USER_TERMINATED)
                {
                    // pokud to pujde, vystoupime z archivu (pripadne az na "fixed-drive")
                    ChangePathToDisk(HWindow, GetPath(), -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }
                else // user dal ESC -> cesta je nejspis nepristupna, jdeme rovnou na "fixed-drive"
                {
                    ChangeToRescuePathOrFixedDrive(HWindow, noChange, refreshListBox);
                }

                EndStopRefresh();
                if (setWait)
                    SetCursor(oldCur);
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                return FALSE;
            }
        }
        if (failReason != NULL)
            *failReason = CHPPFR_SUCCESS;
        sameArch = TRUE;
    }

    // najdeme cestu v archivu, ktera jeste existuje (puvodni nebo zkracena)
    strcpy(path, *archivePath == '\\' ? archivePath + 1 : archivePath);
    char* end = path + strlen(path);
    if (end > path && *(end - 1) == '\\')
        *--end = 0;

    if (sameArch && GetArchiveDir()->SalDirStrCmp(path, GetZIPPath()) == 0) // nova cesta se shoduje se soucasnou cestou
    {
        // aby se nevyignorovali 'suggestedTopIndex' a 'suggestedFocusName', zavolame CommonRefresh
        CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, FALSE, isRefresh);

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);
        return TRUE; // neni co delat
    }

    // ulozime soucasnou cestu v archivu
    char currentPath[MAX_PATH];
    strcpy(currentPath, GetZIPPath());

    SetZIPPath(path);
    BOOL ok = TRUE;
    char* fileName = NULL;
    BOOL useFileName = FALSE;
    while (path[0] != 0 && GetArchiveDirFiles() == NULL)
    {
        end = strrchr(path, '\\');
        useFileName = (canFocusFileName && suggestedFocusName == NULL && fileName == NULL); // fokus souboru je mozny + neni focus zvenci + jen prvni zkraceni
        if (end != NULL)
        {
            *end = 0;
            fileName = end + 1;
        }
        else
        {
            memmove(path + 1, path, strlen(path) + 1);
            fileName = path + 1;
            path[0] = 0;
        }
        SetZIPPath(path);
        ok = FALSE;

        if (!sameArch)
        {
            // dochazi ke zmene "nove" cesty (v CloseCurrentPath mohlo prezit UserWorkedOnThisPath == TRUE)
            UserWorkedOnThisPath = FALSE;
        }
    }

    if (!useFileName && sameArch && GetArchiveDir()->SalDirStrCmp(currentPath, GetZIPPath()) == 0) // nefokusime soubor a zkracena cesta se shoduje se soucasnou cestou
    {                                                                                              // nastava napr. pri pokusu o vstup do nepristupneho adresare (okamzity navrat zpet)
        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);
        if (failReason != NULL)
            *failReason = CHPPFR_SHORTERPATH;
        return FALSE; // pozadovana cesta neni dostupna
    }

    if (!ok)
    {
        // zneplatnime navrhovane nastaveni listboxu (budeme listovat jinou cestu)
        suggestedTopIndex = -1;
        suggestedFocusName = NULL;
        if (failReason != NULL)
            *failReason = useFileName ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH;
    }

    // musi se povest (alespon root archivu vzdy existuje)
    CommonRefresh(HWindow, suggestedTopIndex, useFileName ? fileName : suggestedFocusName,
                  refreshListBox, TRUE, isRefresh);

    if (refreshListBox && !ok && useFileName && GetCaretIndex() == 0)
    { // pokus o vyber jmena souboru selhal -> nebylo to jmeno souboru
        if (failReason != NULL)
            *failReason = CHPPFR_SHORTERPATH;
    }

    EndStopRefresh();
    if (setWait)
        SetCursor(oldCur);

    // pridame cestu, kterou jsme prave opustili (uvnitr archivu se cesty nezaviraji,
    // takze se DirHistoryAddPathUnique jeste nevolalo) + jen kdyz nejde o aktualni
    // cestu (nastava jen kdyz fokusime soubor)
    if (sameArch && GetArchiveDir()->SalDirStrCmp(currentPath, GetZIPPath()) != 0)
    {
        if (UserWorkedOnThisPath)
        {
            MainWindow->DirHistoryAddPathUnique(1, GetZIPArchive(), currentPath, NULL, NULL, NULL);
            UserWorkedOnThisPath = FALSE;
        }

        // opoustime cestu
        HiddenNames.Clear();  // uvolnime skryte nazvy
        OldSelection.Clear(); // a starou selection
    }

    return ok;
}

BOOL CFilesWindow::ChangeAndListPathOnFS(const char* fsName, int fsNameIndex, const char* fsUserPart,
                                         CPluginFSInterfaceEncapsulation& pluginFS, CSalamanderDirectory* dir,
                                         CPluginDataInterfaceAbstract*& pluginData, BOOL& shorterPath,
                                         int& pluginIconsType, int mode, BOOL firstCall,
                                         BOOL* cancel, const char* currentPath, int currentPathFSNameIndex,
                                         BOOL forceUpdate, char* cutFileName, BOOL* keepOldListing)
{
    CALL_STACK_MESSAGE10("CFilesWindow::ChangeAndListPathOnFS(%s, %d, %s, , , , , , %d, %d, , %s, %d, %d, , %d)",
                         fsName, fsNameIndex, fsUserPart, mode, firstCall, currentPath, currentPathFSNameIndex,
                         forceUpdate, (keepOldListing != NULL && *keepOldListing));
    if (cutFileName != NULL)
        *cutFileName = 0;
    char bufFSUserPart[MAX_PATH];
    const char* origUserPart; // user-part, na ktery mame zmenit cestu
    int origFSNameIndex;
    if (fsUserPart == NULL) // jde o odpojeny FS, obnova listingu...
    {
        if (!pluginFS.GetCurrentPath(bufFSUserPart))
        {
            TRACE_E("Unable to get current path from detached FS.");
            return FALSE;
        }
        origUserPart = bufFSUserPart;
        origFSNameIndex = pluginFS.GetPluginFSNameIndex();
    }
    else
    {
        origUserPart = fsUserPart;
        origFSNameIndex = fsNameIndex;
    }

    CSalamanderDirectory* workDir = dir;
    if (keepOldListing != NULL && *keepOldListing)
    {
        workDir = new CSalamanderDirectory(TRUE);
        if (workDir == NULL) // nedostatek pameti -> uvolnime listing (bude prazdny panel)
        {
            *keepOldListing = FALSE;
            workDir = dir;

            if (!firstCall)
            {
                // uvolneni dat listingu v panelu
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                ReleaseListing();                 // pocitame s tim, ze 'dir' je PluginFSDir
                workDir = dir = GetPluginFSDir(); // v ReleaseListing() se muze i jen odpojovat (viz OnlyDetachFSListing)

                // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (prave jsme podrizli data)
                ListBox->SetItemsCount(0, 0, 0, TRUE);
                SelectedCount = 0;
                // Pokud se doruci WM_USER_UPDATEPANEL, dojde k prekreslnei obsahu panelu a nastaveni
                // scrollbary. Dorucit ji muze message loopa pri vytvoreni message boxu.
                // Jinak se panel bude tvarit jako nezmeneny a message bude vyjmuta z fronty.
                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
            }
        }
    }
    else
    {
        if (!firstCall)
        {
            workDir->Clear(NULL); // zbytecne (melo by byt prazdne), jen tak pro jistotu
        }
    }

    BOOL ok = FALSE;
    char user[MAX_PATH];
    lstrcpyn(user, origUserPart, MAX_PATH);
    pluginData = NULL;
    shorterPath = FALSE;
    if (cancel != NULL)
        *cancel = FALSE; // nova data
    // budeme se pokouset nacist obsah "adresare" (cesta se muze postupne zkracovat)
    BOOL useCutFileName = TRUE;
    char fsNameBuf[MAX_PATH];
    fsNameBuf[0] = 0;
    while (1)
    {
        if (cutFileName != NULL && *cutFileName != 0)
            useCutFileName = FALSE;
        BOOL pathWasCut = FALSE;

        char newFSName[MAX_PATH];
        lstrcpyn(newFSName, fsName, MAX_PATH);
        BOOL changePathRet = pluginFS.ChangePath(pluginFS.GetPluginFSNameIndex(), newFSName,
                                                 fsNameIndex, user, cutFileName,
                                                 cutFileName != NULL ? &pathWasCut : NULL,
                                                 forceUpdate, mode);
        if (changePathRet) // ChangePath nevraci error
        {
            if (StrICmp(newFSName, fsName) != 0) // zmena fs-name, musime nove fs-name overit
            {
                BOOL ok2 = FALSE;
                int index;
                int newFSNameIndex;
                if (Plugins.IsPluginFS(newFSName, index, newFSNameIndex))
                {
                    CPluginData* plugin = Plugins.Get(index);
                    if (plugin != NULL)
                    {
                        if (plugin->GetPluginInterface()->GetInterface() == pluginFS.GetPluginInterface())
                            ok2 = TRUE;
                        else
                            TRACE_E("CFilesWindow::ChangeAndListPathOnFS(): pluginFS.ChangePath() returned fs-name "
                                    "("
                                    << newFSName << ") from other plugin: " << plugin->DLLName);
                    }
                    else
                        TRACE_E("Second unexpected situation in CFilesWindow::ChangeAndListPathOnFS()");
                }
                else
                    TRACE_E("CFilesWindow::ChangeAndListPathOnFS(): pluginFS.ChangePath() returned unknown fs-name: " << newFSName);
                if (!ok2)
                    changePathRet = FALSE; // zmena fs-name se nepovedla, simulujeme fatal error na FS
                else                       // zacneme pouzivat nove jmeno FS (pro dalsi pruchod cyklem)
                {
                    lstrcpyn(fsNameBuf, newFSName, MAX_PATH);
                    fsName = fsNameBuf;
                    fsNameIndex = newFSNameIndex;
                }
            }
            if (changePathRet) // pouzite fs-name ulozime do 'pluginFS'
                pluginFS.SetPluginFS(fsName, fsNameIndex);
        }

        if (changePathRet)
        { // cesta vypada o.k.
            if (pathWasCut && cutFileName != NULL && *cutFileName == 0)
                useCutFileName = FALSE;
            if (firstCall) // zmena cesty v ramci FS, listing puvodni cesty neni uvolneny (stacil by?)
            {
                if (!forceUpdate && currentPath != NULL &&
                    pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(),
                                           currentPathFSNameIndex, currentPath)) // cesta zkracena na puvodni cestu
                {                                                                // neni duvod ke zmene cesty, puvodni listing staci
                    shorterPath = !pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(), origFSNameIndex, origUserPart);
                    if (cancel != NULL)
                        *cancel = TRUE;                     // zachovali jsme puvodni data
                    pluginIconsType = GetPluginIconsType(); // zbytecne (nebude se pouzivat), ale zustava stejny
                    ok = TRUE;
                    break;
                }

                if (keepOldListing == NULL || !*keepOldListing) // neni dead-code (pouzije se pri chybe alokace workDir)
                {
                    // uvolneni dat listingu v panelu
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    ReleaseListing();                 // pocitame s tim, ze 'dir' je PluginFSDir
                    workDir = dir = GetPluginFSDir(); // v ReleaseListing() se muze i jen odpojovat (viz OnlyDetachFSListing)

                    // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (prave jsme podrizli data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                    // Pokud se doruci WM_USER_UPDATEPANEL, dojde k prekreslnei obsahu panelu a nastaveni
                    // scrollbary. Dorucit ji muze message loopa pri vytvoreni message boxu.
                    // Jinak se panel bude tvarit jako nezmeneny a message bude vyjmuta z fronty.
                    PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
                }
                firstCall = FALSE;
            }

            // pokusime se vylistovat soubory a adresare z akt. cesty
            if (pluginFS.ListCurrentPath(workDir, pluginData, pluginIconsType, forceUpdate)) // podarilo se ...
            {
                if (keepOldListing != NULL && *keepOldListing) // uz mame novy listing, stary zrusime
                {
                    // uvolneni dat listingu v panelu
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    ReleaseListing();       // pocitame s tim, ze 'dir' je PluginFSDir
                    dir = GetPluginFSDir(); // v ReleaseListing() se muze i jen odpojovat (viz OnlyDetachFSListing)

                    // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (prave jsme podrizli data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                    // Pokud se doruci WM_USER_UPDATEPANEL, dojde k prekreslnei obsahu panelu a nastaveni
                    // scrollbary. Dorucit ji muze message loopa pri vytvoreni message boxu.
                    // Jinak se panel bude tvarit jako nezmeneny a message bude vyjmuta z fronty.
                    PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);

                    SetPluginFSDir(workDir); // nastavime novy listing
                    delete dir;
                    dir = workDir;
                }

                if (pluginIconsType != pitSimple &&
                    pluginIconsType != pitFromRegistry &&
                    pluginIconsType != pitFromPlugin) // overime jestli se trefil do povolene hodnoty
                {
                    TRACE_E("Invalid plugin-icons-type!");
                    pluginIconsType = pitSimple;
                }
                if (pluginIconsType == pitFromPlugin && pluginData == NULL) // to by neslo, degradujeme typ
                {
                    TRACE_E("Plugin-icons-type is pitFromPlugin and plugin-data is NULL!");
                    pluginIconsType = pitSimple;
                }
                shorterPath = !pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(), origFSNameIndex, origUserPart);
                ok = TRUE;
                break;
            }
            // pripravime dir na dalsi pouziti (pokud po sobe plug-in neco nechal, uvolnime to)
            workDir->Clear(NULL);
            // cesta neni o.k., pokusime se ji zkratit v pristim pruchodu cyklu
            if (!pluginFS.GetCurrentPath(user))
            {
                TRACE_E("Unexpected situation in CFilesWindow::ChangeAndListPathOnFS()");
                break;
            }
        }
        else // fatal error, koncime
        {
            TRACE_I("Unable to open FS path " << fsName << ":" << origUserPart);

            if (firstCall && (keepOldListing == NULL || !*keepOldListing)) // neni dead-code (pouzije se pri chybe alokace workDir)
            {
                // uvolneni dat listingu v panelu
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                ReleaseListing();                 // pocitame s tim, ze 'dir' je PluginFSDir
                workDir = dir = GetPluginFSDir(); // v ReleaseListing() se muze i jen odpojovat (viz OnlyDetachFSListing)

                // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (prave jsme podrizli data)
                ListBox->SetItemsCount(0, 0, 0, TRUE);
                SelectedCount = 0;
                // Pokud se doruci WM_USER_UPDATEPANEL, dojde k prekreslnei obsahu panelu a nastaveni
                // scrollbary. Dorucit ji muze message loopa pri vytvoreni message boxu.
                // Jinak se panel bude tvarit jako nezmeneny a message bude vyjmuta z fronty.
                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
            }
            useCutFileName = FALSE;
            break;
        }
    }

    if (dir != workDir)
        delete workDir; // 'workDir' se nepouzil, uvolnime ho

    // zkusime najit soubor k fokuseni v listingu z FS - neni dokonale, pokud je soubor skryty
    // z listingu v panelu (napr. pres "nezobrazovat skryte soubory" nebo pres filtry), pak nemuze
    // byt v panelu vyfokusen a user se o teto "chybe" nedozvi - nicmene asi to ani chybu hlasit
    // nemuze, kdyz ten soubor ve skutecnosti existuje, takze na to kasleme (stejne jako u
    // diskovych cest)...
    if (ok && useCutFileName && cutFileName != NULL && *cutFileName != 0)
    {
        CFilesArray* files = dir->GetFiles("");
        unsigned cutFileNameLen = (int)strlen(cutFileName);
        int count = files->Count;
        int i;
        for (i = 0; i < count; i++)
        {
            CFileData* f = &(files->At(i));
            if (cutFileNameLen == f->NameLen &&
                StrICmpEx(f->Name, cutFileNameLen, cutFileName, cutFileNameLen) == 0)
                break;
        }
        if (i == count) // vypiseme chybu (soubor pro fokuseni nebyl nalezen)
        {
            char errText[MAX_PATH + 200];
            sprintf(errText, LoadStr(IDS_UNABLETOFOCUSFILEONFS), cutFileName);
            SalMessageBox(HWindow, errText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            *cutFileName = 0;
        }
    }

    if (!useCutFileName && cutFileName != NULL)
        *cutFileName = 0; // nechceme pouzivat -> vynulujeme
    return ok;
}

BOOL CFilesWindow::ChangePathToPluginFS(const char* fsName, const char* fsUserPart, int suggestedTopIndex,
                                        const char* suggestedFocusName, BOOL forceUpdate, int mode,
                                        BOOL* noChange, BOOL refreshListBox, int* failReason, BOOL isRefresh,
                                        BOOL canFocusFileName, BOOL convertPathToInternal)
{
    CALL_STACK_MESSAGE11("CFilesWindow::ChangePathToPluginFS(%s, %s, %d, %s, %d, %d, , %d, , %d, %d, %d)",
                         fsName, fsUserPart, suggestedTopIndex, suggestedFocusName, forceUpdate,
                         mode, refreshListBox, isRefresh, canFocusFileName, convertPathToInternal);
    //TRACE_I("change-to-fs: begin");

    // pro pripad, ze by fsName ukazovalo do meneneho retezce (GetPluginFS()->PluginFSName()), udelame zalozni kopii
    char backup[MAX_PATH];
    lstrcpyn(backup, fsName, MAX_PATH);
    fsName = backup;

    if (noChange != NULL)
        *noChange = TRUE;
    if (mode != 3 && canFocusFileName)
    {
        TRACE_E("CFilesWindow::ChangePathToPluginFS() - incorrect use of 'mode' or 'canFocusFileName'.");
        canFocusFileName = FALSE;
    }

    if (strlen(fsUserPart) >= MAX_PATH)
    {
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        MessageBox(HWindow, LoadStr(IDS_TOOLONGPATH), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    // udelame zalozni kopie
    char backup2[MAX_PATH];
    lstrcpyn(backup2, fsUserPart, MAX_PATH);
    fsUserPart = backup2;
    char* fsUserPart2 = backup2;
    char backup3[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup3, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup3;
    }

    // obnovime udaje o stavu panelu (top-index + focused-name) pred pripadnym zavrenim teto cesty
    RefreshPathHistoryData();

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    //---  nahozeni hodin
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // neprejeme si zadne refreshe

    BOOL ok = FALSE;
    BOOL shorterPath;
    char cutFileNameBuf[MAX_PATH];
    int fsNameIndex;
    if (!Is(ptPluginFS) || !IsPathFromActiveFS(fsName, fsUserPart2, fsNameIndex, convertPathToInternal))
    { // neni FS nebo cesta je z jineho FS (i v ramci jednoho plug-inu - jednoho jmena FS)
        BOOL detachFS;
        if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
        { // soucasnou cestu pujde zavrit, zkusime otevrit novou cestu
            int index;
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
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
                        if (convertPathToInternal) // prevedeme cestu na interni format
                            pluginFS.GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart2);
                        // vytvorime si novy objekt pro obsah akt. cesty file systemu
                        CSalamanderDirectory* newFSDir = new CSalamanderDirectory(TRUE);
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                        CPluginDataInterfaceAbstract* pluginData;
                        int pluginIconsType;
                        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // fokus souboru jen pokud neni navrzeny zadny jiny fokus
                        if (ChangeAndListPathOnFS(fsName, fsNameIndex, fsUserPart2, pluginFS, newFSDir, pluginData,
                                                  shorterPath, pluginIconsType, mode, FALSE, NULL, NULL, -1,
                                                  FALSE, cutFileName, NULL))
                        {                    // uspech, cesta (nebo podcesta) byla vylistovana
                            if (shorterPath) // podcesta?
                            {
                                // zneplatnime navrhovane nastaveni listboxu (budeme listovat jinou cestu)
                                suggestedTopIndex = -1;
                                if (cutFileName != NULL && *cutFileName != 0)
                                    suggestedFocusName = cutFileName; // fokus souboru
                                else
                                    suggestedFocusName = NULL;
                                if (failReason != NULL)
                                {
                                    *failReason = cutFileName != NULL && *cutFileName != 0 ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH;
                                }
                            }
                            else
                            {
                                if (failReason != NULL)
                                    *failReason = CHPPFR_SUCCESS;
                            }

                            if (UseSystemIcons || UseThumbnails)
                                SleepIconCacheThread();

                            CloseCurrentPath(HWindow, FALSE, detachFS, FALSE, isRefresh, TRUE); // uspech, prejdeme na novou cestu

                            // prave jsme obdrzeli novy listing, pokud jsou hlaseny nejake zmeny v panelu, stornujeme je
                            InvalidateChangesInPanelWeHaveNewListing();

                            // schovame throbbera a ikonu zabezpeceni, jestli o ne stoji novy FS, musi si je zapnout (napr. v FSE_OPENED nebo FSE_PATHCHANGED)
                            if (DirectoryLine != NULL)
                                DirectoryLine->HideThrobberAndSecurityIcon();

                            SetPanelType(ptPluginFS);
                            SetPath(GetPath()); // odpojeni cesty od Snoopera (konec sledovani zmen na Path)
                            SetPluginFS(pluginFS.GetInterface(), plugin->DLLName, plugin->Version,
                                        plugin->GetPluginInterfaceForFS()->GetInterface(),
                                        plugin->GetPluginInterface()->GetInterface(),
                                        pluginFS.GetPluginFSName(), pluginFS.GetPluginFSNameIndex(),
                                        pluginFS.GetPluginFSCreateTime(), pluginFS.GetChngDrvDuplicateItemIndex(),
                                        plugin->BuiltForVersion);
                            SetPluginIface(plugin->GetPluginInterface()->GetInterface());
                            SetPluginFSDir(newFSDir);
                            PluginData.Init(pluginData, plugin->DLLName, plugin->Version,
                                            plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
                            SetPluginIconsType(pluginIconsType);
                            SetValidFileData(GetPluginFSDir()->GetValidData());

                            if (noChange != NULL)
                                *noChange = FALSE;

                            // obnovime panel
                            UpdateDriveIcon(FALSE); // ziskame z plug-inu ikonku pro aktualni cestu
                            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);

                            // oznamime FS, ze je konecne otevreny
                            GetPluginFS()->Event(FSE_OPENED, GetPanelCode());
                            GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());

                            ok = TRUE;
                        }
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                        if (!ok)
                        {
                            delete newFSDir;
                            pluginFS.ReleaseObject(HWindow);
                            plugin->GetPluginInterfaceForFS()->CloseFS(pluginFS.GetInterface());
                        }
                        Plugins.SetWorkingPluginFS(NULL);
                    }
                    else
                        TRACE_I("Plugin has refused to open FS (maybe it even does not start).");
                }
                else
                    TRACE_E("Unexpected situation in CFilesWindow::ChangePathToPluginFS()");
            }
            else
                TRACE_I("Plugin containing file-system name " << fsName << " is no longer available.");

            if (!ok)
                CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, isRefresh, FALSE); // neuspech, zustaneme na puvodni ceste

            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            //TRACE_I("change-to-fs: end");
            return ok ? !shorterPath : FALSE;
        }
        else // soucasna cesta nejde zavrit
        {
            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            if (failReason != NULL)
                *failReason = CHPPFR_CANNOTCLOSEPATH;
            return FALSE;
        }
    }
    else
    {
        // poznamka: convertPathToInternal uz musi byt FALSE (cesta se prevedla v IsPathFromActiveFS())

        // PluginFS odpovida fsName a cesta fsUserPart2 se na nem da overit
        BOOL samePath = GetPluginFS()->IsCurrentPath(GetPluginFS()->GetPluginFSNameIndex(), fsNameIndex, fsUserPart2);
        if (!forceUpdate && samePath) // cesta je identicka se soucasnou cestou
        {
            // aby se nevyignorovali 'suggestedTopIndex' a 'suggestedFocusName', zavolame CommonRefresh
            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, FALSE, isRefresh);

            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            if (failReason != NULL)
                *failReason = CHPPFR_SUCCESS;
            //TRACE_I("change-to-fs: end");
            return TRUE; // neni co delat
        }

        // zazalohujeme si soucasnou cestu na FS (v pripade chyby ji zkusime znovu zvolit)
        BOOL currentPathOK = TRUE;
        char currentPath[MAX_PATH];
        if (!GetPluginFS()->GetCurrentPath(currentPath))
            currentPathOK = FALSE;
        char currentPathFSName[MAX_PATH];
        strcpy(currentPathFSName, GetPluginFS()->GetPluginFSName());
        int currentPathFSNameIndex = GetPluginFS()->GetPluginFSNameIndex();

        int originalTopIndex = ListBox->GetTopIndex();
        char originalFocusName[MAX_PATH];
        originalFocusName[0] = 0;
        if (FocusedIndex >= 0)
        {
            CFileData* file = NULL;
            if (FocusedIndex < Dirs->Count)
                file = &Dirs->At(FocusedIndex);
            else
            {
                if (FocusedIndex < Files->Count + Dirs->Count)
                    file = &Files->At(FocusedIndex - Dirs->Count);
            }
            if (file != NULL)
                lstrcpyn(originalFocusName, file->Name, MAX_PATH);
        }

        // zkusime zmenit cestu na soucasnem FS
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        CPluginDataInterfaceAbstract* pluginData;
        int pluginIconsType;
        BOOL cancel;
        BOOL keepOldListing = TRUE;
        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // fokus souboru jen pokud neni navrzeny zadny jiny fokus
        if (ChangeAndListPathOnFS(fsName, fsNameIndex, fsUserPart2, *GetPluginFS(), GetPluginFSDir(),
                                  pluginData, shorterPath, pluginIconsType, mode, TRUE, &cancel,
                                  currentPathOK ? currentPath : NULL, currentPathFSNameIndex, forceUpdate,
                                  cutFileName, &keepOldListing))
        { // uspech, cesta (nebo podcesta) byla vylistovana
            if (failReason != NULL)
            {
                *failReason = shorterPath ? (cutFileName != NULL && *cutFileName != 0 ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH) : CHPPFR_SUCCESS;
            }

            if (!cancel) // jen pokud byl nacten novy obsah (nezustal nacteny puvodni obsah)
            {
                // prave jsme obdrzeli novy listing, pokud jsou hlaseny nejake zmeny v panelu, stornujeme je
                InvalidateChangesInPanelWeHaveNewListing();

                // schovame throbbera a ikonu zabezpeceni, jestli o ne FS stoji i nadale, musi si je znovu zapnout (napr. v FSE_PATHCHANGED)
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                // pridame cestu, kterou jsme prave opustili (uvnitr FS se cesty nezaviraji,
                // takze se DirHistoryAddPathUnique jeste nevolalo)
                if (currentPathOK && (!samePath || samePath && shorterPath)) // doslo ke zmene cesty
                {
                    if (UserWorkedOnThisPath)
                    {
                        MainWindow->DirHistoryAddPathUnique(2, currentPathFSName, currentPath, NULL,
                                                            GetPluginFS()->GetInterface(), GetPluginFS());
                        UserWorkedOnThisPath = FALSE;
                    }
                    // opoustime cestu
                    HiddenNames.Clear();  // uvolnime skryte nazvy
                    OldSelection.Clear(); // a starou selection
                }

                if (shorterPath) // podcesta?
                {
                    // zneplatnime navrhovane nastaveni listboxu (budeme listovat jinou cestu)
                    suggestedTopIndex = -1;
                    if (cutFileName != NULL && *cutFileName != 0)
                        suggestedFocusName = cutFileName; // fokus souboru
                    else
                    {
                        suggestedFocusName = NULL;

                        // nova cesta se zkratila na puvodni cestu ("nelistovatelny podadresar"), zachovame
                        // topIndex a focusName pred zahajenim operace (at se userovi neztrati fokus)
                        if (currentPathOK &&
                            GetPluginFS()->IsCurrentPath(GetPluginFS()->GetPluginFSNameIndex(),
                                                         currentPathFSNameIndex, currentPath))
                        {
                            suggestedTopIndex = originalTopIndex;
                            suggestedFocusName = originalFocusName[0] == 0 ? NULL : originalFocusName;
                        }
                    }
                }

                //        if (UseSystemIcons || UseThumbnails) SleepIconCacheThread();   // vola se v ChangeAndListPathOnFS

                // ke stavajicimu (nove naplnenemu) PluginFSDir pridame nove pluginData
                PluginData.Init(pluginData, GetPluginFS()->GetDLLName(),
                                GetPluginFS()->GetVersion(), GetPluginFS()->GetPluginInterface(),
                                GetPluginFS()->GetBuiltForVersion());
                SetPluginIconsType(pluginIconsType);
                if (SimplePluginIcons != NULL)
                {
                    delete SimplePluginIcons;
                    SimplePluginIcons = NULL;
                }
                SetValidFileData(GetPluginFSDir()->GetValidData());

                if (noChange != NULL)
                    *noChange = FALSE;

                // vycistime message-queue od nabufferovane WM_USER_UPDATEPANEL
                MSG msg2;
                PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);

                // obnovime panel
                UpdateDriveIcon(FALSE); // ziskame z plug-inu ikonku pro aktualni cestu
                CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);

                // oznamime FS, ze se zmenila cesta
                GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());
            }
            else
            {
                if (shorterPath && cutFileName != NULL && *cutFileName != 0 && refreshListBox) // je treba fokusnout soubor
                {
                    int focusIndexCase = -1;
                    int focusIndexIgnCase = -1;
                    int i;
                    for (i = 0; i < Dirs->Count; i++)
                    { // pro konzistenci s CommonRefresh hledame nejdrive v adresarich,
                        // pak v souborech (aby se to chovalo stejne v obou pripadech)
                        if (StrICmp(Dirs->At(i).Name, cutFileName) == 0)
                        {
                            if (focusIndexIgnCase == -1)
                                focusIndexIgnCase = i;
                            if (strcmp(Dirs->At(i).Name, cutFileName) == 0)
                            {
                                focusIndexCase = i;
                                break;
                            }
                        }
                    }
                    if (i == Dirs->Count)
                    {
                        for (i = 0; i < Files->Count; i++)
                        {
                            if (StrICmp(Files->At(i).Name, cutFileName) == 0)
                            {
                                if (focusIndexIgnCase == -1)
                                    focusIndexIgnCase = i + Dirs->Count;
                                if (strcmp(Files->At(i).Name, cutFileName) == 0)
                                {
                                    focusIndexCase = i + Dirs->Count;
                                    break;
                                }
                            }
                        }
                    }

                    if (focusIndexIgnCase != -1) // byl nalezen aspon soubor s moznym rozdilem ve velikosti pismen
                    {
                        SetCaretIndex(focusIndexCase != -1 ? focusIndexCase : focusIndexIgnCase, FALSE);
                    }
                }
            }

            ok = TRUE;
        }
        else // pozadovana cesta neni dostupna, zkusime se vratit na puvodni cestu
        {
            if (noChange != NULL)
                *noChange = FALSE; // listing bude vycisteny nebo zmeneny
            if (!samePath &&       // pokud nejde o refresh (zmena na stejnou cestu)
                currentPathOK &&   // pokud se puvodni cestu podarilo ziskat
                ChangeAndListPathOnFS(currentPathFSName, currentPathFSNameIndex, currentPath,
                                      *GetPluginFS(), GetPluginFSDir(),
                                      pluginData, shorterPath, pluginIconsType, mode,
                                      FALSE, NULL, NULL, -1, FALSE, NULL, &keepOldListing))
            { // uspech, puvodni cesta (nebo jeji podcesta) byla vylistovana
                // zneplatnime navrhovane nastaveni listboxu (budeme listovat jinou cestu)
                suggestedTopIndex = -1;
                suggestedFocusName = NULL;

                // puvodni cesta je pristupna (nemuseli jsme zkracovat), zachovame topIndex
                // a focusName pred zahajenim operace (at se userovi neztrati fokus)
                if (!shorterPath)
                {
                    suggestedTopIndex = originalTopIndex;
                    suggestedFocusName = originalFocusName[0] == 0 ? NULL : originalFocusName;
                }

                // pridame cestu, kterou jsme prave opustili (uvnitr FS se cesty nezaviraji,
                // takze se DirHistoryAddPathUnique jeste nevolalo)
                if (currentPathOK && shorterPath) // pokud neni shorterPath, cesta se nezmenila...
                {
                    if (UserWorkedOnThisPath)
                    {
                        MainWindow->DirHistoryAddPathUnique(2, currentPathFSName, currentPath, NULL,
                                                            GetPluginFS()->GetInterface(), GetPluginFS());
                        UserWorkedOnThisPath = FALSE;
                    }
                    // opoustime cestu
                    HiddenNames.Clear();  // uvolnime skryte nazvy
                    OldSelection.Clear(); // a starou selection
                }

                // prave jsme obdrzeli novy listing, pokud jsou hlaseny nejake zmeny v panelu, stornujeme je
                InvalidateChangesInPanelWeHaveNewListing();

                // schovame throbbera a ikonu zabezpeceni, jestli o ne FS stoji i nadale, musi si je znovu zapnout (napr. v FSE_PATHCHANGED)
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                // ke stavajicimu (nove naplnenemu) PluginFSDir pridame nove pluginData
                PluginData.Init(pluginData, GetPluginFS()->GetDLLName(),
                                GetPluginFS()->GetVersion(), GetPluginFS()->GetPluginInterface(),
                                GetPluginFS()->GetBuiltForVersion());
                SetPluginIconsType(pluginIconsType);
                if (SimplePluginIcons != NULL)
                {
                    delete SimplePluginIcons;
                    SimplePluginIcons = NULL;
                }
                SetValidFileData(GetPluginFSDir()->GetValidData());

                // vycistime message-queue od nabufferovane WM_USER_UPDATEPANEL
                MSG msg2;
                PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);

                // obnovime panel
                UpdateDriveIcon(FALSE); // ziskame z plug-inu ikonku pro aktualni cestu
                CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);
                if (failReason != NULL)
                    *failReason = mode == 3 ? CHPPFR_INVALIDPATH : CHPPFR_SHORTERPATH; // bylo tu jen CHPPFR_SHORTERPATH, ale Shift+F7 z dfs:x:\zumpa na dfs:x:\zumpa\aaa jen ohlasilo chybu cesty a nevracelo se do Shift+F7 dialogu

                // oznamime FS, ze se zmenila cesta
                GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());
            }
            else // zobrazime prazdny panel, z FS nejde nic nacist, prepneme na fixed-drive
            {
                if (keepOldListing)
                {
                    // uvolnime stary listing
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();
                    ReleaseListing();
                    // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (prave jsme podrizli data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                }
                else // neni dead-code, viz chyba alokace 'workDir' v ChangeAndListPathOnFS()
                {
                    // vycistime message-queue od nabufferovane WM_USER_UPDATEPANEL
                    MSG msg2;
                    PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);
                }

                // schovame throbbera a ikonu zabezpeceni, protoze FS opoustime...
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                // nutne, nepouzivat 'refreshListBox' - panel bude prazdny "delsi" dobu (+ hrozi message-boxy)
                SetPluginIconsType(pitSimple); // PluginData==NULL, pitFromPlugin nelze ani s prazdnym panelem
                if (SimplePluginIcons != NULL)
                {
                    delete SimplePluginIcons;
                    SimplePluginIcons = NULL;
                }
                // SetValidFileData(VALID_DATA_ALL_FS_ARC);   // nechame soucasnou hodnotu, neni duvod menit
                CommonRefresh(HWindow, -1, NULL, TRUE, TRUE, isRefresh);

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                ChangeToRescuePathOrFixedDrive(HWindow, NULL, refreshListBox, FALSE, FSTRYCLOSE_CHANGEPATHFAILURE);

                EndStopRefresh();
                if (setWait)
                    SetCursor(oldCur);
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                return FALSE;
            }
        }

        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);

        //TRACE_I("change-to-fs: end");
        return ok ? !shorterPath : FALSE;
    }
}

BOOL CFilesWindow::ChangePathToDetachedFS(int fsIndex, int suggestedTopIndex,
                                          const char* suggestedFocusName, BOOL refreshListBox,
                                          int* failReason, const char* newFSName,
                                          const char* newUserPart, int mode, BOOL canFocusFileName)
{
    CALL_STACK_MESSAGE9("CFilesWindow::ChangePathToDetachedFS(%d, %d, %s, %d, , %s, %s, %d, %d)", fsIndex,
                        suggestedTopIndex, suggestedFocusName, refreshListBox, newFSName, newUserPart,
                        mode, canFocusFileName);

    char backup[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup;
    }
    if (newUserPart == NULL || newFSName == NULL)
    {
        newUserPart = NULL;
        newFSName = NULL;
    }
    char backup2[MAX_PATH];
    if (newUserPart != NULL)
    {
        lstrcpyn(backup2, newUserPart, MAX_PATH);
        newUserPart = backup2;
    }
    char backup3[MAX_PATH];
    if (newFSName != NULL)
    {
        lstrcpyn(backup3, newFSName, MAX_PATH);
        newFSName = backup3;
    }

    // obnovime udaje o stavu panelu (top-index + focused-name) pred pripadnym zavrenim teto cesty
    RefreshPathHistoryData();

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    // ziskame z DetachedFSList zapouzdreni FS-ifacu
    if (!MainWindow->DetachedFSList->IsGood())
    { // aby vysel pozdejsi Delete, musi byt pole o.k.
        TRACE_E("DetachedFSList array returns error, unable to finish operation.");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    if (fsIndex < 0 || fsIndex >= MainWindow->DetachedFSList->Count)
    {
        TRACE_E("Invalid index of detached FS: fsIndex=" << fsIndex);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CPluginFSInterfaceEncapsulation* pluginFS = MainWindow->DetachedFSList->At(fsIndex);

    // ziskame fs-name odpojeneho FS
    char fsName[MAX_PATH];
    int fsNameIndex;
    if (newFSName != NULL) // pokud se mame prepnout na nove fs-name, musime zjistit jestli existuje a jaky ma fs-name-index
    {
        strcpy(fsName, newFSName);
        int i;
        if (!Plugins.IsPluginFS(fsName, i, fsNameIndex)) // "always false" (plugin se neunloadnul, fs-name nemohlo zmizet)
        {
            TRACE_E("CFilesWindow::ChangePathToDetachedFS(): unexpected situation: requested FS was not found! fs-name=" << newFSName);
            newUserPart = NULL;
            newFSName = NULL;
        }
    }
    if (newFSName == NULL)
    {
        strcpy(fsName, pluginFS->GetPluginFSName());
        fsNameIndex = pluginFS->GetPluginFSNameIndex();
    }
    if (mode == -1)
        mode = newUserPart == NULL ? 1 : 2 /* refresh nebo history */;

    if (mode != 3 && canFocusFileName)
    {
        TRACE_E("CFilesWindow::ChangePathToDetachedFS() - incorrect use of 'mode' or 'canFocusFileName'.");
        canFocusFileName = FALSE;
    }

    CPluginData* plugin = Plugins.GetPluginData(pluginFS->GetPluginInterfaceForFS()->GetInterface());
    if (plugin == NULL)
    {
        TRACE_E("Unexpected situation in CFilesWindow::ChangePathToDetachedFS.");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }

    //---  nahozeni hodin
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // neprejeme si zadne refreshe

    BOOL ok = FALSE;
    BOOL shorterPath;
    char cutFileNameBuf[MAX_PATH];

    // neni FS nebo cesta je z jineho FS (i v ramci jednoho plug-inu - jednoho jmena FS)
    BOOL detachFS;
    if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
    { // soucasnou cestu pujde zavrit, zkusime otevrit novou cestu
        // vytvorime si novy objekt pro obsah akt. cesty file systemu
        CSalamanderDirectory* newFSDir = new CSalamanderDirectory(TRUE);
        BOOL closeDetachedFS = FALSE;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        CPluginDataInterfaceAbstract* pluginData;
        int pluginIconsType;
        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // fokus souboru jen pokud neni navrzeny zadny jiny fokus
        if (ChangeAndListPathOnFS(fsName, fsNameIndex, newUserPart, *pluginFS, newFSDir, pluginData,
                                  shorterPath, pluginIconsType, mode,
                                  FALSE, NULL, NULL, -1, FALSE, cutFileName, NULL))
        {                    // uspech, cesta (nebo podcesta) byla vylistovana
            if (shorterPath) // podcesta?
            {
                // zneplatnime navrhovane nastaveni listboxu (budeme listovat jinou cestu)
                suggestedTopIndex = -1;
                if (cutFileName != NULL && *cutFileName != 0)
                    suggestedFocusName = cutFileName; // fokus souboru
                else
                    suggestedFocusName = NULL;
                if (failReason != NULL)
                {
                    *failReason = cutFileName != NULL && *cutFileName != 0 ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH;
                }
            }
            else
            {
                if (failReason != NULL)
                    *failReason = CHPPFR_SUCCESS;
            }

            if (UseSystemIcons || UseThumbnails)
                SleepIconCacheThread();

            CloseCurrentPath(HWindow, FALSE, detachFS, FALSE, FALSE, TRUE); // uspech, prejdeme na novou cestu

            // prave jsme obdrzeli novy listing, pokud jsou hlaseny nejake zmeny v panelu, stornujeme je
            InvalidateChangesInPanelWeHaveNewListing();

            // schovame throbbera a ikonu zabezpeceni, jestli o ne stoji odpojeny FS, musi si je zapnout (napr. v FSE_ATTACHED nebo FSE_PATHCHANGED)
            if (DirectoryLine != NULL)
                DirectoryLine->HideThrobberAndSecurityIcon();

            SetPanelType(ptPluginFS);
            SetPath(GetPath()); // odpojeni cesty od Snoopera (konec sledovani zmen na Path)
            SetPluginFS(pluginFS->GetInterface(), plugin->DLLName, plugin->Version,
                        plugin->GetPluginInterfaceForFS()->GetInterface(),
                        plugin->GetPluginInterface()->GetInterface(),
                        pluginFS->GetPluginFSName(), pluginFS->GetPluginFSNameIndex(),
                        pluginFS->GetPluginFSCreateTime(), pluginFS->GetChngDrvDuplicateItemIndex(),
                        plugin->BuiltForVersion);
            SetPluginIface(plugin->GetPluginInterface()->GetInterface());
            SetPluginFSDir(newFSDir);
            PluginData.Init(pluginData, plugin->DLLName, plugin->Version,
                            plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
            SetPluginIconsType(pluginIconsType);
            SetValidFileData(GetPluginFSDir()->GetValidData());

            // obnovime panel
            UpdateDriveIcon(FALSE); // ziskame z plug-inu ikonku pro aktualni cestu
            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox);

            // oznamime FS, ze uz je zase pripojeny
            GetPluginFS()->Event(FSE_ATTACHED, GetPanelCode());
            GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());

            ok = TRUE;
        }
        else
        {
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
            closeDetachedFS = TRUE;
        }
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        if (!ok)
        {
            delete newFSDir;
            CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, FALSE, FALSE); // neuspech, zustaneme na puvodni ceste
        }

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);

        if (ok)
        {
            // uspesne pripojeni, vyradime FS ze seznamu odpojenych (uspech Delete je jisty)
            MainWindow->DetachedFSList->Delete(fsIndex);
            if (!MainWindow->DetachedFSList->IsGood())
                MainWindow->DetachedFSList->ResetState();
        }
        else
        {
            if (closeDetachedFS) // na FS uz nelze otevrit zadnou cestu, zavreme ho
            {
                BOOL dummy;
                if (pluginFS->TryCloseOrDetach(FALSE, FALSE, dummy, FSTRYCLOSE_ATTACHFAILURE))
                { // optame se usera jestli ho chce zavrit nebo nechat (ve stavu po fatalni chybe ChangePath())
                    pluginFS->ReleaseObject(HWindow);
                    plugin->GetPluginInterfaceForFS()->CloseFS(pluginFS->GetInterface());
                    // vyradime FS ze seznamu odpojenych (uspech Delete je jisty)
                    MainWindow->DetachedFSList->Delete(fsIndex);
                    if (!MainWindow->DetachedFSList->IsGood())
                        MainWindow->DetachedFSList->ResetState();
                }
            }
        }

        return ok ? !shorterPath : FALSE;
    }
    else // soucasna cesta nejde zavrit
    {
        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);

        if (failReason != NULL)
            *failReason = CHPPFR_CANNOTCLOSEPATH;
        return FALSE;
    }
}

void CFilesWindow::RefreshDiskFreeSpace(BOOL check, BOOL doNotRefreshOtherPanel)
{
    CALL_STACK_MESSAGE3("CFilesWindow::RefreshDiskFreeSpace(%d, %d)", check, doNotRefreshOtherPanel);
    if (Is(ptDisk))
    {
        if (!check || CheckPath(FALSE) == ERROR_SUCCESS)
        { // jen je-li cesta pristupna
            CQuadWord r = MyGetDiskFreeSpace(GetPath());
            DirectoryLine->SetSize(r);

            if (!doNotRefreshOtherPanel)
            {
                // pokud je ve vedlejsim panelu cesta se stejnym rootem, provedeme refresh
                // disk-free-space i ve vedlejsim panelu (neni dokonale - idealni by bylo
                // testovat, jestli jsou cesty na stejnem svazku, ale to by bylo moc pomale,
                // toto zjednoduseni bude pro obycejne pouziti bohate stacit)
                CFilesWindow* otherPanel = (MainWindow->LeftPanel == this) ? MainWindow->RightPanel : MainWindow->LeftPanel;
                if (otherPanel->Is(ptDisk) && HasTheSameRootPath(GetPath(), otherPanel->GetPath()))
                    otherPanel->RefreshDiskFreeSpace(TRUE, TRUE /* jinak nekonecna rekurze */);
            }
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            DirectoryLine->SetSize(CQuadWord(-1, -1)); // u archivu nema volne misto valny smysl, schovame udaj
        }
        else
        {
            if (Is(ptPluginFS))
            {
                CQuadWord r;
                GetPluginFS()->GetFSFreeSpace(&r); // ziskame od plug-inu volne misto na FS
                DirectoryLine->SetSize(r);
            }
        }
    }
}

void CFilesWindow::GetContextMenuPos(POINT* p)
{
    CALL_STACK_MESSAGE_NONE
    RECT r;
    if (!ListBox->GetItemRect(FocusedIndex, &r))
    {
        GetWindowRect(GetListBoxHWND(), &r);
        p->x = r.left;
        p->y = r.top;
        return;
    }
    p->x = r.left + 18;
    p->y = r.bottom;
    ClientToScreen(GetListBoxHWND(), p);
}

void GetCommonFileTypeStr(char* buf, int* resLen, const char* ext)
{
    char uppercaseExt[MAX_PATH];
    char* d = uppercaseExt;
    char* end = uppercaseExt + MAX_PATH - 1;
    while (d < end && *ext != 0 && *ext != ' ')
        *d++ = UpperCase[*ext++];
    *d = 0;
    if (*ext == 0 && uppercaseExt[0] != 0)
    { // mame upper-case celou priponu (neobsahuje mezery a neni delsi nez MAX_PATH) + neni prazdna
        *resLen = _snprintf_s(buf, TRANSFER_BUFFER_MAX, _TRUNCATE, CommonFileTypeName2, uppercaseExt);
        if (*resLen < 0)
            *resLen = TRANSFER_BUFFER_MAX - 1; // _snprintf_s hlasi orez na velikost bufferu
    }
    else
    {
        memcpy(buf, CommonFileTypeName, CommonFileTypeNameLen + 1);
        *resLen = CommonFileTypeNameLen;
    }
}

void CFilesWindow::RefreshListBox(int suggestedXOffset,
                                  int suggestedTopIndex, int suggestedFocusIndex,
                                  BOOL ensureFocusIndexVisible, BOOL wholeItemVisible)
{
    CALL_STACK_MESSAGE6("CFilesWindow::RefreshListBox(%d, %d, %d, %d, %d)", suggestedXOffset,
                        suggestedTopIndex, suggestedFocusIndex, ensureFocusIndexVisible, wholeItemVisible);

    //TRACE_I("refreshlist: begin");

    KillQuickRenameTimer(); // zamezime pripadnemu otevreni QuickRenameWindow

    NarrowedNameColumn = FALSE;
    FullWidthOfNameCol = 0;
    WidthOfMostOfNames = 0;

    HDC dc = HANDLES(GetDC(GetListBoxHWND()));
    HFONT of = (HFONT)SelectObject(dc, Font);
    SIZE act;

    char formatedFileName[MAX_PATH];
    switch (GetViewMode())
    {
    case vmBrief:
    {
        SIZE max;
        max.cx = 0;
        act.cy = 0;
        int i;
        for (i = 0; i < Dirs->Count; i++)
        {
            CFileData* f = &Dirs->At(i);
            AlterFileName(formatedFileName, f->Name, f->NameLen,
                          Configuration.FileNameFormat, 0, TRUE);
            GetTextExtentPoint32(dc, formatedFileName, f->NameLen, &act);
            if (max.cx < act.cx)
                max.cx = act.cx;
        }
        for (i = 0; i < Files->Count; i++)
        {
            CFileData* f = &Files->At(i);
            AlterFileName(formatedFileName, f->Name, f->NameLen,
                          Configuration.FileNameFormat, 0, FALSE);
            GetTextExtentPoint32(dc, formatedFileName, f->NameLen, &act);
            if (max.cx < act.cx)
                max.cx = act.cx;
        }
        max.cy = act.cy;
        CaretHeight = (short)act.cy;

        max.cx += 2 * IconSizes[ICONSIZE_16];
        // minimalni sirka (napriklad pro prazdny panel) aby user dokazal trefit UpDir
        if (max.cx < 4 * IconSizes[ICONSIZE_16])
            max.cx = 4 * IconSizes[ICONSIZE_16];
        Columns[0].Width = max.cx; // sirka sloupce 'Name'
        max.cy += 4;
        if (max.cy < IconSizes[ICONSIZE_16] + 1)
            max.cy = IconSizes[ICONSIZE_16] + 1;
        ListBox->SetItemWidthHeight(max.cx, max.cy);
        break;
    }

    case vmIcons:
    {
        int w = IconSizes[ICONSIZE_32];
        int h = IconSizes[ICONSIZE_32];
        w += Configuration.IconSpacingHorz;
        h += Configuration.IconSpacingVert;
        ListBox->SetItemWidthHeight(w, h);
        break;
    }

    case vmThumbnails:
    {
        int w = ListBox->ThumbnailWidth + 2;
        int h = ListBox->ThumbnailHeight + 2;
        w += Configuration.ThumbnailSpacingHorz;
        h += Configuration.IconSpacingVert;
        ListBox->SetItemWidthHeight(w, h);
        break;
    }

    case vmTiles:
    {
        int w = IconSizes[ICONSIZE_48];
        int h = IconSizes[ICONSIZE_48];
        if (w < 48)
            w = 48; // pro 32x32 sirka nedostacovala
        w += (int)(2.5 * (double)w);
        h += Configuration.TileSpacingVert;
        int textH = 3 * FontCharHeight + 4;
        ListBox->SetItemWidthHeight(w, max(textH, h));
        break;
    }

    case vmDetailed:
    {
        // Detailed view
        int columnWidthName = 0;
        int columnWidthExt = 0;
        int columnWidthDosName = 0;
        int columnWidthSize = 0;
        int columnWidthType = 0;
        int columnWidthDate = 0;
        int columnWidthTime = 0;
        int columnWidthAttr = 0;
        int columnWidthDesc = 0;

        // zjistime, ktere sloupce jsou skutecne zobrazene (plugin mohl sloupce zmodifikovat)
        BOOL extColumnIsVisible = FALSE;
        DWORD autoWidthColumns = 0;
        int i;
        for (i = 0; i < Columns.Count; i++)
        {
            CColumn* column = &Columns[i];

            if (column->ID == COLUMN_ID_EXTENSION)
                extColumnIsVisible = TRUE;
            if (column->FixedWidth == 0)
            {
                switch (column->ID)
                {
                case COLUMN_ID_EXTENSION:
                    autoWidthColumns |= VIEW_SHOW_EXTENSION;
                    break;
                case COLUMN_ID_DOSNAME:
                    autoWidthColumns |= VIEW_SHOW_DOSNAME;
                    break;
                case COLUMN_ID_SIZE:
                    autoWidthColumns |= VIEW_SHOW_SIZE;
                    break;
                case COLUMN_ID_TYPE:
                    autoWidthColumns |= VIEW_SHOW_TYPE;
                    break;
                case COLUMN_ID_DATE:
                    autoWidthColumns |= VIEW_SHOW_DATE;
                    break;
                case COLUMN_ID_TIME:
                    autoWidthColumns |= VIEW_SHOW_TIME;
                    break;
                case COLUMN_ID_ATTRIBUTES:
                    autoWidthColumns |= VIEW_SHOW_ATTRIBUTES;
                    break;
                case COLUMN_ID_DESCRIPTION:
                    autoWidthColumns |= VIEW_SHOW_DESCRIPTION;
                    break;
                }
            }
        }

        i = 0;

        int dirsCount = Dirs->Count;
        int totalCount = Files->Count + Dirs->Count;
        if (dirsCount > 0)
        {
            GetTextExtentPoint32(dc, DirColumnStr, DirColumnStrLen, &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthSize < act.cx)
                columnWidthSize = act.cx;
        }
        else
            act.cx = act.cy = 0;

        char text[50];

        DWORD attrSkipCache[10]; // optimaliace ziskavani sirky atributu
        int attrSkipCacheCount = 0;
        ZeroMemory(&attrSkipCache, sizeof(attrSkipCache));

        CQuadWord maxSize(0, 0);

        BOOL computeDate = autoWidthColumns & VIEW_SHOW_DATE;
        if (computeDate && (totalCount > 20))
        {
            // zjistim, jestli jsme schopni predvidat sirky
            if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, text, 50) != 0)
            {
                // zjistim, jestli format datumu neobsahuje svazky (dddd || MMMM),
                // ktere jsou v dusledku textove: (pondeli || kveten)
                if (strstr(text, "dddd") == NULL && strstr(text, "MMMM") == NULL)
                {
                    SYSTEMTIME st;
                    st.wMilliseconds = 0;
                    st.wMinute = 59;
                    st.wSecond = 59;
                    st.wHour = 10;
                    st.wYear = 2000;
                    st.wMonth = 12;
                    st.wDay = 24;
                    st.wDayOfWeek = 0; // nedele
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, text, 50) == 0)
                        sprintf(text, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                    GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
                    act.cx += SPACE_WIDTH;
                    if (columnWidthDate < act.cx)
                        columnWidthDate = act.cx;
                    computeDate = FALSE;
                }
            }
        }

        int* nameColWidths = NULL;
        if (totalCount > 0)
            nameColWidths = (int*)malloc(totalCount * sizeof(int));

        BOOL dirTypeDone = FALSE;
        int nameLen;
        for (i = 0; i < totalCount; i++)
        {
            BOOL isDir = i < Dirs->Count;
            CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - dirsCount);
            //--- name
            BOOL extIsInExtColumn = extColumnIsVisible && (!isDir || Configuration.SortDirsByExt) &&
                                    f->Ext[0] != 0 && f->Ext > f->Name + 1; // vyjimka pro jmena jako ".htaccess", ukazuji se ve sloupci Name i kdyz jde o pripony
            if (Columns[0].FixedWidth == 0 || (autoWidthColumns & VIEW_SHOW_EXTENSION) && extIsInExtColumn)
            {
                AlterFileName(formatedFileName, f->Name, f->NameLen, // priprava formatovaneho jmena i pro vypocet sire oddeleneho sloupce Ext
                              Configuration.FileNameFormat, 0, isDir);
                if (Columns[0].FixedWidth == 0)
                {
                    nameLen = extIsInExtColumn ? (int)(f->Ext - f->Name - 1) : f->NameLen;

                    GetTextExtentPoint32(dc, formatedFileName, nameLen, &act);
                    act.cx += 1 + IconSizes[ICONSIZE_16] + 1 + 2 + SPACE_WIDTH;
                    if (columnWidthName < act.cx)
                        columnWidthName = act.cx;
                    if (nameColWidths != NULL)
                        nameColWidths[i] = act.cx;
                }
            }
            //--- extension
            if ((autoWidthColumns & VIEW_SHOW_EXTENSION) && extIsInExtColumn)
            {
                GetTextExtentPoint32(dc, formatedFileName + (int)(f->Ext - f->Name), (int)(f->NameLen - (f->Ext - f->Name)), &act);
                act.cx += SPACE_WIDTH;
                if (columnWidthExt < act.cx)
                    columnWidthExt = act.cx;
            }
            //--- dosname
            if ((autoWidthColumns & VIEW_SHOW_DOSNAME) && f->DosName != NULL)
            {
                GetTextExtentPoint32(dc, f->DosName, (int)strlen(f->DosName), &act);
                act.cx += SPACE_WIDTH;
                if (columnWidthDosName < act.cx)
                    columnWidthDosName = act.cx;
            }
            //--- size
            if ((autoWidthColumns & VIEW_SHOW_SIZE) &&
                (!isDir || f->SizeValid)) // soubory a adresare s platnou napocitanou velikosti
            {
                if (f->Size > maxSize)
                    maxSize = f->Size;
            }
            //--- date || time
            if (computeDate)
            {
                SYSTEMTIME st;
                FILETIME ft;
                int len;
                if (!FileTimeToLocalFileTime(&f->LastWrite, &ft) ||
                    !FileTimeToSystemTime(&ft, &st))
                {
                    len = sprintf(text, LoadStr(IDS_INVALID_DATEORTIME));
                }
                else
                {
                    len = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, text, 50) - 1;
                    if (len < 0)
                        len = sprintf(text, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                }
                GetTextExtentPoint32(dc, text, len, &act);
                act.cx += SPACE_WIDTH;
                if (columnWidthDate < act.cx)
                    columnWidthDate = act.cx;
            }

            if (autoWidthColumns & VIEW_SHOW_ATTRIBUTES)
            {
                //--- attr
                // pripravim data pro cache !!! zde je treba naorovat pripadne dalsi merene atributy
                DWORD mask = f->Attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                                        FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE |
                                        FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_COMPRESSED |
                                        FILE_ATTRIBUTE_ENCRYPTED);
                if (mask != 0)
                {
                    if (mask != attrSkipCache[0] && mask != attrSkipCache[1] &&
                        mask != attrSkipCache[2] && mask != attrSkipCache[3] &&
                        mask != attrSkipCache[4] && mask != attrSkipCache[5] &&
                        mask != attrSkipCache[6] && mask != attrSkipCache[7] &&
                        mask != attrSkipCache[8] && mask != attrSkipCache[9])
                    {
                        GetAttrsString(text, f->Attr);
                        // tuhle kombinaci jsme jeste nemerili
                        GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
                        act.cx += SPACE_WIDTH;
                        if (columnWidthAttr < act.cx)
                            columnWidthAttr = act.cx;
                        if (attrSkipCacheCount < 10)
                        {
                            // jeste je misto, zaradim polozku da cache
                            attrSkipCache[attrSkipCacheCount] = mask;
                            attrSkipCacheCount++;
                        }
                        else
                            attrSkipCache[0] = mask; // dam ji na prvni misto
                    }
                }
            }

            if (autoWidthColumns & VIEW_SHOW_TYPE)
            {
                //--- file-type
                if (!isDir) // je to soubor
                {
                    char buf[TRANSFER_BUFFER_MAX];
                    BOOL commonFileType = TRUE;
                    if (f->Ext[0] != 0) // existuje pripona
                    {
                        char* dst = buf;
                        char* src = f->Ext;
                        while (*src != 0)
                            *dst++ = LowerCase[*src++];
                        *((DWORD*)dst) = 0;
                        int index;
                        if (Associations.GetIndex(buf, index))
                        {
                            src = Associations[index].Type;
                            if (src != NULL) // pokud nejde o prazdny retezec
                            {
                                commonFileType = FALSE;
                                GetTextExtentPoint32(dc, src, (int)strlen(src), &act);
                                act.cx += SPACE_WIDTH;
                                if (columnWidthType < act.cx)
                                    columnWidthType = act.cx;
                            }
                        }
                    }
                    if (commonFileType)
                    {
                        int resLen;
                        GetCommonFileTypeStr(buf, &resLen, f->Ext);
                        GetTextExtentPoint32(dc, buf, resLen, &act);
                        act.cx += SPACE_WIDTH;
                        if (columnWidthType < act.cx)
                            columnWidthType = act.cx;
                    }
                }
                else // je to adresar
                {
                    if (!dirTypeDone) // jen pokud uz jsme ho nepocitali
                    {
                        if (i == 0 && isDir && strcmp(f->Name, "..") == 0)
                        {
                            GetTextExtentPoint32(dc, UpDirTypeName, UpDirTypeNameLen, &act);
                        }
                        else
                        {
                            dirTypeDone = TRUE;
                            GetTextExtentPoint32(dc, FolderTypeName, FolderTypeNameLen, &act);
                        }
                        act.cx += SPACE_WIDTH;
                        if (columnWidthType < act.cx)
                            columnWidthType = act.cx;
                    }
                }
            }
        }

        // size
        if (autoWidthColumns & VIEW_SHOW_SIZE)
        {
            int numLen;
            switch (Configuration.SizeFormat)
            {
            case SIZE_FORMAT_BYTES:
            {
                numLen = NumberToStr2(text, maxSize);
                break;
            }

            case SIZE_FORMAT_KB: // pozor, stejny kod je na dalsim miste, hledat tuto konstantu
            {
                PrintDiskSize(text, maxSize, 3);
                numLen = (int)strlen(text);
                break;
            }

            case SIZE_FORMAT_MIXED:
            {
                sprintf(text, "1023 GB"); // nejhorsi mozny pripad
                numLen = (int)strlen(text);
                break;
            }
            }
            GetTextExtentPoint32(dc, text, numLen, &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthSize < act.cx)
                columnWidthSize = act.cx;
        }
        // time
        if (autoWidthColumns & VIEW_SHOW_TIME)
        {
            SYSTEMTIME st;
            st.wYear = 2000;
            st.wMonth = 1;
            st.wDayOfWeek = 6;
            st.wDay = 1;
            st.wMilliseconds = 0;
            st.wMinute = 59;
            st.wSecond = 59;
            st.wHour = 10; // dopoledne
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, text, 50) == 0)
                sprintf(text, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthTime < act.cx)
                columnWidthTime = act.cx;
            st.wHour = 23; // odpoledne
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, text, 50) == 0)
                sprintf(text, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthTime < act.cx)
                columnWidthTime = act.cx;
        }

        ListBox->HeaderLine.SetMinWidths();

        FullWidthOfNameCol = (WORD)columnWidthName;

        if (nameColWidths != NULL && Columns[0].FixedWidth == 0 && totalCount > 0)
        {
            if (totalCount > 1)
                IntSort(nameColWidths, 0, totalCount - 1);
            WidthOfMostOfNames = (DWORD)(1.2 * nameColWidths[(DWORD)(totalCount * 0.85)]); // dame 20% sirky navic, jednak budou lip videt extra dlouha jmena a jednak se ukazi cela jmena blizka zvolene hranici
            if (WidthOfMostOfNames * 1.2 >= FullWidthOfNameCol)
                WidthOfMostOfNames = FullWidthOfNameCol; // pokud staci rozsirit o 44% (1.2*1.2), aby se vesly vsechny jmena, udelame to
        }
        if (nameColWidths != NULL)
            free(nameColWidths);

        TransferPluginDataIface = PluginData.GetInterface();
        int totalWidth = 0;
        for (i = 0; i < Columns.Count; i++)
        {
            CColumn* column = &Columns[i];

            if (column->FixedWidth == 0)
            {
                switch (column->ID)
                {
                case COLUMN_ID_NAME:
                    column->Width = (WORD)columnWidthName;
                    break;
                case COLUMN_ID_EXTENSION:
                    column->Width = (WORD)columnWidthExt;
                    break;
                case COLUMN_ID_DOSNAME:
                    column->Width = (WORD)columnWidthDosName;
                    break;
                case COLUMN_ID_SIZE:
                    column->Width = (WORD)columnWidthSize;
                    break;
                case COLUMN_ID_TYPE:
                    column->Width = (WORD)columnWidthType;
                    break;
                case COLUMN_ID_DATE:
                    column->Width = (WORD)columnWidthDate;
                    break;
                case COLUMN_ID_TIME:
                    column->Width = (WORD)columnWidthTime;
                    break;
                case COLUMN_ID_ATTRIBUTES:
                    column->Width = (WORD)columnWidthAttr;
                    break;
                case COLUMN_ID_DESCRIPTION:
                    column->Width = (WORD)columnWidthDesc;
                    break;
                case COLUMN_ID_CUSTOM:
                {
                    TransferActCustomData = column->CustomData;
                    int columnMaxWidth = column->MinWidth;
                    // doptame se pluginu
                    int j;
                    for (j = 0; j < totalCount; j++)
                    {
                        if (j < Dirs->Count)
                        {
                            TransferFileData = &Dirs->At(j);
                            TransferIsDir = (j == 0 && strcmp(TransferFileData->Name, "..") == 0) ? 2 : 1;
                        }
                        else
                        {
                            TransferFileData = &Files->At(j - dirsCount);
                            TransferIsDir = 0;
                        }
                        TransferRowData = 0;
                        TransferAssocIndex = -2; // mozna zbytecne - pokud se nemuze volat InternalGetType()
                        column->GetText();
                        if (TransferLen > 0)
                        {
                            GetTextExtentPoint32(dc, TransferBuffer, TransferLen, &act);
                            act.cx += SPACE_WIDTH;
                            if (act.cx > columnMaxWidth)
                                columnMaxWidth = act.cx;
                        }
                        else
                            act.cx = 0;
                    }
                    column->Width = columnMaxWidth;
                    break;
                }

                default:
                    TRACE_E("Unknown type of column");
                    break;
                }
            }
            // osetrime meze, aby sirka nesla pod minimalni hodnoty header line
            if (column->Width < column->MinWidth)
                column->Width = column->MinWidth;

            totalWidth += column->Width;
        }

        // osetrime Smart Mode sloupce Name
        BOOL leftPanel = (MainWindow->LeftPanel == this);
        if (Columns[0].FixedWidth == 0 &&
            (leftPanel && ViewTemplate->LeftSmartMode || !leftPanel && ViewTemplate->RightSmartMode) &&
            ListBox->FilesRect.right - ListBox->FilesRect.left > 0) // jen pokud uz je files-box inicializovany
        {
            CColumn* column = &Columns[0];
            int narrow = totalWidth - (ListBox->FilesRect.right - ListBox->FilesRect.left);
            if (narrow > 0)
            {
                DWORD minWidth = WidthOfMostOfNames;
                if (minWidth > (DWORD)(0.75 * (ListBox->FilesRect.right - ListBox->FilesRect.left)))
                    minWidth = (DWORD)(0.75 * (ListBox->FilesRect.right - ListBox->FilesRect.left));
                if (minWidth < column->MinWidth)
                    minWidth = column->MinWidth;
                DWORD newWidth = max((int)(column->Width - narrow), (int)minWidth);
                NarrowedNameColumn = column->Width > newWidth;
                totalWidth -= column->Width - newWidth;
                column->Width = newWidth;
            }
        }

        CaretHeight = (short)FontCharHeight;

        int cy = FontCharHeight + 4;
        if (cy < IconSizes[ICONSIZE_16] + 1)
            cy = IconSizes[ICONSIZE_16] + 1;
        ListBox->SetItemWidthHeight(totalWidth, cy);
        break;
    }
    }

    SelectObject(dc, of);
    HANDLES(ReleaseDC(GetListBoxHWND(), dc));

    LastFocus = INT_MAX;
    FocusedIndex = 0;
    if (suggestedFocusIndex != -1)
    {
        FocusedIndex = suggestedFocusIndex;
        // pokud neni doporucen TopIndex nebo je vyzadovana
        // viditelnost focusIndex, napocitam novy TopIndex
        // -- prehlednejsi verze s podporou pro vmIcons a vmThumbnails
        // -- zmena pro castecne viditelne polozky: drive se prepocital TopIndex
        //    a doslo ke zbytecnemu cuknuti; ted TopIndex nechavame nezmeneny

        BOOL findTopIndex = TRUE; // TRUE - budeme hledat TopIndex; FALSE - nechame soucasny
        if (suggestedTopIndex != -1)
        {
            // nechame napocitat EntireItemsInColumn
            ListBox->SetItemsCount2(Files->Count + Dirs->Count);
            if (ensureFocusIndexVisible)
            {
                // musime zajistit viditelnost focusu
                switch (ListBox->ViewMode)
                {
                case vmBrief:
                {
                    if (suggestedFocusIndex < suggestedTopIndex)
                        break; // focus lezi pred panelem, musime najit lepsi TopIndex

                    int cols = (ListBox->FilesRect.right - ListBox->FilesRect.left +
                                ListBox->ItemWidth - 1) /
                               ListBox->ItemWidth;
                    if (cols < 1)
                        cols = 1;

                    if (wholeItemVisible)
                    {
                        if (suggestedTopIndex + cols * ListBox->EntireItemsInColumn <=
                            suggestedFocusIndex + ListBox->EntireItemsInColumn)
                            break; // focus lezi za panelem, musime najit lepsi TopIndex
                    }
                    else
                    {
                        if (suggestedTopIndex + cols * ListBox->EntireItemsInColumn <=
                            suggestedFocusIndex)
                            break; // focus lezi za panelem, musime najit lepsi TopIndex
                    }

                    // focus je alespon castecne viditelny, potlacime hledani TopIndex
                    findTopIndex = FALSE;
                    break;
                }

                case vmDetailed:
                {
                    if (suggestedFocusIndex < suggestedTopIndex)
                        break; // focus lezi nad panelem, musime najit lepsi TopIndex

                    int rows = (ListBox->FilesRect.bottom - ListBox->FilesRect.top +
                                ListBox->ItemHeight - 1) /
                               ListBox->ItemHeight;
                    if (rows < 1)
                        rows = 1;

                    if (wholeItemVisible)
                    {
                        if (suggestedTopIndex + rows <= suggestedFocusIndex + 1) // nechceme castecnou viditelnost, proto ta +1
                            break;                                               // focus lezi pod panelem, musime najit lepsi TopIndex
                    }
                    else
                    {
                        if (suggestedTopIndex + rows <= suggestedFocusIndex)
                            break; // focus lezi pod panelem, musime najit lepsi TopIndex
                    }

                    // focus je cely viditelny, potlacime hledani TopIndex
                    findTopIndex = FALSE;
                    break;
                }

                case vmIcons:
                case vmThumbnails:
                case vmTiles:
                {
                    int suggestedTop = ListBox->FilesRect.top + (suggestedFocusIndex /
                                                                 ListBox->ColumnsCount) *
                                                                    ListBox->ItemHeight;
                    int suggestedBottom = suggestedTop + ListBox->ItemHeight;

                    if (wholeItemVisible)
                    {
                        if (suggestedTop < suggestedTopIndex)
                            break; // focus lezi nad panelem, musime najit lepsi TopIndex

                        if (suggestedBottom > suggestedTopIndex +
                                                  ListBox->FilesRect.bottom - ListBox->FilesRect.top)
                            break; // focus lezi pod panelem, musime najit lepsi TopIndex
                    }
                    else
                    {
                        if (suggestedBottom <= suggestedTopIndex)
                            break; // focus lezi nad panelem, musime najit lepsi TopIndex

                        if (suggestedTop >= suggestedTopIndex +
                                                ListBox->FilesRect.bottom - ListBox->FilesRect.top)
                            break; // focus lezi pod panelem, musime najit lepsi TopIndex
                    }

                    // focus je alespon castecne viditelny, potlacime hledani TopIndex
                    findTopIndex = FALSE;
                    break;
                }
                }
            }
            else
                findTopIndex = FALSE;
        }
        if (findTopIndex)
        {
            ListBox->SetItemsCount2(Files->Count + Dirs->Count);
            suggestedTopIndex = ListBox->PredictTopIndex(suggestedFocusIndex);
        }
        /*
    // pokud neni doporucen TopIndex nebo je vyzadovana
    // viditelnost focusIndex, napocitam novy TopIndex
    if (suggestedTopIndex == -1 || (ensureFocusIndexVisible &&
                                    (suggestedFocusIndex < suggestedTopIndex ||
                                     ListBox->Mode == vmDetailed &&
                                     suggestedTopIndex + ListBox->EntireItemsInColumn <= suggestedFocusIndex ||
                                     ListBox->Mode != vmDetailed &&
                                     suggestedTopIndex + ((ListBox->FilesRect.right -
                                      ListBox->FilesRect.left) / ListBox->ItemWidth) *
                                      ListBox->EntireItemsInColumn <= suggestedFocusIndex)
                                    ))
    {
      ListBox->ItemsCount = Files->Count + Dirs->Count;
      ListBox->UpdateInternalData();
      suggestedTopIndex = ListBox->PredictTopIndex(suggestedFocusIndex);
    }
*/
    }
    else
    {
        // p.s. patch situace, kdy suggestedTopIndex != -1 a suggestedFocusIndex == -1 (napr. Back v historii
        // panelu do mista, kde puvodne fokusenej soubor v panelu uz neexistuje)
        if (ensureFocusIndexVisible && suggestedTopIndex != -1) // focus ma byt videt
        {
            suggestedTopIndex = -1; // top-index nemuzeme nastavovat (focus by nebyl videt)
        }
    }

    if (suggestedXOffset == -1)
    {
        suggestedXOffset = 0;
        if (ListBox->ViewMode == vmDetailed)
            suggestedXOffset = ListBox->GetXOffset();
    }
    if (suggestedTopIndex == -1)
    {
        suggestedTopIndex = 0;
    }

    ListBox->SetItemsCount(Files->Count + Dirs->Count, suggestedXOffset, suggestedTopIndex, FALSE);
    ListBox->PaintHeaderLine();
    if (ListBox->HVScrollBar != NULL)
        UpdateWindow(ListBox->HVScrollBar);
    if (ListBox->HHScrollBar != NULL)
        UpdateWindow(ListBox->BottomBar.HWindow);
    ListBox->PaintAllItems(NULL, 0);

    IdleRefreshStates = TRUE;                       // pri pristim Idle vynutime kontrolu stavovych promennych
    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                                                    //TRACE_I("refreshlist: end");
}

int CFilesWindow::GetResidualColumnWidth(int nameColWidth)
{
    CALL_STACK_MESSAGE1("CFilesWindow::GetResidualColumnWidth()");
    if (GetViewMode() == vmBrief)
        return 0;

    int colsWidth = 0;
    int colNameWidth = nameColWidth;
    // nascitam sirku zobrazenych sloupcu (mimo sloupce NAME)
    int i;
    for (i = 0; i < Columns.Count; i++)
    {
        CColumn* column = &Columns[i];
        if (column->ID != COLUMN_ID_NAME)
            colsWidth += column->Width;
        else
        {
            if (colNameWidth == 0)
                colNameWidth = column->Width;
        }
    }

    int residual = ListBox->FilesRect.right - ListBox->FilesRect.left;
    residual -= colsWidth;
    if (residual < 0)
        residual = 0;
    if (residual > colNameWidth)
        residual = colNameWidth;
    return residual;
}
