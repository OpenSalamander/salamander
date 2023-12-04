// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "mainwnd.h"
#include "stswnd.h"
#include "dialogs.h"
#include "shellib.h"
#include "pack.h"
#include "drivelst.h"
#include "snooper.h"
#include "zip.h"
#include "shiconov.h"

//
// ****************************************************************************
// CFilesWindow
//

int DeltaForTotalCount(int total)
{
    int delta = total / 10;
    if (delta < 1)
        delta = 1;
    else if (delta > 10000)
        delta = 10000;
    return delta;
}

#ifndef _WIN64

BOOL AddWin64RedirectedDir(const char* path, CFilesArray* dirs, WIN32_FIND_DATA* fileData,
                           int* index, BOOL* dirWithSameNameExists); // je dale v tomto modulu

#endif // _WIN64

#ifndef IO_REPARSE_TAG_FILE_PLACEHOLDER
#define IO_REPARSE_TAG_FILE_PLACEHOLDER (0x80000015L) // winnt
#endif                                                // IO_REPARSE_TAG_FILE_PLACEHOLDER

// prevzal jsem z: http://msdn.microsoft.com/en-us/library/windows/desktop/dn323738%28v=vs.85%29.aspx
BOOL IsFilePlaceholder(WIN32_FIND_DATA const* findData)
{
    return (findData->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
           (findData->dwReserved0 == IO_REPARSE_TAG_FILE_PLACEHOLDER);
}

BOOL CFilesWindow::ReadDirectory(HWND parent, BOOL isRefresh)
{
    CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory()");

    //  TRACE_I("ReadDirectory: begin");

    //  MainWindow->ReleaseMenuNew();  // pro pripad, ze jde o tento adresar
    HiddenDirsFilesReason = 0;
    HiddenDirsCount = HiddenFilesCount = 0;

    CutToClipChanged = FALSE; // cut-to-clip flagy touto operaci zapomeneme

    FocusFirstNewItem = FALSE;
    UseSystemIcons = FALSE;
    UseThumbnails = FALSE;
    Files->DestroyMembers();
    Dirs->DestroyMembers();
    VisibleItemsArray.InvalidateArr();
    VisibleItemsArraySurround.InvalidateArr();
    SelectedCount = 0;
    NeedRefreshAfterIconsReading = FALSE; // ted uz by byl refresh nesmyslny (jestli bude potreba, nastavi se znovu behem nacitani ikon)
    NumberOfItemsInCurDir = 0;
    InactWinOptimizedReading = FALSE;

    // vycisteni icon-cache
    SleepIconCacheThread();
    IconCache->Release();
    EndOfIconReadingTime = GetTickCount() - 10000;
    StopThumbnailLoading = FALSE; // cista icon-cache - konci obdobi, kdy nesla pouzivat data o "thumbnail-loaderech" v icon-cache

    TemporarilySimpleIcons = FALSE;

    if (ColumnsTemplateIsForDisk != Is(ptDisk))
        BuildColumnsTemplate();                      // pri zmene typu panelu je potreba template postavit znovu
    CopyColumnsTemplateToColumns();                  // vytahnu z cache standardni sloupce
    DeleteColumnsWithoutData();                      // vyhazeme sloupce, pro ktere nemame data (zobrazily by se v nich jen prazdne hodnoty)
    GetPluginIconIndex = InternalGetPluginIconIndex; // nastavime standardni callback (jen vraci nulu)

    char fileName[MAX_PATH + 4];

    if (Is(ptDisk))
    {
        // nastavime velikost ikonek pro IconCache
        CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
        IconCache->SetIconSize(iconSize);

        BOOL readThumbnails = (GetViewMode() == vmThumbnails);

        CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory::disk1");
        // vybereme pluginy, ktere umi loadit thumbnaily (pro optimalizaci)
        TIndirectArray<CPluginData> thumbLoaderPlugins(10, 10, dtNoDelete);
        TIndirectArray<CPluginData> foundThumbLoaderPlugins(10, 10, dtNoDelete); // pole pro pluginy, ktere umi loadit thumbnail pro aktualni soubor
        if (readThumbnails)
        {
            if (thumbLoaderPlugins.IsGood())
                Plugins.AddThumbLoaderPlugins(thumbLoaderPlugins);
            if (!thumbLoaderPlugins.IsGood() ||
                !foundThumbLoaderPlugins.IsGood() || // chyba (malo pameti?)
                thumbLoaderPlugins.Count == 0)       // nebo zadne pluginy pro ziskani thumbnailu nemame -> na thumbnaily kasleme
            {
                if (!thumbLoaderPlugins.IsGood())
                    thumbLoaderPlugins.ResetState();
                if (!foundThumbLoaderPlugins.IsGood())
                    foundThumbLoaderPlugins.ResetState();
                readThumbnails = FALSE;
            }
        }
        UseThumbnails = readThumbnails;

        SetCurrentDirectory(GetPath()); // aby to lepe odsypalo ...

#ifndef _WIN64
        BOOL isWindows64BitDir = Windows64Bit && WindowsDirectory[0] != 0 && IsTheSamePath(GetPath(), WindowsDirectory);
#endif // _WIN64

        RefreshDiskFreeSpace(FALSE);

        Files->SetDeleteData(TRUE);
        Dirs->SetDeleteData(TRUE);

        if (WaitForESCReleaseBeforeTestingESC) // pockame na pusteni ESC (aby nedoslo okamzite k preruseni
        {                                      // listovani - tenhle ESC nejspis ukoncil modalni dialog/messagebox)
            WaitForESCRelease();
            WaitForESCReleaseBeforeTestingESC = FALSE; // dalsi cekani nema smysl
        }

        GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help

        GetRootPath(fileName, GetPath());
        BOOL isRootPath = (strlen(GetPath()) <= strlen(fileName));

        //--- zjisteni typu drivu (sitove disky nebudeme obtezovat zjistovanim sharu)
        UINT drvType = MyGetDriveType(GetPath());
        BOOL testShares = drvType != DRIVE_REMOTE;
        if (testShares)
            Shares.PrepareSearch(GetPath());
        switch (drvType)
        {
        case DRIVE_REMOVABLE:
        {
            BOOL isDriveFloppy = FALSE; // floppy maji svuji konfiguraci vedle ostatnich removable drivu
            int drv = UpperCase[fileName[0]] - 'A' + 1;
            if (drv >= 1 && drv <= 26) // pro jistotu provedeme "range-check"
            {
                DWORD medium = GetDriveFormFactor(drv);
                if (medium == 350 || medium == 525 || medium == 800 || medium == 1)
                    isDriveFloppy = TRUE;
            }
            UseSystemIcons = isDriveFloppy ? !Configuration.DrvSpecFloppySimple : !Configuration.DrvSpecRemovableSimple;
            break;
        }

        case DRIVE_REMOTE:
        {
            UseSystemIcons = !Configuration.DrvSpecRemoteSimple;
            break;
        }

        case DRIVE_CDROM:
        {
            UseSystemIcons = !Configuration.DrvSpecCDROMSimple;
            break;
        }

        default: // case DRIVE_FIXED:   // nejen fixed, ale i ty ostatni (RAM DISK, atd.)
        {
            UseSystemIcons = !Configuration.DrvSpecFixedSimple;
            break;
        }
        }

        const char* s = GetPath();
        char* st = fileName;
        while (*s != 0)
            *st++ = *s++;
        if (s == GetPath())
        {
            SetCurrentDirectoryToSystem();
            DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
            //      TRACE_I("ReadDirectory: end");
            return FALSE; // prazdny string na vstupu
        }
        if (*(st - 1) != '\\')
            *st++ = '\\';
        strcpy(st, "*");
        char* fileNameEnd = st;
        //--- priprava pro nacitani ikon
        if (UseSystemIcons)
        {
            IconCacheValid = FALSE;
            MSG msg; // musime zlikvidovat pripadnou WM_USER_ICONREADING_END, ktera by nastavila IconCacheValid = TRUE
            while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                ;

            int i;
            for (i = 0; i < Associations.Count; i++)
            {
                if (Associations[i].GetIndex(iconSize) == -3)
                    Associations[i].SetIndex(-1, iconSize); // shozeni flagu "nacitana ikona"
            }
        }
        else
        {
            if (UseThumbnails)
            {
                IconCacheValid = FALSE;
                MSG msg; // musime zlikvidovat pripadnou WM_USER_ICONREADING_END, ktera by nastavila IconCacheValid = TRUE
                while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                    ;
            }
        }
        //--- nacteni obsahu adresare
        BOOL upDir;
        BOOL UNCRootUpDir = FALSE;
        if (GetPath()[0] == '\\' && GetPath()[1] == '\\')
        {
            if (GetPath()[2] == '.' && GetPath()[3] == '\\' && GetPath()[4] != 0 && GetPath()[5] == ':') // cesta typu "\\.\C:\"
            {
                upDir = strlen(GetPath()) > 7;
            }
            else // UNC cesta
            {
                const char* s2 = GetPath() + 2;
                while (*s2 != 0 && *s2 != '\\')
                    s2++;
                if (*s2 != 0)
                    s2++;
                while (*s2 != 0 && *s2 != '\\')
                    s2++;
                upDir = (*s2 == '\\' && *(s2 + 1) != 0);
                if (!upDir && Plugins.GetFirstNethoodPluginFSName())
                {
                    upDir = TRUE;
                    UNCRootUpDir = TRUE;
                }
            }
        }
        else
            upDir = strlen(GetPath()) > 3;

        CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory::disk2");

        CIconData iconData;
        iconData.FSFileData = NULL;
        iconData.SetReadingDone(0); // jen tak pro formu
        BOOL addtoIconCache;
        CFileData file;
        // inicializace clenu struktury, ktere uz dale nebudeme menit
        file.PluginData = -1; // -1 jen tak, ignoruje se
        file.Selected = 0;
        file.SizeValid = 0;
        file.Dirty = 0; // zbytecne, jen pro formu
        file.CutToClip = 0;
        file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
        file.IconOverlayDone = 0;
        int len;
#ifndef _WIN64
        int foundWin64RedirectedDirs = 0;
        BOOL isWin64RedirectedDir = FALSE;
#endif // _WIN64

    _TRY_AGAIN:

        // po 2000 ms zobrazime okenko s prerusovaci napovedou
        char buf[2 * MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_READINGPATHESC), GetPath());
        CreateSafeWaitWindow(buf, NULL, 2000, TRUE, MainWindow->HWindow);

        DWORD lastEscCheckTime;
        //lastEscCheckTime = GetTickCount() - 200;  // prvni ESC pujde hned
        lastEscCheckTime = GetTickCount(); // prvni ESC pujde az po 200ms -- jde o ochranu
        // pred nabidkou na cancel listingu po te, co uzivatel Escapem zavrel napriklad Files/Security/* dialogy
        // a v panelu mel sitovy disk (a behem otevreneho dialogu se prepnul ze Salamandera a zpet, takze doslo
        // k refresh directory)

        BOOL isUpDir = FALSE;
        WIN32_FIND_DATA fileData;
        HANDLE search;
        search = HANDLES_Q(FindFirstFile(fileName, &fileData));
        if (search == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            DestroySafeWaitWindow();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_NO_MORE_FILES)
            {
                if (!upDir)
                {
                    StatusLine->SetText(LoadStr(IDS_NOFILESFOUND));
                    SetCurrentDirectoryToSystem();
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    if (UseSystemIcons || UseThumbnails) // sice zadne ikony nemame, ale nacitani musime pustit (uz kvuli nahozeni IconCacheValid = TRUE)
                    {
                        if (IconCache->Count > 1)
                            IconCache->SortArray(0, IconCache->Count - 1, NULL);
                        WakeupIconCacheThread(); // zacni nacitat ikony
                    }
                    //          TRACE_I("ReadDirectory: end");
                    return TRUE;
                }
            }
            else
            {
                SetCurrentDirectoryToSystem();
                RefreshListBox(0, -1, -1, FALSE, FALSE);
                DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                DirectoryLine->InvalidateIfNeeded();
                IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
                StatusLine->SetText("");
                UpdateWindow(HWindow);

                BOOL showErr = TRUE;
                if (err == ERROR_INVALID_PARAMETER || err == ERROR_NOT_READY)
                {
                    DWORD attrs = SalGetFileAttributes(GetPath());
                    if (attrs != INVALID_FILE_ATTRIBUTES &&
                        (attrs & FILE_ATTRIBUTE_DIRECTORY) &&
                        (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
                    {
                        showErr = FALSE;
                        char drive[MAX_PATH];
                        UINT drvType2;
                        if (GetPath()[0] == '\\' && GetPath()[1] == '\\')
                        {
                            drvType2 = DRIVE_REMOTE;
                            GetRootPath(drive, GetPath());
                            drive[strlen(drive) - 1] = 0; // nestojime o posledni '\\'
                        }
                        else
                        {
                            drive[0] = GetPath()[0];
                            drive[1] = 0;
                            drvType2 = MyGetDriveType(GetPath());
                        }
                        if (drvType2 != DRIVE_REMOTE)
                        {
                            GetCurrentLocalReparsePoint(GetPath(), CheckPathRootWithRetryMsgBox);
                            if (strlen(CheckPathRootWithRetryMsgBox) > 3)
                            {
                                lstrcpyn(drive, CheckPathRootWithRetryMsgBox, MAX_PATH);
                                SalPathRemoveBackslash(drive);
                            }
                        }
                        else
                            GetRootPath(CheckPathRootWithRetryMsgBox, GetPath());
                        sprintf(buf, LoadStr(IDS_NODISKINDRIVE), drive);
                        int msgboxRes = (int)CDriveSelectErrDlg(parent, buf, GetPath()).Execute();
                        CheckPathRootWithRetryMsgBox[0] = 0;
                        UpdateWindow(MainWindow->HWindow);
                        if (msgboxRes == IDRETRY)
                            goto _TRY_AGAIN;
                    }
                }
                if (isRefresh &&
                    (err == ERROR_ACCESS_DENIED || err == ERROR_PATH_NOT_FOUND ||
                     err == ERROR_BAD_PATHNAME || err == ERROR_FILE_NOT_FOUND))
                { // pri smazani cesty zobrazene v panelu se ukazuji tyhle chyby, coz nechceme, proste jen tise zkratime cestu na prvni existujici (bohuzel se to nechyti drive, protoze cesta existuje jeste nejakou dobu po svem smazani, proste to se zase neco ve woknech nepovedlo)
                    //          TRACE_I("ReadDirectory(): silently ignoring FindFirstFile failure: " << GetErrorText(err));
                    showErr = FALSE;
                }
                if (showErr)
                    SalMessageBox(parent, GetErrorText(err), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                //        TRACE_I("ReadDirectory: end");
                return FALSE;
            }
        }
        else
        {
            BOOL testFindNextErr;
            testFindNextErr = TRUE;
            do
            {
                NumberOfItemsInCurDir++;

                // test ESC - nechce to cteni user prerusit ?
                if (GetTickCount() - lastEscCheckTime >= 200) // 5x za sekundu
                {
                    if (UserWantsToCancelSafeWaitWindow())
                    {
                        MSG msg; // vyhodime nabufferovany ESC
                        while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                            ;

                        SetCurrentDirectoryToSystem();
                        RefreshListBox(0, -1, -1, FALSE, FALSE);

                        int resBut = SalMessageBox(parent, LoadStr(IDS_READDIRTERMINATED), LoadStr(IDS_QUESTION),
                                                   MB_YESNOCANCEL | MB_ICONQUESTION);
                        UpdateWindow(MainWindow->HWindow);

                        WaitForESCRelease();
                        WaitForESCReleaseBeforeTestingESC = FALSE; // dalsi cekani nema smysl
                        GetAsyncKeyState(VK_ESCAPE);               // novy init GetAsyncKeyState - viz help

                        if (resBut == IDYES)
                        {
                            testFindNextErr = FALSE;
                            break; // ukoncime nacitani
                        }
                        else
                        {
                            if (resBut == IDNO)
                            {
                                if (GetMonitorChanges()) // musime potlacit monitorovani zmen (autorefresh)
                                {
                                    DetachDirectory((CFilesWindow*)this);
                                    SetMonitorChanges(FALSE); // dale uz se zmeny monitorovat nebudou
                                }

                                SetSuppressAutoRefresh(TRUE);
                            }
                        }
                    }
                    lastEscCheckTime = GetTickCount();
                }

                st = fileData.cFileName;
                len = (int)strlen(st);
                BOOL isDir;
                isDir = (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                isUpDir = (len == 2 && *st == '.' && *(st + 1) == '.');
                //--- osetreni '.', '..' a skrytych/systemovych souboru (soubor "." se neignoruje, FLAME spyware tyhle soubory pouziva, tak at jsou videt)
                if (len == 0 || len == 1 && *st == '.' && isDir ||
                    ((isRootPath || !isDir ||
                      CQuadWord(fileData.ftLastWriteTime.dwLowDateTime, // datum na ".." je starsi nebo roven 1.1.1980, radsi ho nacteme pozdeji "poradne"
                                fileData.ftLastWriteTime.dwHighDateTime) <= CQuadWord(2148603904, 27846551)) &&
                     isUpDir))
                    continue;

                if (Configuration.NotHiddenSystemFiles &&
                    !IsFilePlaceholder(&fileData) && // placeholder je hidden, ale Explorer ho ukazuje normalne, budeme ho taky ukazovat
                    (fileData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) &&
                    (len != 2 || *st != '.' || *(st + 1) != '.'))
                { // skipnuti hidden souboru/adresare
                    if (isDir)
                        HiddenDirsCount++;
                    else
                        HiddenFilesCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                    continue;
                }
                //--- na soubory se aplikuje filter
                if (FilterEnabled && !isDir)
                {
                    const char* ext = fileData.cFileName + len;
                    while (--ext >= fileData.cFileName && *ext != '.')
                        ;
                    if (ext < fileData.cFileName)
                        ext = fileData.cFileName + len; // ".cvspass" ve Windows je pripona ...
                    else
                        ext++;
                    if (!Filter.AgreeMasks(fileData.cFileName, ext))
                    {
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_FILTER;
                        continue;
                    }
                }

                //--- pokud je jmeno obsazeno v poli HiddenNames, zahodime ho
                if (HiddenNames.Contains(isDir, fileData.cFileName))
                {
                    if (isDir)
                        HiddenDirsCount++;
                    else
                        HiddenFilesCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                    continue;
                }

            ADD_ITEM: // pri pridani ".."

                //--- jmeno
                file.Name = (char*)malloc(len + 1); // alokace
                if (file.Name == NULL)
                {
                    if (search != NULL)
                    {
                        DestroySafeWaitWindow();
                        HANDLES(FindClose(search));
                    }
                    TRACE_E(LOW_MEMORY);
                    SetCurrentDirectoryToSystem();
                    Files->DestroyMembers();
                    Dirs->DestroyMembers();
                    VisibleItemsArray.InvalidateArr();
                    VisibleItemsArraySurround.InvalidateArr();
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    //          TRACE_I("ReadDirectory: end");
                    return FALSE;
                }
                memmove(file.Name, st, len + 1); // kopie textu
                file.NameLen = len;
                //--- pripona
                if (!Configuration.SortDirsByExt && (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // jde o ptDisk
                {
                    file.Ext = file.Name + file.NameLen; // adresare nemaji pripony
                }
                else
                {
                    s = st + len;
                    while (--s >= st && *s != '.')
                        ;
                    if (s >= st)
                        file.Ext = file.Name + (s - st + 1); // ".cvspass" ve Windows je pripona ...
                                                             //          if (s > st) file.Ext = file.Name + (s - st + 1);
                    else
                        file.Ext = file.Name + file.NameLen;
                }
                //--- ostatni
                file.Size = CQuadWord(fileData.nFileSizeLow, fileData.nFileSizeHigh);
                file.Attr = fileData.dwFileAttributes;
                file.LastWrite = fileData.ftLastWriteTime;
                // placeholder je hidden, ale Explorer ho ukazuje normalne, budeme ho taky ukazovat normalne (bez ghosted ikony)
                file.Hidden = (file.Attr & FILE_ATTRIBUTE_HIDDEN) && !IsFilePlaceholder(&fileData) ? 1 : 0;

                file.IsOffline = !isUpDir && (file.Attr & FILE_ATTRIBUTE_OFFLINE) ? 1 : 0;
                if (testShares && (file.Attr & FILE_ATTRIBUTE_DIRECTORY)) // jde o ptDisk
                {
                    file.Shared = Shares.Search(file.Name);
                }
                else
                    file.Shared = 0;

                if (fileData.cAlternateFileName[0] != 0)
                {
                    int l = (int)strlen(fileData.cAlternateFileName) + 1;
                    file.DosName = (char*)malloc(l);
                    if (file.DosName == NULL)
                    {
                        free(file.Name);
                        if (search != NULL)
                        {
                            DestroySafeWaitWindow();
                            HANDLES(FindClose(search));
                        }
                        TRACE_E(LOW_MEMORY);
                        SetCurrentDirectoryToSystem();
                        Files->DestroyMembers();
                        Dirs->DestroyMembers();
                        VisibleItemsArray.InvalidateArr();
                        VisibleItemsArraySurround.InvalidateArr();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                    memmove(file.DosName, fileData.cAlternateFileName, l);
                }
                else
                    file.DosName = NULL;
                if (file.Attr & FILE_ATTRIBUTE_DIRECTORY) // jde o ptDisk
                {
                    file.Association = 0;
                    file.Archive = 0;
#ifndef _WIN64
                    file.IsLink = (isWin64RedirectedDir || (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || // POZOR: pseudo-directory musi mit nahozeny IsLink, jinak je nutne predelat ContainsWin64RedirectedDir
                                   isWindows64BitDir && file.NameLen == 8 && StrICmp(file.Name, "system32") == 0)
                                      ? 1
                                      : 0; // adresar system32 v 32-bitovem Salamovi je link do adresare SysWOW64 + win64 redirected-dir + volume mount point nebo junction point = zobrazime adresar s link overlayem
#else                                      // _WIN64
                    file.IsLink = (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; // volume mount point nebo junction point = zobrazime adresar s link overlayem
#endif                                     // _WIN64
                    if (len == 2 && *st == '.' && *(st + 1) == '.')
                    { // osetreni ".."
                        if (GetPath()[3] != 0)
                            Dirs->Insert(0, file); // krom korene ...
                        else
                        {
                            if (file.Name != NULL)
                                free(file.Name);
                            if (file.DosName != NULL)
                                free(file.DosName);
                        }
                        addtoIconCache = FALSE;
                    }
                    else
                    {
                        Dirs->Add(file);
#ifndef _WIN64
                        addtoIconCache = isWin64RedirectedDir ? FALSE : TRUE;
#else  // _WIN64
                        addtoIconCache = TRUE;
#endif // _WIN64
                    }
                    if (!Dirs->IsGood())
                    {
                        Dirs->ResetState();
                        if (search != NULL)
                        {
                            DestroySafeWaitWindow();
                            HANDLES(FindClose(search));
                        }
                        SetCurrentDirectoryToSystem();
                        Files->DestroyMembers();
                        Dirs->DestroyMembers();
                        VisibleItemsArray.InvalidateArr();
                        VisibleItemsArraySurround.InvalidateArr();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                }
                else
                {
                    if (s >= st) // existuje pripona
                    {
                        while (*++s != 0)
                            *st++ = LowerCase[*s];
                        *(DWORD*)st = 0;         // nuly na konec
                        st = fileData.cFileName; // pripona malymi pismeny

                        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                            file.IsLink = 1; // pokud je soubor reparse-point (mozna vubec neni mozne) = zobrazime ho s link overlayem
                        else
                        {
                            file.IsLink = (*(DWORD*)st == *(DWORD*)"lnk" ||
                                           *(DWORD*)st == *(DWORD*)"pif" ||
                                           *(DWORD*)st == *(DWORD*)"url")
                                              ? 1
                                              : 0;
                        }

                        if (PackerFormatConfig.PackIsArchive(file.Name, file.NameLen)) // je to archiv, ktery umime zpracovat?
                        {
                            file.Association = 1;
                            file.Archive = 1;
                            addtoIconCache = FALSE;
                        }
                        else
                        {
                            file.Association = Associations.IsAssociated(st, addtoIconCache, iconSize);
                            file.Archive = 0;
                            if (*(DWORD*)st == *(DWORD*)"scr" || // par vyjimek
                                *(DWORD*)st == *(DWORD*)"pif")
                            {
                                addtoIconCache = TRUE;
                            }
                            else
                            {
                                if (*(DWORD*)st == *(DWORD*)"lnk") // ikonky pres link
                                {
                                    strcpy(fileData.cFileName, file.Name);
                                    char* ext2 = strrchr(fileData.cFileName, '.');
                                    if (ext2 != NULL) // ".cvspass" ve Windows je pripona ...
                                                      //                  if (ext2 != NULL && ext2 != fileData.cFileName)
                                    {
                                        *ext2 = 0;
                                        if (PackerFormatConfig.PackIsArchive(fileData.cFileName)) // je to link na archiv, ktery umime zpracovat?
                                        {
                                            file.Association = 1;
                                            file.Archive = 1;
                                            addtoIconCache = FALSE;
                                        }
                                    }
                                    addtoIconCache = TRUE;
                                }
                            }
                        }
                    }
                    else
                    {
                        file.Association = 0;
                        file.Archive = 0;
                        file.IsLink = (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; // pokud je soubor reparse-point (mozna vubec neni mozne) = zobrazime ho s link overlayem
                        addtoIconCache = FALSE;
                    }

                    Files->Add(file);
                    if (!Files->IsGood())
                    {
                        Files->ResetState();
                        if (search != NULL)
                        {
                            DestroySafeWaitWindow();
                            HANDLES(FindClose(search));
                        }
                        SetCurrentDirectoryToSystem();
                        Files->DestroyMembers();
                        Dirs->DestroyMembers();
                        VisibleItemsArray.InvalidateArr();
                        VisibleItemsArraySurround.InvalidateArr();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                }

                // u souboru zkontrolujeme, jestli neni potreba nacist thumbnail
                if (readThumbnails &&                              // maji se nacitat thumbnaily
                    (file.Attr & FILE_ATTRIBUTE_DIRECTORY) == 0 && // (jde o ptDisk, proto je pouziti FILE_ATTRIBUTE_DIRECTORY je o.k.)
                    file.Archive == 0)                             // ikona archivu ma prednost pred thumbnailem
                {
                    foundThumbLoaderPlugins.DestroyMembers();
                    int i;
                    for (i = 0; i < thumbLoaderPlugins.Count; i++)
                    {
                        CPluginData* p = thumbLoaderPlugins[i];
                        if (p->ThumbnailMasks.AgreeMasks(file.Name, file.Ext) &&
                            !p->ThumbnailMasksDisabled) // neprobiha jeho unload/remove
                        {
                            if (!p->GetLoaded()) // plugin je potreba naloadit (mozna zmena masky pro "thumbnail loader")
                            {
                                //                RefreshListBox(0, -1, -1, FALSE, FALSE);  // nahrazeno pres ListBox->SetItemsCount + WM_USER_UPDATEPANEL, protoze to blikalo napr. pri pridani prvniho souboru *.doc do adresare s obrazky (dojde k loadu Eroiicy (pro thumbnail *.doc))

                                // muze dojit k zobrazeni napr. dialogu "PictView is not registered" -> v tom pripade je nutny
                                // refresh listboxu (jinak zadny refresh nedelame, aby to neblikalo s panelem)
                                // zabezpecime listbox proti chybam vzniklym zadosti o prekresleni (data se prave ctou z disku)
                                ListBox->SetItemsCount(0, 0, 0, TRUE); // TRUE - zakazeme nastaveni scrollbar
                                // Pokud se doruci WM_USER_UPDATEPANEL, dojde k prekresleni obsahu panelu a nastaveni
                                // scrollbary. Dorucit ji muze message loopa pri vytvoreni message boxu (nebo dialogu).
                                // Jinak se panel bude tvarit jako nezmeneny a message bude vyjmuta z fronty.
                                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);

                                BOOL cont = FALSE;
                                if (p->InitDLL(HWindow, FALSE, TRUE, FALSE) &&  // uspesny load pluginu
                                    p->ThumbnailMasks.GetMasksString()[0] != 0) // plugin stale jeste je "thumbnail loader"
                                {
                                    if (!p->ThumbnailMasks.AgreeMasks(file.Name, file.Ext) || // uz neumi thumbnaily pro tento soubor
                                        p->ThumbnailMasksDisabled)                            // probiha jeho unload/remove
                                    {
                                        cont = TRUE;
                                    }
                                }
                                else // nelze naloadit -> vyhodime ho z pole zkousenych pluginu (opatreni proti opakovani chybovych hlasek)
                                {
                                    TRACE_I("Unable to use plugin " << p->Name << " as thumbnail loader.");
                                    thumbLoaderPlugins.Delete(i);
                                    readThumbnails = thumbLoaderPlugins.Count > 0;
                                    if (!thumbLoaderPlugins.IsGood())
                                        thumbLoaderPlugins.ResetState();
                                    else
                                        i--;
                                    cont = TRUE; // zkusime stesti s dalsim pluginem
                                }

                                // vycistime message-queue od nabufferovane WM_USER_UPDATEPANEL
                                MSG msg2;
                                PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);

                                if (cont)
                                    continue;
                            }

                            foundThumbLoaderPlugins.Add(p);
                        }
                    }
                    if (foundThumbLoaderPlugins.IsGood())
                    {
                        if (foundThumbLoaderPlugins.Count > 0)
                        {
                            int size = len + 4;
                            size -= (size & 0x3); // size % 4  (zarovnani po ctyrech bytech)
                            int nameSize = size;
                            size += sizeof(CQuadWord) + sizeof(FILETIME);
                            size += (foundThumbLoaderPlugins.Count + 1) * sizeof(void*); // misto pro ukazatele na rozhrani pluginu + NULL na konci
                            iconData.NameAndData = (char*)malloc(size);
                            if (iconData.NameAndData != NULL)
                            {
                                memcpy(iconData.NameAndData, file.Name, len);
                                memset(iconData.NameAndData + len, 0, nameSize - len); // konec jmena vynulujem
                                // pridame velikost + cas posl. zapisu do souboru
                                *(CQuadWord*)(iconData.NameAndData + nameSize) = file.Size;
                                *(FILETIME*)(iconData.NameAndData + nameSize + sizeof(CQuadWord)) = file.LastWrite;
                                // pridame seznam ukazatelu na zapouzdreni rozhrani pluginu pro ziskani thumbnailu
                                void** ifaces = (void**)(iconData.NameAndData + nameSize + sizeof(CQuadWord) + sizeof(FILETIME));
                                int i2;
                                for (i2 = 0; i2 < foundThumbLoaderPlugins.Count; i2++)
                                {
                                    *ifaces++ = foundThumbLoaderPlugins[i2]->GetPluginInterfaceForThumbLoader();
                                }
                                *ifaces = NULL;      // ukonceni seznamu rozhrani pluginu
                                iconData.SetFlag(4); // zatim nenacteny thumbnail

                                // musime naalokovat misto pro thumbnail, v threadu to nejde
                                iconData.SetIndex(IconCache->AllocThumbnail());

                                if (iconData.GetIndex() != -1)
                                {
                                    IconCache->Add(iconData);
                                    if (!IconCache->IsGood())
                                    {
                                        free(iconData.NameAndData);
                                        IconCache->ResetState();
                                    }
                                    else
                                        addtoIconCache = FALSE; // je to thumbnail, nemuze to byt zaroven i ikona
                                }
                                else
                                    free(iconData.NameAndData);
                            }
                        }
                    }
                    else
                        foundThumbLoaderPlugins.ResetState();
                }

                // pridani adresare do IconCache -> je treba nacist ikonku
                if (UseSystemIcons && addtoIconCache)
                {
                    int size = len + 4;
                    size -= (size & 0x3); // size % 4  (zarovnani po ctyrech bytech)
                    iconData.NameAndData = (char*)malloc(size);
                    if (iconData.NameAndData != NULL)
                    {
                        memmove(iconData.NameAndData, file.Name, len);
                        memset(iconData.NameAndData + len, 0, size - len); // konec vynulujem
                        iconData.SetFlag(0);                               // zatim nenactena ikona
                                                                           // musime naalokovat misto pro bitmapky, v threadu to nejde
                        iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                        if (iconData.GetIndex() != -1)
                        {
                            IconCache->Add(iconData);
                            if (!IconCache->IsGood())
                            {
                                free(iconData.NameAndData);
                                IconCache->ResetState();
                            }
                        }
                        else
                            free(iconData.NameAndData);
                    }
                }
                if (search == NULL)
                {
                    testFindNextErr = FALSE;
#ifndef _WIN64
                    isWin64RedirectedDir = FALSE;
#endif                     // _WIN64
                    break; // druhy pruchod (pridani ".." nebo win64 redirected-diru)
                }
            } while (FindNextFile(search, &fileData));
            DWORD err = GetLastError();

            if (search != NULL) // prvni pruchod
            {
                DestroySafeWaitWindow();
                HANDLES(FindClose(search));
            }

            if (testFindNextErr && err != ERROR_NO_MORE_FILES)
            {
                SetCurrentDirectoryToSystem();
                RefreshListBox(0, -1, -1, FALSE, FALSE);

                sprintf(buf, LoadStr(IDS_CANNOTREADDIR), GetPath(), GetErrorText(err));
                SalMessageBox(parent, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
        }

        if (upDir && (Dirs->Count == 0 || strcmp(Dirs->At(0).Name, "..") != 0))
        {
            upDir = FALSE;
            *(fileNameEnd - 1) = 0; // neni to logicky, ale casy ".." jsou od akt. adresare
            if (!UNCRootUpDir)
                search = HANDLES_Q(FindFirstFile(fileName, &fileData));
            else
                search = INVALID_HANDLE_VALUE;
            if (search == INVALID_HANDLE_VALUE)
            {
                fileData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY; // jde o ptDisk
                SYSTEMTIME ltNone;
                ltNone.wYear = 1602;
                ltNone.wMonth = 1;
                ltNone.wDay = 1;
                ltNone.wDayOfWeek = 2;
                ltNone.wHour = 0;
                ltNone.wMinute = 0;
                ltNone.wSecond = 0;
                ltNone.wMilliseconds = 0;
                FILETIME ft;
                SystemTimeToFileTime(&ltNone, &ft);
                LocalFileTimeToFileTime(&ft, &fileData.ftCreationTime);
                LocalFileTimeToFileTime(&ft, &fileData.ftLastAccessTime);
                LocalFileTimeToFileTime(&ft, &fileData.ftLastWriteTime);

                fileData.nFileSizeHigh = 0;
                fileData.nFileSizeLow = 0;
                fileData.dwReserved0 = fileData.dwReserved1 = 0;
            }
            else
                HANDLES(FindClose(search));
            search = NULL;                                              // druhy/treti pruchod ...
            fileData.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;      // jde o ptDisk
            fileData.dwFileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT; // musime odstranit flag FILE_ATTRIBUTE_REPARSE_POINT, jinak bude link overlay na ".."
            strcpy(fileData.cFileName, "..");
            fileData.cAlternateFileName[0] = 0;
            st = fileData.cFileName;
            len = (int)strlen(st);
            isUpDir = TRUE;
            goto ADD_ITEM;
        }

#ifndef _WIN64

    FIND_NEXT_WIN64_REDIRECTEDDIR:

        BOOL dirWithSameNameExists;
        if (foundWin64RedirectedDirs < 10 &&
            AddWin64RedirectedDir(GetPath(), Dirs, &fileData, &foundWin64RedirectedDirs, &dirWithSameNameExists))
        {
            foundWin64RedirectedDirs++; // napr. pod system32 jich muze byt 5, pridal jsem jistou rezervu do deseti...

            if (Configuration.NotHiddenSystemFiles &&
                (fileData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
            { // skipnuti hidden adresare
                if (!dirWithSameNameExists)
                {
                    HiddenDirsCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                }
                goto FIND_NEXT_WIN64_REDIRECTEDDIR;
            }

            //--- pokud je jmeno obsazeno v poli HiddenNames, zahodime ho
            if (HiddenNames.Contains(TRUE, fileData.cFileName))
            {
                if (!dirWithSameNameExists)
                {
                    HiddenDirsCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                }
                goto FIND_NEXT_WIN64_REDIRECTEDDIR;
            }

            isWin64RedirectedDir = TRUE;
            search = NULL; // druhy/treti pruchod ...
            st = fileData.cFileName;
            len = (int)strlen(st);
            isUpDir = FALSE;
            goto ADD_ITEM;
        }
        if (foundWin64RedirectedDirs >= 10)
            TRACE_E("CFilesWindow::ReadDirectory(): foundWin64RedirectedDirs >= 10 (there are more redirected-dirs?)");

#endif // _WIN64

        SetCurrentDirectoryToSystem();

        if (Files->Count + Dirs->Count == 0)
            StatusLine->SetText(LoadStr(IDS_NOFILESFOUND));

        // seradime Files a Dirs podle akt. zpusobu razeni
        SortDirectory();

        if (UseSystemIcons || UseThumbnails)
        {
            if (IconCache->Count > 1)
                IconCache->SortArray(0, IconCache->Count - 1, NULL);
            WakeupIconCacheThread(); // zacni nacitat ikony
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            RefreshDiskFreeSpace();

            if (PluginData.NotEmpty())
            {
                CSalamanderView view(this);
                PluginData.SetupView(this == MainWindow->LeftPanel, &view, GetZIPPath(),
                                     GetArchiveDir()->GetUpperDir(GetZIPPath()));
            }

            // nastavime velikost ikonek pro IconCache
            CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
            IconCache->SetIconSize(iconSize);

            CFilesArray* ZIPFiles = GetArchiveDirFiles();
            CFilesArray* ZIPDirs = GetArchiveDirDirs();

            Files->SetDeleteData(FALSE); // jen melke kopie dat
            Dirs->SetDeleteData(FALSE);  // jen melke kopie dat

            if (ZIPFiles != NULL && ZIPDirs != NULL)
            {
                // viz komentar v pripade ptPluginFS
                Files->SetDelta(DeltaForTotalCount(ZIPFiles->Count));
                Dirs->SetDelta(DeltaForTotalCount(ZIPDirs->Count));

                int i;
                for (i = 0; i < ZIPFiles->Count; i++)
                {
                    CFileData* f = &ZIPFiles->At(i);
                    if (Configuration.NotHiddenSystemFiles &&
                        (f->Hidden || // Hidden i Attr jsou nulovane pokud jsou neplatne -> testy failnou
                         (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                    { // skipnuti hidden souboru/adresare
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                        continue;
                    }

                    if (FilterEnabled && (!Filter.AgreeMasks(f->Name, f->Ext)))
                    {
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_FILTER;
                        continue;
                    }

                    //--- pokud je jmeno obsazeno v poli HiddenNames, zahodime ho
                    if (HiddenNames.Contains(FALSE, f->Name))
                    {
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                        continue;
                    }

                    Files->Add(*f);
                }
                CFileData upDir;
                static char buffUp[] = "..";
                upDir.Name = buffUp; // nebude se volat free(), muzeme si dovolit ".."
                upDir.Ext = upDir.Name + 2;
                upDir.Size = CQuadWord(0, 0);
                upDir.Attr = 0;
                upDir.LastWrite = GetZIPArchiveDate();
                upDir.DosName = NULL;
                upDir.PluginData = 0; // 0 jen tak, plug-in si prepise na svou hodnotu
                upDir.NameLen = 2;
                upDir.Hidden = 0;
                upDir.IsLink = 0;
                upDir.IsOffline = 0;
                upDir.Association = 0;
                upDir.Selected = 0;
                upDir.Shared = 0;
                upDir.Archive = 0;
                upDir.SizeValid = 0;
                upDir.Dirty = 0; // zbytecne, jen pro formu
                upDir.CutToClip = 0;
                upDir.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
                upDir.IconOverlayDone = 0;

                if (PluginData.NotEmpty())
                    PluginData.GetFileDataForUpDir(GetZIPPath(), upDir);

                Dirs->Add(upDir);
                for (i = 0; i < ZIPDirs->Count; i++)
                {
                    CFileData* f = &ZIPDirs->At(i);
                    if (Configuration.NotHiddenSystemFiles &&
                        (f->Hidden || // Hidden i Attr jsou nulovane pokud jsou neplatne -> testy failnou
                         (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                    { // skipnuti hidden souboru/adresare
                        HiddenDirsCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                        continue;
                    }

                    //--- pokud je jmeno obsazeno v poli HiddenNames, zahodime ho
                    if (HiddenNames.Contains(TRUE, f->Name))
                    {
                        HiddenDirsCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                        continue;
                    }

                    Dirs->Add(*f);
                }
                if (!Files->IsGood() || !Dirs->IsGood())
                {
                    if (!Files->IsGood())
                        Files->ResetState();
                    if (!Dirs->IsGood())
                        Dirs->ResetState();
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    //          TRACE_I("ReadDirectory: end");
                    return FALSE;
                }
            }
            else
            {
                DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                //        TRACE_I("ReadDirectory: end");
                return FALSE; // adresar prestal existovat ...
            }

            // seradime Files a Dirs podle akt. zpusobu razeni
            SortDirectory();

            UseSystemIcons = !Configuration.UseSimpleIconsInArchives;
            if (UseSystemIcons)
            {
                // priprava pro nacitani ikon
                IconCacheValid = FALSE;
                MSG msg; // musime zlikvidovat pripadnou WM_USER_ICONREADING_END, ktera by nastavila IconCacheValid = TRUE
                while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                    ;

                int i;
                for (i = 0; i < Associations.Count; i++)
                {
                    if (Associations[i].GetIndex(iconSize) == -3)
                        Associations[i].SetIndex(-1, iconSize);
                }

                // ziskani potrebnych statickych ikon (vypakovavat nic nebudeme)
                for (i = 0; i < Files->Count; i++)
                {
                    const char* iconLocation = NULL;
                    CFileData* f = &Files->At(i);

                    if (*f->Ext != 0) // existuje pripona
                    {
                        /*
            if (PackerFormatConfig.PackIsArchive(f->Name))   // je to archiv, ktery umime zpracovat?
            {
              f->Association = TRUE;
              f->Archive = TRUE;
            }
            else
            {
*/
                        char* s = f->Ext - 1;
                        char* st = fileName;
                        while (*++s != 0)
                            *st++ = LowerCase[*s];
                        *(DWORD*)st = 0; // nuly na konec
                        st = fileName;   // pripona malymi pismeny

                        f->Association = Associations.IsAssociatedStatic(st, iconLocation, iconSize);
                        f->Archive = FALSE;
                        /*
            }
*/
                    }
                    else
                    {
                        f->Association = FALSE;
                        f->Archive = FALSE;
                    }

                    if (iconLocation != NULL)
                    {
                        CIconData iconData;
                        iconData.FSFileData = NULL;
                        iconData.SetReadingDone(0); // jen tak pro formu
                        int size = (int)strlen(iconLocation) + 4;
                        size -= (size & 0x3);                // size % 4  (zarovnani po ctyrech bytech)
                        const char* s = iconLocation + size; // preskok zarovnani z nul
                        int len = (int)strlen(s);
                        if (len > 0) // icon-location neni prazdna
                        {
                            int nameLen = f->NameLen + 4;
                            nameLen -= (nameLen & 0x3); // nameLen % 4  (zarovnani po ctyrech bytech)
                            iconData.NameAndData = (char*)malloc(nameLen + len + 1);
                            if (iconData.NameAndData != NULL)
                            {
                                memcpy(iconData.NameAndData, f->Name, f->NameLen);                  // jmeno +
                                memset(iconData.NameAndData + f->NameLen, 0, nameLen - f->NameLen); // zarovnani nul +
                                memcpy(iconData.NameAndData + nameLen, s, len + 1);                 // icon-location + '\0'

                                iconData.SetFlag(3); // nenactena ikona dana pouze icon-location
                                                     // musime naalokovat misto pro bitmapky, v threadu to nejde
                                iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                                if (iconData.GetIndex() != -1)
                                {
                                    IconCache->Add(iconData);
                                    if (!IconCache->IsGood())
                                    {
                                        free(iconData.NameAndData);
                                        IconCache->ResetState();
                                    }
                                }
                                else
                                    free(iconData.NameAndData);
                            }
                        }
                    }
                }

                // probuzeni nacitani ikon
                if (IconCache->Count > 1)
                    IconCache->SortArray(0, IconCache->Count - 1, NULL);
                WakeupIconCacheThread(); // zacni nacitat ikony
            }
            else
            {
                int i;
                for (i = 0; i < Files->Count; i++)
                {
                    CFileData* f = &Files->At(i);

                    if (*f->Ext != 0) // existuje pripona
                    {
                        /*
            if (PackerFormatConfig.PackIsArchive(f->Name))   // je to archiv, ktery umime zpracovat?
            {
              f->Association = TRUE;
              f->Archive = TRUE;
            }
            else
            {
*/
                        char* s = f->Ext - 1;
                        char buf[MAX_PATH];
                        char* st = buf;
                        while (*++s != 0)
                            *st++ = LowerCase[*s];
                        *(DWORD*)st = 0; // nuly na konec
                        st = buf;        // pripona malymi pismeny

                        f->Association = Associations.IsAssociated(st);
                        f->Archive = FALSE;
                        /*
            }
*/
                    }
                    else
                    {
                        f->Association = FALSE;
                        f->Archive = FALSE;
                    }
                }
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                RefreshDiskFreeSpace();

                if (PluginData.NotEmpty())
                {
                    CSalamanderView view(this);
                    PluginData.SetupView(this == MainWindow->LeftPanel, &view, NULL, NULL);
                }

                // nastavime velikost ikonek pro IconCache
                CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
                IconCache->SetIconSize(iconSize);

                CFilesArray* FSFiles = GetFSFiles();
                CFilesArray* FSDirs = GetFSDirs();

                Files->SetDeleteData(FALSE); // jen melke kopie dat
                Dirs->SetDeleteData(FALSE);  // jen melke kopie dat

                if (FSFiles != NULL && FSDirs != NULL)
                {
                    // Undelete plugin dokaze ve sloucenem adresari zobrazit desitky tisic souboru
                    // na jedne hromade a realokace CFilesArray po implicitnich 200 prvcich pak sla
                    // do nekolika vterin. Protoze predem zname pocet polozek, muzeme zvolit lepsi
                    // strategii pro realokace.
                    Files->SetDelta(DeltaForTotalCount(FSFiles->Count));
                    Dirs->SetDelta(DeltaForTotalCount(FSDirs->Count));

                    int i;
                    for (i = 0; i < FSFiles->Count; i++)
                    {
                        CFileData* f = &FSFiles->At(i);

                        if (Configuration.NotHiddenSystemFiles &&
                            (f->Hidden || // Hidden i Attr jsou nulovane pokud jsou neplatne -> testy failnou
                             (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                        { // skipnuti hidden souboru/adresare
                            HiddenFilesCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                            continue;
                        }

                        if (FilterEnabled && !Filter.AgreeMasks(f->Name, f->Ext))
                        {
                            HiddenFilesCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_FILTER;
                            continue;
                        }

                        //--- pokud je jmeno obsazeno v poli HiddenNames, zahodime ho
                        if (HiddenNames.Contains(FALSE, f->Name))
                        {
                            HiddenFilesCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                            continue;
                        }

                        Files->Add(*f);
                    }

                    for (i = 0; i < FSDirs->Count; i++)
                    {
                        CFileData* f = &FSDirs->At(i);
                        if (Configuration.NotHiddenSystemFiles &&
                            (f->Hidden || // Hidden i Attr jsou nulovane pokud jsou neplatne -> testy failnou
                             (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                        { // skipnuti hidden souboru/adresare
                            HiddenDirsCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                            continue;
                        }

                        //--- pokud je jmeno obsazeno v poli HiddenNames, zahodime ho
                        if (HiddenNames.Contains(TRUE, f->Name))
                        {
                            HiddenDirsCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                            continue;
                        }

                        Dirs->Add(*f);
                    }

                    if (!Files->IsGood() || !Dirs->IsGood())
                    {
                        if (!Files->IsGood())
                            Files->ResetState();
                        if (!Dirs->IsGood())
                            Dirs->ResetState();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                }
                else
                {
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    //          TRACE_I("ReadDirectory: end");
                    return FALSE; // adresar prestal existovat ...
                }

                // pokud je prazdny panel, nastavime infoline na "No files found"
                if (Files->Count + Dirs->Count == 0)
                {
                    char buff[1000];
                    DWORD varPlacements[100];
                    int varPlacementsCount = 100;
                    if (PluginData.NotEmpty() &&
                        PluginData.GetInfoLineContent(MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT,
                                                      NULL, FALSE, 0, 0, TRUE, CQuadWord(0, 0), buff,
                                                      varPlacements, varPlacementsCount))
                    {
                        if (StatusLine->SetText(buff))
                            StatusLine->SetSubTexts(varPlacements, varPlacementsCount);
                    }
                    else
                        StatusLine->SetText(LoadStr(IDS_NOFILESFOUND));
                }

                // seradime Files a Dirs podle akt. zpusobu razeni
                SortDirectory();

                if (GetPluginIconsType() == pitFromRegistry)
                {
                    UseSystemIcons = TRUE; // pro zjednoduseni draw-item v panelu

                    // priprava pro nacitani ikon
                    IconCacheValid = FALSE;
                    MSG msg; // musime zlikvidovat pripadnou WM_USER_ICONREADING_END, ktera by nastavila IconCacheValid = TRUE
                    while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                        ;

                    int i;
                    for (i = 0; i < Associations.Count; i++)
                    {
                        if (Associations[i].GetIndex(iconSize) == -3)
                            Associations[i].SetIndex(-1, iconSize);
                    }

                    // ziskani potrebnych statickych ikon (vypakovavat nic nebudeme)
                    for (i = 0; i < Files->Count; i++)
                    {
                        const char* iconLocation = NULL;
                        CFileData* f = &Files->At(i);

                        if (*f->Ext != 0) // existuje pripona
                        {
                            /*
              if (PackerFormatConfig.PackIsArchive(f->Name))   // je to archiv, ktery umime zpracovat?
              {
                f->Association = TRUE;
                f->Archive = TRUE;
              }
              else
              {
*/
                            char* s = f->Ext - 1;
                            char* st = fileName;
                            while (*++s != 0)
                                *st++ = LowerCase[*s];
                            *(DWORD*)st = 0; // nuly na konec
                            st = fileName;   // pripona malymi pismeny

                            f->Association = Associations.IsAssociatedStatic(st, iconLocation, iconSize);
                            f->Archive = FALSE;
                            /*
              }
*/
                        }
                        else
                        {
                            f->Association = FALSE;
                            f->Archive = FALSE;
                        }

                        if (iconLocation != NULL)
                        {
                            CIconData iconData;
                            iconData.FSFileData = NULL;
                            iconData.SetReadingDone(0); // jen tak pro formu
                            int size = (int)strlen(iconLocation) + 4;
                            size -= (size & 0x3);                // size % 4  (zarovnani po ctyrech bytech)
                            const char* s = iconLocation + size; // preskok zarovnani z nul
                            int len = (int)strlen(s);
                            if (len > 0) // icon-location neni prazdna
                            {
                                int nameLen = f->NameLen + 4;
                                nameLen -= (nameLen & 0x3); // nameLen % 4  (zarovnani po ctyrech bytech)
                                iconData.NameAndData = (char*)malloc(nameLen + len + 1);
                                if (iconData.NameAndData != NULL)
                                {
                                    memcpy(iconData.NameAndData, f->Name, f->NameLen);                  // jmeno +
                                    memset(iconData.NameAndData + f->NameLen, 0, nameLen - f->NameLen); // zarovnani nul +
                                    memcpy(iconData.NameAndData + nameLen, s, len + 1);                 // icon-location + '\0'

                                    iconData.SetFlag(3); // nenactena ikona dana pouze icon-location
                                                         // musime naalokovat misto pro bitmapky, v threadu to nejde
                                    iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                                    if (iconData.GetIndex() != -1)
                                    {
                                        IconCache->Add(iconData);
                                        if (!IconCache->IsGood())
                                        {
                                            free(iconData.NameAndData);
                                            IconCache->ResetState();
                                        }
                                    }
                                    else
                                        free(iconData.NameAndData);
                                }
                            }
                        }
                    }

                    // probuzeni nacitani ikon
                    if (IconCache->Count > 1)
                        IconCache->SortArray(0, IconCache->Count - 1, NULL);
                    WakeupIconCacheThread(); // zacni nacitat ikony
                }
                else
                {
                    if (GetPluginIconsType() == pitFromPlugin)
                    {
#ifdef _DEBUG // nejspis zadna chyba, jen by se to teoreticky nemelo stat
                        if (SimplePluginIcons != NULL)
                            TRACE_E("SimplePluginIcons is not NULL before GetSimplePluginIcons().");
#endif // _DEBUG
                        SimplePluginIcons = PluginData.GetSimplePluginIcons(iconSize);
                        if (SimplePluginIcons == NULL) // neuspech -> degradace na pitSimple
                        {
                            SetPluginIconsType(pitSimple);
                        }
                    }

                    if (GetPluginIconsType() == pitSimple)
                    {
                        UseSystemIcons = FALSE; // pro zjednoduseni draw-item v panelu

                        int i;
                        for (i = 0; i < Files->Count; i++)
                        {
                            CFileData* f = &Files->At(i);

                            if (*f->Ext != 0) // existuje pripona
                            {
                                /*
                if (PackerFormatConfig.PackIsArchive(f->Name))   // je to archiv, ktery umime zpracovat?
                {
                  f->Association = TRUE;
                  f->Archive = TRUE;
                }
                else
                {
  */
                                char* s = f->Ext - 1;
                                char buf[MAX_PATH];
                                char* st = buf;
                                while (*++s != 0)
                                    *st++ = LowerCase[*s];
                                *(DWORD*)st = 0; // nuly na konec
                                st = buf;        // pripona malymi pismeny

                                f->Association = Associations.IsAssociated(st);
                                f->Archive = FALSE;
                                /*
                }
  */
                            }
                            else
                            {
                                f->Association = FALSE;
                                f->Archive = FALSE;
                            }
                        }
                    }
                    else
                    {
                        if (GetPluginIconsType() == pitFromPlugin)
                        {
                            UseSystemIcons = TRUE; // pro zjednoduseni draw-item v panelu

                            // SimplePluginIcons uz je nastaveno (kod pred GetPluginIconsType() == pitSimple)

                            // priprava pro nacitani ikon
                            IconCacheValid = FALSE;
                            MSG msg; // musime zlikvidovat pripadnou WM_USER_ICONREADING_END, ktera by nastavila IconCacheValid = TRUE
                            while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                                ;

                            // soubory/adresare, ktery nemaji jednoduchou ikonu pridame do icon-cache
                            {
                                CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory::FS-icons-from-plugin");

                                int count = FSFiles->Count + FSDirs->Count;
                                int debugCount = 0;
                                int i;
                                for (i = 0; i < count; i++)
                                {
                                    BOOL isDir = i < FSDirs->Count;
                                    CFileData* f = (isDir ? &FSDirs->At(i) : &FSFiles->At(i - FSDirs->Count));

                                    // potrebujeme ukazatel na CFileData do PluginFSDir, protoze ve Files a Dirs dochazi k presunum
                                    // napr. pri sorteni (Files a Dirs jsou pole CFileData, ne (CFileData *), proto jsou tyto problemy)
                                    if (Configuration.NotHiddenSystemFiles &&
                                        (f->Hidden || // Hidden i Attr jsou nulovane pokud jsou neplatne -> testy failnou
                                         (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                                    { // skipnuti hidden souboru/adresare
                                        continue;
                                    }
                                    if (!isDir)
                                    {
                                        if (FilterEnabled && !Filter.AgreeMasks(f->Name, f->Ext))
                                            continue;
                                    }
                                    //--- pokud je jmeno obsazeno v poli HiddenNames, zahodime ho
                                    if (HiddenNames.Contains(isDir, f->Name))
                                        continue;
                                    debugCount++;

                                    if ((!isDir || i != 0 || strcmp(f->Name, "..") != 0) &&
                                        !PluginData.HasSimplePluginIcon(*f, isDir)) // pridat do icon-cache?
                                    {
                                        CIconData iconData;
                                        iconData.FSFileData = f;
                                        iconData.SetReadingDone(0); // jen tak pro formu
                                        int nameLen = f->NameLen + 4;
                                        nameLen -= (nameLen & 0x3); // nameLen % 4  (zarovnani po ctyrech bytech)
                                        iconData.NameAndData = (char*)malloc(nameLen);
                                        if (iconData.NameAndData != NULL)
                                        {
                                            memcpy(iconData.NameAndData, f->Name, f->NameLen);                  // jmeno +
                                            memset(iconData.NameAndData + f->NameLen, 0, nameLen - f->NameLen); // zarovnani nul

                                            iconData.SetFlag(0); // zatim nenactena ikona (plugin-icon)
                                                                 // musime naalokovat misto pro bitmapky, v threadu to nejde
                                            iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                                            if (iconData.GetIndex() != -1)
                                            {
                                                IconCache->Add(iconData);
                                                if (!IconCache->IsGood())
                                                {
                                                    free(iconData.NameAndData);
                                                    IconCache->ResetState();
                                                }
                                            }
                                            else
                                                free(iconData.NameAndData);
                                        }
                                    }
                                }
                                if (debugCount != Files->Count + Dirs->Count)
                                {
                                    TRACE_E("CFilesWindow::ReadDirectory(): unexpected situation: different count of filtered items "
                                            "for icon-reading ("
                                            << debugCount << ") and panel (" << Files->Count + Dirs->Count << ")!");
                                }
                            }

                            // probuzeni nacitani ikon
                            if (IconCache->Count > 1)
                                IconCache->SortArray(0, IconCache->Count - 1, &PluginData);
                            WakeupIconCacheThread(); // zacni nacitat ikony
                        }
                        else
                            TRACE_E("Unexpected situation (1) in v CFilesWindow::ReadDirectory().");
                    }
                }
            }
            else
            {
                TRACE_E("Unexpected situation (2) in CFilesWindow::ReadDirectory().");
                DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                // nastavime velikost ikonek pro IconCache
                CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
                IconCache->SetIconSize(iconSize);
                //        TRACE_I("ReadDirectory: end");
                return FALSE;
            }
        }
    }
    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);

    // pokud pro tuto cestu mame ulozene velikosti adresaru, priradime je
    //  DirectorySizesHolder.Restore(this);

    //  TRACE_I("ReadDirectory: end");
    return TRUE;
}

// seradi pole Dirs a Files nazavisle na globalnich promennych
void SortFilesAndDirectories(CFilesArray* files, CFilesArray* dirs, CSortType sortType, BOOL reverseSort, BOOL sortDirsByName)
{
    CALL_STACK_MESSAGE1("SortDirectoryAux()");

    // POZOR: musi odpovidat sort-kodu v RefreshDirectory, ChangeSortType a CompareDirectories !!!

    if (dirs->Count > 0)
    {
        BOOL hasRoot = (dirs->At(0).NameLen == 2 && dirs->At(0).Name[0] == '.' &&
                        dirs->At(0).Name[1] == '.'); // korenovy adresar
        int firstIndex = hasRoot ? 1 : 0;
        if (dirs->Count - firstIndex > 1) // pokud je pouze jedna polozka, neni co radit
        {
            switch (sortType)
            {
            case stName:
                SortNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            case stExtension:
                SortExtName(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            case stTime:
            {
                if (sortDirsByName)
                    SortNameExt(*dirs, firstIndex, dirs->Count - 1, FALSE);
                else
                    SortTimeNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            }
            case stSize:
                SortSizeNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            case stAttr:
                SortAttrNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            }
        }
    }
    if (files->Count > 1)
    {
        switch (sortType)
        {
        case stName:
            SortNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        case stExtension:
            SortExtName(*files, 0, files->Count - 1, reverseSort);
            break;
        case stTime:
            SortTimeNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        case stSize:
            SortSizeNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        case stAttr:
            SortAttrNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        }
    }
}

void CFilesWindow::SortDirectory(CFilesArray* files, CFilesArray* dirs)
{
    CALL_STACK_MESSAGE1("CFilesWindow::SortDirectory()");

    if (files == NULL)
        files = Files;
    if (dirs == NULL)
        dirs = Dirs;
    SortFilesAndDirectories(files, dirs, SortType, ReverseSort, Configuration.SortDirsByName);

    // jednoucelove hlidace zmen promennych Configuration.SortUsesLocale a
    // Configuration.SortDetectNumbers pro metodu CFilesWindow::RefreshDirectory
    SortedWithRegSet = Configuration.SortUsesLocale;
    SortedWithDetectNum = Configuration.SortDetectNumbers;
    VisibleItemsArray.InvalidateArr();
    VisibleItemsArraySurround.InvalidateArr();
}

#ifndef _WIN64

BOOL IsWin64RedirectedDirAux(const char* subDir, const char* redirectedDir, const char* redirectedDirLastComp,
                             char* winDir, char* winDirEnd, char** lastSubDir, BOOL failIfDirWithSameNameExists)
{
    if (IsTheSamePath(subDir, redirectedDir))
    {
        strcpy(winDirEnd, redirectedDir);

        WIN32_FIND_DATA find;
        HANDLE h;
        if (failIfDirWithSameNameExists)
        {
            h = HANDLES_Q(FindFirstFile(winDir, &find));
            if (h != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(h));
                return FALSE; // nejde jen o pseudo-adresar, existuje tam stejne pojmenovany adresar, coz znamena, ze napr. kontextove menu bude vice mene normalne fungovat
            }
        }

        strcat(winDirEnd, "\\*");
        h = HANDLES_Q(FindFirstFile(winDir, &find));
        if (h != INVALID_HANDLE_VALUE)
        {
            HANDLES(FindClose(h));
            if (lastSubDir != NULL)
            {
                strcpy(*lastSubDir + 1, redirectedDirLastComp != NULL ? redirectedDirLastComp : redirectedDir);
                *lastSubDir += strlen(*lastSubDir);
            }
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsWin64RedirectedDir(const char* path, char** lastSubDir, BOOL failIfDirWithSameNameExists)
{
    if (Windows64Bit && WindowsDirectory[0] != 0)
    {
        char winDir[MAX_PATH];
        strcpy(winDir, WindowsDirectory);
        int len = (int)strlen(winDir);
        if (len > 0 && winDir[len - 1] != '\\')
            strcpy(winDir + len++, "\\");
        if (StrNICmp(winDir, path, len) == 0)
        {
            const char* subDir = path + len;
            if (IsWin64RedirectedDirAux(subDir, "Sysnative", NULL, winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\catroot", "catroot", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\catroot2", "catroot2", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                Windows7AndLater && IsWin64RedirectedDirAux(subDir, "system32\\DriverStore", "DriverStore", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\drivers\\etc", "etc", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\LogFiles", "LogFiles", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\spool", "spool", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

BOOL ContainsWin64RedirectedDir(CFilesWindow* panel, int* indexes, int count, char* redirectedDir, BOOL onlyAdded)
{
    redirectedDir[0] = 0;
    if (Windows64Bit && WindowsDirectory[0] != 0)
    {
        char path[MAX_PATH];
        lstrcpyn(path, panel->GetPath(), MAX_PATH);
        char* pathEnd = path + strlen(path);
        int i;
        for (i = 0; i < count; i++)
        {
            if (indexes[i] >= 0 && indexes[i] < panel->Dirs->Count) // zajimaji me jen podadresare
            {
                CFileData* dir = &panel->Dirs->At(indexes[i]);
                if (dir->IsLink) // vsechny pseudo-directory maji nahozeny IsLink
                {
                    *pathEnd = 0;
                    if (SalPathAppend(path, dir->Name, MAX_PATH) &&
                        IsWin64RedirectedDir(path, NULL, onlyAdded))
                    {
                        lstrcpyn(redirectedDir, dir->Name, MAX_PATH);
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

BOOL AddWin64RedirectedDirAux(const char* path, const char* subDir, const char* redirectedDirPrefix,
                              const char* redirectedDirLastComp, CFilesArray* dirs,
                              WIN32_FIND_DATA* fileData, BOOL* dirWithSameNameExists)
{
    if (IsTheSamePath(subDir, redirectedDirPrefix))
    {
        int deleteIndex = -1;
        int i;
        for (i = 0; i < dirs->Count; i++)
            if (StrICmp(dirs->At(i).Name, redirectedDirLastComp) == 0)
            {
                if (dirs->At(i).IsLink)
                    return FALSE; // tenhle redirected-dir uz jsme pridali
                deleteIndex = i;
                break;
            }

        char findPath[MAX_PATH];
        lstrcpyn(findPath, path, MAX_PATH);
        if (SalPathAppend(findPath, redirectedDirLastComp, MAX_PATH) &&
            SalPathAppend(findPath, "*", MAX_PATH))
        {
            HANDLE h;
            h = HANDLES_Q(FindFirstFile(findPath, fileData)); // find-data pro redirected-dir ziskame z adresare "." v listingu redirected-diru
            if (h != INVALID_HANDLE_VALUE)
            {
                BOOL found = FALSE;
                do
                {
                    if (strcmp(fileData->cFileName, "..") == 0 &&
                        (fileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) // adresar "."
                    {
                        found = TRUE;
                        break;
                    }
                } while (FindNextFile(h, fileData));
                HANDLES(FindClose(h));
                if (found)
                {
                    if (deleteIndex != -1)
                        dirs->Delete(deleteIndex); // je tam adresar, smazneme ho, redirected-dir ma prednost (redirector ten adresar vyignoruje)
                    lstrcpyn(fileData->cFileName, redirectedDirLastComp, MAX_PATH);
                    fileData->cAlternateFileName[0] = 0;

                    if (CutDirectory(findPath)) // zjistime, jestli na disku je adresar pojmenovany stejne jako redirected-dir (v poli 'dirs' nemusi byt napr. diky prikazu "Hide Selected Names")
                    {
                        WIN32_FIND_DATA fd;
                        h = HANDLES_Q(FindFirstFile(findPath, &fd));
                        if (h != INVALID_HANDLE_VALUE)
                        {
                            HANDLES(FindClose(h));
                            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                                StrICmp(fd.cFileName, redirectedDirLastComp) == 0)
                            {
                                *dirWithSameNameExists = TRUE;
                            }
                        }
                    }

                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

BOOL AddWin64RedirectedDir(const char* path, CFilesArray* dirs, WIN32_FIND_DATA* fileData,
                           int* index, BOOL* dirWithSameNameExists)
{
    *dirWithSameNameExists = FALSE;
    if (Windows64Bit && WindowsDirectory[0] != 0)
    {
        char winDir[MAX_PATH];
        strcpy(winDir, WindowsDirectory);
        int len = (int)strlen(winDir);
        if (len > 0 && winDir[len - 1] != '\\')
            strcpy(winDir + len++, "\\");
        if ((int)strlen(path) + 1 == len)
            winDir[--len] = 0; // patch kvuli path == win-dir (zahodime ukoncujici backslash)
        if (StrNICmp(winDir, path, len) == 0)
        {
            const char* subDir = path + len;
            if (*index == 0 && AddWin64RedirectedDirAux(path, subDir, "", "Sysnative", dirs, fileData, dirWithSameNameExists) ||
                *index == 0 && AddWin64RedirectedDirAux(path, subDir, "system32\\drivers", "etc", dirs, fileData, dirWithSameNameExists) ||
                *index == 0 && AddWin64RedirectedDirAux(path, subDir, "system32", "catroot", dirs, fileData, dirWithSameNameExists) ||
                *index <= 1 && AddWin64RedirectedDirAux(path, subDir, "system32", "catroot2", dirs, fileData, dirWithSameNameExists) && (*index = 1) != 0 ||
                *index <= 2 && AddWin64RedirectedDirAux(path, subDir, "system32", "LogFiles", dirs, fileData, dirWithSameNameExists) && (*index = 2) != 0 ||
                *index <= 3 && AddWin64RedirectedDirAux(path, subDir, "system32", "spool", dirs, fileData, dirWithSameNameExists) && (*index = 3) != 0 ||
                Windows7AndLater && *index <= 4 && AddWin64RedirectedDirAux(path, subDir, "system32", "DriverStore", dirs, fileData, dirWithSameNameExists) && (*index = 4) != 0)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

#endif // _WIN64

BOOL CFilesWindow::ChangeDir(const char* newDir, int suggestedTopIndex, const char* suggestedFocusName,
                             int mode, int* failReason, BOOL convertFSPathToInternal, BOOL showNewDirPathInErrBoxes)
{
    CALL_STACK_MESSAGE7("CFilesWindow::ChangeDir(%s, %d, %s, %d, , %d, %d)", newDir, suggestedTopIndex,
                        suggestedFocusName, mode, convertFSPathToInternal, showNewDirPathInErrBoxes);

    // zazalohujeme retezec (mohl by se v prubehu provadeni zmenit - je-li to napr. Name z CFileData z panelu)
    char backup[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup;
    }

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    char absFSPath[MAX_PATH];
    char path[2 * MAX_PATH];
    char errBuf[3 * MAX_PATH + 100];
    GetGeneralPath(path, 2 * MAX_PATH, TRUE);
    BOOL sendDirectlyToPlugin = FALSE;
    CChangeDirDlg dlg(HWindow, path, MainWindow->GetActivePanel()->Is(ptPluginFS) ? &sendDirectlyToPlugin : NULL);

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    BOOL useStopRefresh = newDir == NULL;
    if (useStopRefresh)
        BeginStopRefresh(); // cmuchal si da pohov

CHANGE_AGAIN:

    if (newDir != NULL || dlg.Execute() == IDOK)
    {
        if (newDir == NULL)
        {
            convertFSPathToInternal = TRUE; // pri zadani od uzivatele je konverze na interni format nutna
            // postprocessing provedeme jen u cesty, ktera se nema poslat primo do pluginu
            if (!sendDirectlyToPlugin && !PostProcessPathFromUser(HWindow, path))
                goto CHANGE_AGAIN;
        }
        BOOL sendDirectlyToPluginLocal = sendDirectlyToPlugin;
        TopIndexMem.Clear(); // dlouhy skok

    CHANGE_AGAIN_NO_DLG:

        UpdateWindow(MainWindow->HWindow);
        if (newDir != NULL)
            lstrcpyn(path, newDir, 2 * MAX_PATH);
        else // u cesty z dialogu nebudeme provadet fokus a nastaveni top-indexu
        {
            suggestedTopIndex = -1;
            suggestedFocusName = NULL;
        }

        char fsName[MAX_PATH];
        char* fsUserPart;
        if (!sendDirectlyToPluginLocal && IsPluginFSPath(path, fsName, (const char**)&fsUserPart))
        {
            if (strlen(fsUserPart) >= MAX_PATH) // pluginy s delsi cestou nepocitaji
            {
                sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, LoadStr(IDS_TOOLONGPATH));
                SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                              MB_OK | MB_ICONEXCLAMATION);
                if (newDir != NULL)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDPATH;
                    return FALSE; // koncime, neda se opakovat
                }
                goto CHANGE_AGAIN;
            }
            int index;
            int fsNameIndex;
            if (Plugins.IsPluginFS(fsName, index, fsNameIndex))
            {
                BOOL pluginFailure = FALSE;
                if (convertFSPathToInternal) // prevedeme cestu na interni format
                {
                    CPluginData* plugin = Plugins.Get(index);
                    if (plugin != NULL && plugin->InitDLL(MainWindow->HWindow, FALSE, TRUE, TRUE)) // plugin nemusi byt naloadeny, pripadne ho nechame naloadit
                        plugin->GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart);
                    else
                    {
                        pluginFailure = TRUE;
                        // TRACE_E("Unexpected situation in CFilesWindow::ChangeDir()");  // pokud je plugin blokovany uzivatelem (pokud chybi registracni klic k pluginu)
                    }
                }

                // vyber FS pro otevreni cesty ma toto poradi: aktivni FS, nektery z detached FS, az pak novy FS

                // CPluginData *p = Plugins.Get(index);
                // if (p != NULL) strcpy(fsName, );  // velikost pismen nechame na uzivateli, pro prevod na puvodni velikost pismen bysme museli prohledat pole p->FSNames
                int localFailReason;
                BOOL ret;
                BOOL done = FALSE;

                // pokud cestu nelze vylistovat ve FS interfacu v panelu, zkusime najit odpojeny FS interface,
                // ktery by cestu umel vylistovat (aby se zbytecne neoteviral novy FS)
                int fsNameIndexDummy;
                BOOL convertPathToInternalDummy = FALSE;
                if (!Is(ptPluginFS) || // FS interface v panelu cestu neumi vylistovat (ChangePathToPluginFS otevre novy FS)
                    !IsPathFromActiveFS(fsName, fsUserPart, fsNameIndexDummy, convertPathToInternalDummy))
                {
                    CDetachedFSList* list = MainWindow->DetachedFSList;
                    int i;
                    for (i = 0; i < list->Count; i++)
                    {
                        if (list->At(i)->IsPathFromThisFS(fsName, fsUserPart))
                        {
                            done = TRUE;
                            // zkusime zmenu na pozadovanou cestu, zaroven pripojime odpojene FS
                            ret = ChangePathToDetachedFS(i, suggestedTopIndex, suggestedFocusName, TRUE,
                                                         &localFailReason, fsName, fsUserPart, mode, mode == 3);

                            break;
                        }
                    }
                }

                if (!pluginFailure && !done)
                {
                    ret = ChangePathToPluginFS(fsName, fsUserPart, suggestedTopIndex, suggestedFocusName,
                                               FALSE, mode, NULL, TRUE, &localFailReason, FALSE, mode == 3);
                }
                else
                {
                    if (!done) // jen pokud jeste neni nastaveny 'ret' a 'localFailReason' z ChangePathToDetachedFS()
                    {
                        ret = FALSE;
                        localFailReason = CHPPFR_INVALIDPATH;
                    }
                }

                if (!ret && newDir == NULL && localFailReason == CHPPFR_INVALIDPATH)
                { // chyba cesty (muze byt i chyba pluginu - nelze naloadit - to je ale dost nepravdepodobne)
                    // prevedeme cestu na externi format
                    CPluginData* plugin = Plugins.Get(index);
                    if (!pluginFailure && plugin != NULL && plugin->InitDLL(MainWindow->HWindow, FALSE, TRUE, FALSE)) // plugin nemusi byt naloadeny, pripadne ho nechame naloadit
                        plugin->GetPluginInterfaceForFS()->ConvertPathToExternal(fsName, fsNameIndex, fsUserPart);
                    // else TRACE_E("Unexpected situation (2) in CFilesWindow::ChangeDir()");  // pokud je plugin blokovany uzivatelem (pokud chybi registracni klic k pluginu)

                    goto CHANGE_AGAIN; // vracime se zpet do dialogu zadani cesty
                }
                else
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    if (failReason != NULL)
                        *failReason = localFailReason;
                    return ret;
                }
            }
            else
            {
                sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, LoadStr(IDS_NOTPLUGINFS));
                SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                              MB_OK | MB_ICONEXCLAMATION);
                if (newDir != NULL)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDPATH;
                    return FALSE; // koncime, neda se opakovat
                }
                goto CHANGE_AGAIN;
            }
        }
        else
        {
            if (!sendDirectlyToPluginLocal &&
                (path[0] != 0 && path[1] == ':' ||                                             // cesty typu X:...
                 (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\') || // UNC cesty
                 Is(ptDisk) || Is(ptZIPArchive)))                                              // disk+archiv relativni cesty
            {                                                                                  // jde o diskovou cestu (absolutni nebo relativni) - otocime vsechny '/' na '\\' + vyhodime zdvojene '\\'
                SlashesToBackslashesAndRemoveDups(path);
            }

            int errTextID;
            const char* text = NULL;                 // pozor: nutne nastavovat textFailReason
            int textFailReason = CHPPFR_INVALIDPATH; // je-li text != NULL, obsahuje kod chyby, ktera nastala
            char curPath[2 * MAX_PATH];
            curPath[0] = 0;
            if (sendDirectlyToPluginLocal)
                errTextID = IDS_INCOMLETEFILENAME;
            else
            {
                //        MainWindow->GetActivePanel()->GetGeneralPath(curPath, 2 * MAX_PATH);
                //        if (!SalGetFullName(path, &errTextID, (MainWindow->GetActivePanel()->Is(ptDisk) ||
                //                            MainWindow->GetActivePanel()->Is(ptZIPArchive)) ? curPath : NULL))
                if (Is(ptDisk) || Is(ptZIPArchive))
                    GetGeneralPath(curPath, 2 * MAX_PATH); // kvuli FTP pluginu - rel. cesta v "target panel path" pri connectu
            }
            BOOL callNethood = FALSE;
            if (sendDirectlyToPluginLocal ||
                !SalGetFullName(path, &errTextID, (Is(ptDisk) || Is(ptZIPArchive)) ? curPath : NULL, NULL,
                                &callNethood, 2 * MAX_PATH))
            {
                sendDirectlyToPluginLocal = FALSE;
                if ((errTextID == IDS_SERVERNAMEMISSING || errTextID == IDS_SHARENAMEMISSING) && callNethood)
                { // nekompletni UNC cestu predame prvnimu pluginu, ktery podporuje FS a zavolal SalamanderGeneral->SetPluginIsNethood()
                    if (Plugins.GetFirstNethoodPluginFSName(absFSPath))
                    {
                        if (strlen(absFSPath) + 1 < MAX_PATH)
                            strcat(absFSPath, ":");
                        if (strlen(absFSPath) + strlen(path) < MAX_PATH)
                            strcat(absFSPath, path);
                        if (newDir != NULL)
                            newDir = absFSPath; // pro pripad, kdy je cesta zadana zvenci
                        else
                            strcpy(path, absFSPath);
                        goto CHANGE_AGAIN_NO_DLG; // jdeme zkusit nekompletni UNC cestu otevrit v pluginu
                    }
                }
                if (errTextID == IDS_EMPTYNAMENOTALLOWED)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    if (failReason != NULL)
                        *failReason = CHPPFR_SUCCESS;
                    return FALSE; // prazdny string, neni co delat
                }
                if (errTextID == IDS_INCOMLETEFILENAME) // relativni cesta na FS
                {
                    if (MainWindow->GetActivePanel()->Is(ptPluginFS) &&
                        MainWindow->GetActivePanel()->GetPluginFS()->NotEmpty())
                    {
                        BOOL success = TRUE;
                        if ((int)strlen(path) < MAX_PATH)
                            strcpy(absFSPath, path);
                        else
                        {
                            sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, LoadStr(IDS_TOOLONGPATH));
                            SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                          MB_OK | MB_ICONEXCLAMATION);
                            success = FALSE;
                        }
                        if (!success ||
                            MainWindow->GetActivePanel()->GetPluginFS()->GetFullFSPath(HWindow,
                                                                                       MainWindow->GetActivePanel()->GetPluginFS()->GetPluginFSName(),
                                                                                       absFSPath, MAX_PATH, success))
                        {
                            if (success) // mame absolutni cestu
                            {
                                if (newDir != NULL)
                                    newDir = absFSPath; // pro pripad, kdy je cesta zadana zvenci
                                else
                                    strcpy(path, absFSPath);
                                convertFSPathToInternal = FALSE; // cesta je nove ziskana interni, takze ji nesmime znovu konvertovat na interni
                                goto CHANGE_AGAIN_NO_DLG;        // jdeme zkusit zadanou absolutni cestu
                            }
                            else // chyba byla uzivateli vypsana
                            {
                                if (newDir != NULL)
                                {
                                    if (useStopRefresh)
                                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                                    if (failReason != NULL)
                                        *failReason = CHPPFR_INVALIDPATH;
                                    return FALSE; // koncime, neda se opakovat
                                }
                                goto CHANGE_AGAIN; // ukazeme v dialogu znovu cestu, at si ji user muze poeditovat
                            }
                        }
                    }
                }
                text = LoadStr(errTextID);
                textFailReason = CHPPFR_INVALIDPATH;
            }
            BOOL showErr = TRUE;
            if (text == NULL)
            {
                if (*path != 0 && path[1] == ':')
                    path[0] = UpperCase[path[0]]; // "c:" cesta bude "C:"
                char copy[2 * MAX_PATH + 10];
                int len = GetRootPath(copy, path);

                if (!CheckAndRestorePath(copy))
                {
                    if (newDir != NULL)
                    {
                        if (useStopRefresh)
                            EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        if (failReason != NULL)
                            *failReason = CHPPFR_INVALIDPATH;
                        return FALSE; // koncime, neda se opakovat
                    }
                    goto CHANGE_AGAIN;
                }

                if (len < (int)strlen(path)) // neni to root-cesta
                {
                    char* s = path + len - 1;     // odkud se bude kopirovat (ukazuje na '\\')
                    char* end = s;                // konec kopirovaneho useku
                    char* st = copy + (s - path); // kam budeme kopirovat
                    while (*end != 0)
                    {
                        while (*++end != 0 && *end != '\\')
                            ;
                        memcpy(st, s, end - s);
                        st[end - s] = 0;
                        s = end;

                    _TRY_AGAIN:

                        BOOL pathEndsWithSpaceOrDot;
                        DWORD copyAttr;
                        int copyLen;
                        copyLen = (int)strlen(copy);
                        if (copyLen >= MAX_PATH)
                        {
                            if (*end != 0 && !SalPathAppend(copy, end + 1, 2 * MAX_PATH)) // pokud by se dosud projita cast cesty prodlouzila a uz se nevesel zbytek cesty, pouzijeme puvodni podobu cesty
                                strcpy(copy, path);
                            text = LoadStr(IDS_TOOLONGPATH);
                            textFailReason = CHPPFR_INVALIDPATH;
                            break;
                        }
                        if (copyLen > 0 && (copy[copyLen - 1] <= ' ' || copy[copyLen - 1] == '.'))
                        {
                            copyAttr = SalGetFileAttributes(copy);
                            pathEndsWithSpaceOrDot = copyAttr != INVALID_FILE_ATTRIBUTES;
                        }
                        else
                        {
                            pathEndsWithSpaceOrDot = FALSE;
                        }

                        WIN32_FIND_DATA find;
                        HANDLE h;
                        if (!pathEndsWithSpaceOrDot)
                            h = HANDLES_Q(FindFirstFile(copy, &find));
                        else
                            h = INVALID_HANDLE_VALUE;
                        DWORD err;
                        if (h == INVALID_HANDLE_VALUE && !pathEndsWithSpaceOrDot)
                        {
                            err = GetLastError();
                            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND &&
                                err != ERROR_BAD_PATHNAME && err != ERROR_INVALID_NAME)
                            { // pokud je sance, ze jde o cestu obsahujici adresar, ke kteremu nemame pristup (zkusime jestli jsou pristupne dalsi komponenty cesty)
                                DWORD firstErr = err;
                                char* firstCopyEnd = st + strlen(st);
                                while (*end != 0)
                                {
                                    st += strlen(st); // nepristupnou komponentu cesty jen nakopirujeme (muze zustat jako dos-name nebo s jinou velikosti pismenek)
                                    while (*++end != 0 && *end != '\\')
                                        ;
                                    memcpy(st, s, end - s);
                                    st[end - s] = 0;
                                    s = end;
                                    if ((int)strlen(copy) >= MAX_PATH) // prilis dlouha cesta, koncime...
                                    {
                                        h = INVALID_HANDLE_VALUE;
                                        break;
                                    }
                                    else
                                    {
                                        h = HANDLES_Q(FindFirstFile(copy, &find));
                                        if (h != INVALID_HANDLE_VALUE)
                                            break; // nasli jsme pristupnou komponentu, pokracujeme...
                                        err = GetLastError();
                                        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
                                            err == ERROR_BAD_PATHNAME || err == ERROR_INVALID_NAME)
                                            break; // chyba v ceste, koncime...
                                    }
                                }
                                if (*end == 0 && h == INVALID_HANDLE_VALUE) // nenasli jsme dalsi pristupnou komponentu, zkusime jeste jestli nejde aktualni cesta vylistovat
                                {
                                    if ((int)strlen(copy) < MAX_PATH && SalPathAppend(copy, "*.*", MAX_PATH + 10))
                                    {
                                        h = HANDLES_Q(FindFirstFile(copy, &find));
                                        CutDirectory(copy);
                                        if (h != INVALID_HANDLE_VALUE) // cesta jde listovat
                                        {
                                            HANDLES(FindClose(h));

                                            // zmena cesty na absolutni windows cestu
                                            BOOL ret = ChangePathToDisk(HWindow, copy, suggestedTopIndex, suggestedFocusName,
                                                                        NULL, TRUE, FALSE, FALSE, failReason);
                                            if (useStopRefresh)
                                                EndStopRefresh(); // ted uz zase cmuchal nastartuje
                                            return ret;
                                        }
                                    }
                                }
                                if (h == INVALID_HANDLE_VALUE) // nenasli jsme pristupnou cast cesty, ohlasime prvni nalezenou chybu
                                {
                                    err = firstErr;
                                    *firstCopyEnd = 0;

                                    // nepouzivaji se, ale radsi vynulujeme, at to v pripadnem nove pridanem kodu hnedka spadne a doresi se to...
                                    st = NULL;
                                    end = NULL;
                                    s = NULL;
                                }
                            }
#ifndef _WIN64
                            else
                            {
                                if (IsWin64RedirectedDir(copy, &st, FALSE))
                                    continue;
                            }
#endif // _WIN64
                        }

                        if (h != INVALID_HANDLE_VALUE || pathEndsWithSpaceOrDot)
                        {
                            if (h != INVALID_HANDLE_VALUE)
                            {
                                HANDLES(FindClose(h));
                                int len2 = (int)strlen(find.cFileName); // musi se vejit (meni se jen velikost pismen - vysledek FindFirstFile)
                                if ((int)strlen(st + 1) != len2)        // dela napr. pro "aaa  " vraci "aaa", reprodukovat: Paste (text bez vnejsich uvozovek): "   "   %TEMP%\aaa   "   "
                                {
                                    TRACE_E("CFilesWindow::ChangeDir(): unexpected situation: FindFirstFile returned name with "
                                            "different length: \""
                                            << find.cFileName << "\" for \"" << (st + 1) << "\"");
                                }
                                memcpy(st + 1, find.cFileName, len2);
                                st += 1 + len2;
                                *st = 0;
                            }
                            else
                                st += strlen(st);

                            // copy obsahuje "prelozenou" cestu
                            if (!pathEndsWithSpaceOrDot)
                                copyAttr = find.dwFileAttributes;
                            if ((copyAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                            { // soubor -> jde o archiv?
                                if (PackerFormatConfig.PackIsArchive(copy))
                                {
                                    if ((int)strlen(*end != 0 ? end + 1 : end) >= MAX_PATH) // prilis dlouha cesta v archivu
                                    {
                                        if (!SalPathAppend(copy, end + 1, 2 * MAX_PATH)) // pokud by se jmeno archivu prodlouzilo a uz se nevesla cesta do archivu, pouzijeme puvodni podobu cesty
                                            strcpy(copy, path);
                                        text = LoadStr(IDS_TOOLONGPATH);
                                        textFailReason = CHPPFR_INVALIDPATH;
                                        break;
                                    }
                                    else
                                    {
                                        if (*end != 0)
                                            end++;
                                        // zmena cesty na absolutni cestu do archivu
                                        int localFailReason;
                                        BOOL ret = ChangePathToArchive(copy, end, suggestedTopIndex, suggestedFocusName,
                                                                       FALSE, NULL, TRUE, &localFailReason, FALSE, TRUE);
                                        if (!ret && localFailReason == CHPPFR_SHORTERPATH)
                                        {
                                            sprintf(errBuf, LoadStr(IDS_PATHINARCHIVENOTFOUND), end);
                                            SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                                          MB_OK | MB_ICONEXCLAMATION);
                                        }
                                        if (failReason != NULL)
                                            *failReason = localFailReason;
                                        if (useStopRefresh)
                                            EndStopRefresh(); // ted uz zase cmuchal nastartuje
                                        return ret;
                                    }
                                }
                                else
                                {
                                    char* name;
                                    char shortenedPath[MAX_PATH];
                                    strcpy(shortenedPath, copy);
                                    if (*end == 0 && CutDirectory(shortenedPath, &name)) // pokud cesta nekonci na '\\' (cesta k souboru)
                                    {
                                        // zmena cesty na absolutni windows cestu + fokus souboru
                                        ChangePathToDisk(HWindow, shortenedPath, -1, name, NULL, TRUE, FALSE, FALSE, failReason);
                                        if (useStopRefresh)
                                            EndStopRefresh(); // ted uz zase cmuchal nastartuje
                                        if (failReason != NULL && *failReason == CHPPFR_SUCCESS)
                                            *failReason = CHPPFR_FILENAMEFOCUSED;
                                        return FALSE; // listujeme jinou cestu (zkracenou o jmeno souboru)
                                    }
                                    else
                                    {
                                        text = LoadStr(IDS_NOTARCHIVEPATH);
                                        textFailReason = CHPPFR_INVALIDARCHIVE;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
                                err == ERROR_BAD_PATHNAME)
                            {
                                text = LoadStr(IDS_PATHNOTFOUND);
                            }
                            else
                            {
                                if (err == ERROR_INVALID_PARAMETER || err == ERROR_NOT_READY)
                                {
                                    char drive[MAX_PATH];
                                    lstrcpyn(drive, copy, MAX_PATH);
                                    if (CutDirectory(drive))
                                    {
                                        DWORD attrs = SalGetFileAttributes(drive);
                                        if (attrs != INVALID_FILE_ATTRIBUTES &&
                                            (attrs & FILE_ATTRIBUTE_DIRECTORY) &&
                                            (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
                                        {
                                            UINT drvType;
                                            if (copy[0] == '\\' && copy[1] == '\\')
                                            {
                                                drvType = DRIVE_REMOTE;
                                                GetRootPath(drive, copy);
                                                drive[strlen(drive) - 1] = 0; // nestojime o posledni '\\'
                                            }
                                            else
                                            {
                                                drive[0] = copy[0];
                                                drive[1] = 0;
                                                drvType = MyGetDriveType(copy);
                                            }
                                            if (drvType != DRIVE_REMOTE)
                                            {
                                                GetCurrentLocalReparsePoint(copy, CheckPathRootWithRetryMsgBox);
                                                if (strlen(CheckPathRootWithRetryMsgBox) > 3)
                                                {
                                                    lstrcpyn(drive, CheckPathRootWithRetryMsgBox, MAX_PATH);
                                                    SalPathRemoveBackslash(drive);
                                                }
                                            }
                                            else
                                                GetRootPath(CheckPathRootWithRetryMsgBox, copy);
                                            sprintf(errBuf, LoadStr(IDS_NODISKINDRIVE), drive);
                                            int msgboxRes = (int)CDriveSelectErrDlg(HWindow, errBuf, copy).Execute();
                                            CheckPathRootWithRetryMsgBox[0] = 0;
                                            UpdateWindow(MainWindow->HWindow);
                                            if (msgboxRes == IDRETRY)
                                                goto _TRY_AGAIN;
                                            showErr = FALSE;
                                        }
                                    }
                                }
                                text = GetErrorText(err);
                            }
                            textFailReason = CHPPFR_INVALIDPATH;
                            break;
                        }
                    }
                    strcpy(path, copy); // vezmeme si novou cestu
                }
            }

            if (text != NULL)
            {
                if (showErr)
                {
                    sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), showNewDirPathInErrBoxes && newDir != NULL ? newDir : path, text);
                    SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                }
                if (newDir != NULL)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    if (failReason != NULL)
                        *failReason = textFailReason;
                    return FALSE; // koncime, neda se opakovat
                }
                goto CHANGE_AGAIN;
            }
            else
            {
                // zmena cesty na absolutni windows cestu
                BOOL ret = ChangePathToDisk(HWindow, path, suggestedTopIndex, suggestedFocusName,
                                            NULL, TRUE, FALSE, FALSE, failReason);
                if (useStopRefresh)
                    EndStopRefresh(); // ted uz zase cmuchal nastartuje
                return ret;
            }
        }
    }
    UpdateWindow(MainWindow->HWindow);
    if (useStopRefresh)
        EndStopRefresh(); // ted uz zase cmuchal nastartuje
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CFilesWindow::ChangeDirLite(const char* newDir)
{
    int failReason;
    BOOL ret = ChangeDir(newDir, -1, NULL, 3, &failReason, TRUE);
    return ret || failReason == CHPPFR_SHORTERPATH || failReason == CHPPFR_FILENAMEFOCUSED ||
           failReason == CHPPFR_SUCCESS /* nesmysl, ale zily se nam nezkrati */;
}

BOOL CFilesWindow::ChangePathToDrvType(HWND parent, int driveType, const char* displayName)
{
    char path[MAX_PATH];
    const char* userFolderOneDrive = NULL;
    if (driveType == drvtOneDrive || driveType == drvtOneDriveBus)
    {
        InitOneDrivePath();
        if (driveType == drvtOneDrive && OneDrivePath[0] == 0 ||
            driveType == drvtOneDriveBus && !OneDriveBusinessStorages.Find(displayName, &userFolderOneDrive))
        { // OneDrive Personal/Business byl odpojen, provedeme refresh Drive bary, at zmizi nebo se updatne jeho ikona
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
            return FALSE;
        }
    }
    if (driveType == drvtMyDocuments && GetMyDocumentsOrDesktopPath(path, MAX_PATH) ||
        driveType == drvtGoogleDrive && ShellIconOverlays.GetPathForGoogleDrive(path, MAX_PATH) ||
        driveType == drvtDropbox && strcpy_s(path, DropboxPath) == 0 ||
        driveType == drvtOneDrive && strcpy_s(path, OneDrivePath) == 0 ||
        driveType == drvtOneDriveBus && (int)strlen(userFolderOneDrive) < MAX_PATH && strcpy_s(path, userFolderOneDrive) == 0)
    {
        return ChangePathToDisk(parent, path);
    }
    return FALSE;
}

void CFilesWindow::ChangeDrive(char drive)
{
    CALL_STACK_MESSAGE2("CFilesWindow::ChangeDrive(%u)", drive);
    //---  obnova DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() != this);
    //---  pripadny vyber disku z dialogu
    CFilesWindow* anotherPanel = (Parent->LeftPanel == this ? Parent->RightPanel : Parent->LeftPanel);
    if (drive == 0)
    {
        CDriveTypeEnum driveType = drvtUnknow; // dummy
        DWORD_PTR driveTypeParam = (Is(ptDisk) || Is(ptZIPArchive)) ? GetPath()[0] : 0;
        int postCmd;
        void* postCmdParam;
        BOOL fromContextMenu;
        if (CDrivesList(this, ((Is(ptDisk) || Is(ptZIPArchive)) ? GetPath() : ""),
                        &driveType, &driveTypeParam, &postCmd, &postCmdParam,
                        &fromContextMenu)
                    .Track() == FALSE ||
            fromContextMenu && postCmd == 0) // pouze close-menu spustene z kontextoveho menu
        {
            return;
        }

        if (!fromContextMenu && driveType != drvtPluginFS && driveType != drvtPluginCmd)
            TopIndexMem.Clear(); // dlouhy skok

        UpdateWindow(MainWindow->HWindow);

        char path[MAX_PATH];
        switch (driveType)
        {
        case drvtMyDocuments:
        case drvtGoogleDrive:
        case drvtDropbox:
        case drvtOneDrive:
        case drvtOneDriveBus:
        {
            ChangePathToDrvType(HWindow, driveType, driveType == drvtOneDriveBus ? (const char*)driveTypeParam : NULL);
            if (driveType == drvtOneDriveBus)
                free((char*)driveTypeParam);
            return;
        }

        case drvtOneDriveMenu:
            TRACE_E("CFilesWindow::ChangeDrive(): unexpected drive type: drvtOneDriveMenu");
            break;

        // jde o Network?
        case drvtNeighborhood:
        {
            if (GetTargetDirectory(HWindow, HWindow, LoadStr(IDS_CHANGEDRIVE),
                                   LoadStr(IDS_CHANGEDRIVETEXT), path, TRUE))
            {
                UpdateWindow(MainWindow->HWindow);
                ChangePathToDisk(HWindow, path);
                return;
            }
            else
                return;
        }

        // jde o other Panel?
        case drvtOtherPanel:
        {
            ChangePathToOtherPanelPath();
            return;
        }

        // jde o Hot Path?
        case drvtHotPath:
        {
            GotoHotPath((int)driveTypeParam);
            return;
        }

        // jde o normalni cestu
        case drvtUnknow:
        case drvtRemovable:
        case drvtFixed:
        case drvtRemote:
        case drvtCDROM:
        case drvtRAMDisk:
        {
            drive = (char)LOWORD(driveTypeParam);

            UpdateWindow(MainWindow->HWindow);
            break;
        }

        case drvtPluginFS:
        {
            CPluginFSInterfaceAbstract* fs = (CPluginFSInterfaceAbstract*)driveTypeParam;
            CPluginInterfaceForFSEncapsulation* ifaceForFS = NULL;
            // musime overit, ze 'fs' je stale platny interface
            if (Is(ptPluginFS) && GetPluginFS()->GetInterface() == fs)
            { // vyber aktivniho FS - provedeme refresh
                if (!fromContextMenu)
                    RefreshDirectory();
                else
                    ifaceForFS = GetPluginFS()->GetPluginInterfaceForFS();
            }
            else
            {
                CDetachedFSList* list = MainWindow->DetachedFSList;
                int i;
                for (i = 0; i < list->Count; i++)
                {
                    if (list->At(i)->GetInterface() == fs)
                    { // vyber odpojeneho FS
                        if (!fromContextMenu)
                            ChangePathToDetachedFS(i);
                        else
                            ifaceForFS = list->At(i)->GetPluginInterfaceForFS();
                        break;
                    }
                }
            }

            if (ifaceForFS != NULL) // post-cmd z kontextoveho menu aktivniho/odpojeneho FS
            {
                ifaceForFS->ExecuteChangeDrivePostCommand(PANEL_SOURCE, postCmd, postCmdParam);
            }
            return;
        }

        case drvtPluginFSInOtherPanel:
            return; // nepovolena akce (FS z vedlejsiho panelu nelze dat do aktivniho panelu)

        case drvtPluginCmd:
        {
            const char* dllName = (const char*)driveTypeParam;
            CPluginData* data = Plugins.GetPluginData(dllName);
            if (data != NULL) // plug-in existuje, jdeme spustit prikaz
            {
                if (!fromContextMenu) // prikaz polozky FS
                {
                    data->ExecuteChangeDriveMenuItem(PANEL_SOURCE);
                }
                else // post-cmd z kontextoveho menu polozky FS
                {
                    data->GetPluginInterfaceForFS()->ExecuteChangeDrivePostCommand(PANEL_SOURCE, postCmd, postCmdParam);
                }
            }
            return;
        }

        default:
        {
            TRACE_E("Unknown DriveType = " << driveType);
            return;
        }
        }
    }
    else
        TopIndexMem.Clear(); // dlouhy skok

    ChangePathToDisk(HWindow, DefaultDir[LowerCase[drive] - 'a'], -1, NULL,
                     NULL, TRUE, FALSE, FALSE, NULL, FALSE);
}

void CFilesWindow::UpdateFilterSymbol()
{
    CALL_STACK_MESSAGE_NONE
    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
}

void CFilesWindow::UpdateDriveIcon(BOOL check)
{
    CALL_STACK_MESSAGE2("CFilesWindow::UpdateDriveIcon(%d)", check);
    if (Is(ptDisk))
    {
        if (!check || CheckPath(FALSE) == ERROR_SUCCESS)
        { // jen je-li cesta pristupna
            if (DirectoryLine->HWindow != NULL)
            {
                UINT type = MyGetDriveType(GetPath());
                char root[MAX_PATH];
                GetRootPath(root, GetPath());
                HICON hIcon = GetDriveIcon(root, type, TRUE);
                DirectoryLine->SetDriveIcon(hIcon);
                HANDLES(DestroyIcon(hIcon));
            }
            // 2.5RC3: tlacitko v drive bars je treba nastavit i v pripade, ze vypnuta directory line
            MainWindow->UpdateDriveBars(); // zamackneme v drive bar spravny disk
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            if (DirectoryLine->HWindow != NULL)
            {
                HICON hIcon = LoadArchiveIcon(IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags);
                DirectoryLine->SetDriveIcon(hIcon);
                HANDLES(DestroyIcon(hIcon));
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                if (DirectoryLine->HWindow != NULL)
                {
                    BOOL destroyIcon;
                    HICON icon = GetPluginFS()->GetFSIcon(destroyIcon);
                    if (icon != NULL) // plug-inem definovana
                    {
                        DirectoryLine->SetDriveIcon(icon);
                        if (destroyIcon)
                            HANDLES(DestroyIcon(icon));
                    }
                    else // standardni
                    {
                        icon = SalLoadIcon(HInstance, IDI_PLUGINFS, IconSizes[ICONSIZE_16]);
                        DirectoryLine->SetDriveIcon(icon);
                        HANDLES(DestroyIcon(icon));
                    }
                }
                // 2.5RC3: tlacitko v drive bars je treba nastavit i v pripade, ze vypnuta directory line
                MainWindow->UpdateDriveBars(); // zamackneme v drive bar spravny disk
            }
        }
    }
}
