// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "dialogs.h"
#include "mainwnd.h"
#include "usermenu.h"
#include "plugins.h"
#include "fileswnd.h"
#include "stswnd.h"
#include "snooper.h"
#include "zip.h"
#include "shellib.h"
#include "pack.h"
#include "thumbnl.h"
#include "geticon.h"
#include "shiconov.h"

//
// ****************************************************************************
// CFilesWindowAncestor
//

CFilesWindowAncestor::CFilesWindowAncestor()
{
    CALL_STACK_MESSAGE_NONE
    Files = new CFilesArray;
    Dirs = new CFilesArray;
    SelectedCount = 0;

    Path[0] = 0;
    SuppressAutoRefresh = FALSE;
    PanelType = ptDisk;
    MonitorChanges = TRUE;
    DriveType = DRIVE_UNKNOWN;

    ArchiveDir = NULL;
    ZIPArchive[0] = 0;
    ZIPPath[0] = 0;

    PluginFS.Init(NULL, NULL, NULL, NULL, NULL, NULL, -1, 0, 0, 0);
    PluginFSDir = NULL;
    PluginIconsType = pitSimple;
    SimplePluginIcons = NULL;

    char buf[MAX_PATH];
    GetSystemDirectory(buf, MAX_PATH);
    GetRootPath(Path, buf);

    OnlyDetachFSListing = FALSE;
    NewFSFiles = NULL;
    NewFSDirs = NULL;
    NewFSPluginFSDir = NULL;
    NewFSIconCache = NULL;

    PluginIface = NULL;
    PluginIfaceLastIndex = -1;
}

CFilesWindowAncestor::~CFilesWindowAncestor()
{
    CALL_STACK_MESSAGE1("CFilesWindowAncestor::~CFilesWindowAncestor()");
    if (Files != NULL)
        delete Files;
    if (Dirs != NULL)
        delete Dirs;
    if (PluginFS.NotEmpty() || PluginData.NotEmpty() ||
        ArchiveDir != NULL || PluginFSDir != NULL)
    {
        TRACE_E("Unexpected situation in CFilesWindowAncestor::~CFilesWindowAncestor()");
    }
}

DWORD
CFilesWindowAncestor::CheckPath(BOOL echo, const char* path, DWORD err, BOOL postRefresh, HWND parent)
{
    CALL_STACK_MESSAGE5("CFilesWindowAncestor::CheckPath(%d, %s, 0x%X, %d, )", echo, path, err, postRefresh);

    parent = (parent == NULL) ? HWindow : parent;
    if (path == NULL)
        path = GetPath();

    return SalCheckPath(echo, path, err, postRefresh, parent);
}

void CFilesWindowAncestor::ReleaseListing()
{
    CALL_STACK_MESSAGE_NONE

        ((CFilesWindow*)this)
            ->VisibleItemsArray.InvalidateArr();
    ((CFilesWindow*)this)->VisibleItemsArraySurround.InvalidateArr();
    if (OnlyDetachFSListing)
    {
        // odpojime z panelu listing vcetne ikon
        Files = NewFSFiles;
        Dirs = NewFSDirs;
        SetPluginFSDir(NewFSPluginFSDir);
        PluginData.Init(NULL, NULL, NULL, NULL, 0);
        if (NewFSIconCache != NULL)
            ((CFilesWindow*)this)->IconCache = NewFSIconCache;
        ((CFilesWindow*)this)->SetValidFileData(GetPluginFSDir()->GetValidData());

        OnlyDetachFSListing = FALSE;
        NewFSFiles = NULL;
        NewFSDirs = NULL;
        NewFSPluginFSDir = NULL;
        NewFSIconCache = NULL;
    }
    else
    {
        ReleaseListingBody(PanelType, ArchiveDir, PluginFSDir, PluginData, Files, Dirs, FALSE);
    }
    SelectedCount = 0;
}

BOOL CFilesWindowAncestor::IsPathFromActiveFS(const char* fsName, char* fsUserPart, int& fsNameIndex,
                                              BOOL& convertPathToInternal)
{
    CALL_STACK_MESSAGE_NONE
    fsNameIndex = -1;
    if (Is(ptPluginFS) && PluginFS.NotEmpty())
    {
        if (Plugins.AreFSNamesFromSamePlugin(PluginFS.GetPluginFSName(), fsName, fsNameIndex)) // porovname jestli jsou FS ze stejneho plug-inu
        {
            if (convertPathToInternal)
            {
                PluginFS.GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart);
                convertPathToInternal = FALSE;
            }
            return PluginFS.IsOurPath(PluginFS.GetPluginFSNameIndex(), fsNameIndex, fsUserPart);
        }
    }
    return FALSE;
}

BOOL CFilesWindowAncestor::GetGeneralPath(char* buf, int bufSize, BOOL convertFSPathToExternal)
{
    CALL_STACK_MESSAGE_NONE
    if (bufSize == 0)
        return FALSE;
    BOOL ret = TRUE;
    char buf2[2 * MAX_PATH];
    if (Is(ptDisk))
    {
        int l = (int)strlen(GetPath());
        if (l >= bufSize)
        {
            l = bufSize - 1;
            ret = FALSE;
        }
        memcpy(buf, GetPath(), l);
        buf[l] = 0;
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            strcpy(buf2, GetZIPArchive());
            if (GetZIPPath()[0] != 0)
            {
                if (GetZIPPath()[0] != '\\')
                    strcat(buf2, "\\");
                strcat(buf2, GetZIPPath());
            }
            int l = (int)strlen(buf2);
            if (l >= bufSize)
            {
                l = bufSize - 1;
                ret = FALSE;
            }
            memcpy(buf, buf2, l);
            buf[l] = 0;
        }
        else
        {
            if (Is(ptPluginFS))
            {
                strcpy(buf2, PluginFS.GetPluginFSName());
                strcat(buf2, ":");
                char* userPart = buf2 + strlen(buf2);
                if (PluginFS.NotEmpty() && PluginFS.GetCurrentPath(userPart))
                {
                    if (convertFSPathToExternal)
                    {
                        PluginFS.GetPluginInterfaceForFS()->ConvertPathToExternal(PluginFS.GetPluginFSName(),
                                                                                  PluginFS.GetPluginFSNameIndex(),
                                                                                  userPart);
                    }

                    int l = (int)strlen(buf2);
                    if (l >= bufSize)
                    {
                        l = bufSize - 1;
                        ret = FALSE;
                    }
                    memcpy(buf, buf2, l);
                    buf[l] = 0;
                }
                else
                {
                    buf[0] = 0;
                    ret = FALSE;
                }
            }
            else
            {
                buf[0] = 0;
                ret = FALSE;
            }
        }
    }
    return ret;
}

void CFilesWindowAncestor::SetPath(const char* path)
{
    CALL_STACK_MESSAGE2("CFilesWindowAncestor::SetPath(%s)", path);
    if (SuppressAutoRefresh && (!Is(ptDisk) || !IsTheSamePath(path, Path)))
        SuppressAutoRefresh = FALSE;
    DetachDirectory((CFilesWindow*)this);
    strcpy(Path, path);

    //--- zjisteni file-based komprese/sifrovani a FAT32
    DWORD dummy1, flags;
    if ((Is(ptDisk) || Is(ptZIPArchive)) &&
        MyGetVolumeInformation(path, NULL, NULL, NULL, NULL, 0, NULL, &dummy1, &flags, NULL, 0))
    {
        ((CFilesWindow*)this)->FileBasedCompression = (flags & FS_FILE_COMPRESSION) != 0 && Is(ptDisk);
        ((CFilesWindow*)this)->FileBasedEncryption = (flags & FILE_SUPPORTS_ENCRYPTION) != 0 && Is(ptDisk);
        ((CFilesWindow*)this)->SupportACLS = (flags & FS_PERSISTENT_ACLS) != 0 && Is(ptDisk);
    }
    else
    {
        ((CFilesWindow*)this)->FileBasedCompression = FALSE;
        ((CFilesWindow*)this)->FileBasedEncryption = FALSE;
        ((CFilesWindow*)this)->SupportACLS = FALSE;
    }

    MonitorChanges = FALSE;
    DriveType = DRIVE_UNKNOWN;
    if (!Is(ptPluginFS)) // pluginFS zajistuje zmeny jinak ...
    {
        DriveType = MyGetDriveType(Path);
        switch (DriveType)
        {
        case DRIVE_REMOVABLE:
        {
            BOOL isDriveFloppy = FALSE; // floppy maji svuji konfiguraci vedle ostatnich removable drivu
            int drv = UpperCase[Path[0]] - 'A' + 1;
            if (drv >= 1 && drv <= 26) // pro jistotu provedeme "range-check"
            {
                DWORD medium = GetDriveFormFactor(drv);
                if (medium == 350 || medium == 525 || medium == 800 || medium == 1)
                    isDriveFloppy = TRUE;
            }
            MonitorChanges = isDriveFloppy ? Configuration.DrvSpecFloppyMon : Configuration.DrvSpecRemovableMon;
            break;
        }

        case DRIVE_REMOTE:
        {
            MonitorChanges = Configuration.DrvSpecRemoteMon;
            break;
        }

        case DRIVE_CDROM:
        {
            MonitorChanges = Configuration.DrvSpecCDROMMon;
            break;
        }

        default: // case DRIVE_FIXED:   // nejen fixed, ale i ty ostatni (RAM DISK, atd.)
        {
            MonitorChanges = Configuration.DrvSpecFixedMon;
            break;
        }
        }

        // osetrime potlaceni autorefreshe
        if (SuppressAutoRefresh)
            MonitorChanges = FALSE;

        if (MonitorChanges)
            AddDirectory((CFilesWindow*)this, Path, DriveType == DRIVE_REMOVABLE || DriveType == DRIVE_FIXED);
        else // pokud zmeny nemonitorujeme, nezavola snooper SetAutomaticRefresh -> udelame to tady
        {
            ((CFilesWindow*)this)->SetAutomaticRefresh(FALSE, TRUE);
        }
    }
    else // ptPluginFS - nebudeme provadet zadne refreshe - plug-in si refreshovani ridi sam
    {
        ((CFilesWindow*)this)->SetAutomaticRefresh(TRUE, TRUE);
    }
}

CFilesArray*
CFilesWindowAncestor::GetArchiveDirFiles(const char* zipPath)
{
    CALL_STACK_MESSAGE_NONE
    if (zipPath == NULL)
        zipPath = ZIPPath;
    return ArchiveDir->GetFiles(zipPath);
}

CFilesArray*
CFilesWindowAncestor::GetArchiveDirDirs(const char* zipPath)
{
    CALL_STACK_MESSAGE_NONE
    if (zipPath == NULL)
        zipPath = ZIPPath;
    return ArchiveDir->GetDirs(zipPath);
}

CFilesArray*
CFilesWindowAncestor::GetFSFiles()
{
    CALL_STACK_MESSAGE_NONE
    return PluginFSDir->GetFiles("");
}

CFilesArray*
CFilesWindowAncestor::GetFSDirs()
{
    CALL_STACK_MESSAGE_NONE
    return PluginFSDir->GetDirs("");
}

CPluginData*
CFilesWindowAncestor::GetPluginDataForPluginIface()
{
    return Plugins.GetPluginData(PluginIface, &PluginIfaceLastIndex);
}

void CFilesWindowAncestor::SetZIPPath(const char* path)
{
    CALL_STACK_MESSAGE_NONE
    if (*path == '\\')
        path++; // '\\' nebude na zacatku ZIPPath
    int l = (int)strlen(path);
    if (l > 0 && path[l - 1] == '\\')
        l--; // ZIPPath nebude koncit na '\\'
    memcpy(ZIPPath, path, l);
    ZIPPath[l] = 0;
}

void CFilesWindowAncestor::SetZIPArchive(const char* archive)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(ZIPArchive, archive);
}

BOOL CFilesWindowAncestor::SamePath(CFilesWindowAncestor* other)
{
    CALL_STACK_MESSAGE_NONE
    int l1 = (int)strlen(Path);
    if (l1 > 0 && Path[l1 - 1] == '\\')
        l1--;
    int l2 = (int)strlen(other->Path);
    if (l2 > 0 && other->Path[l2 - 1] == '\\')
        l2--;
    return (PanelType == ptDisk || PanelType == ptZIPArchive) &&
           (other->PanelType == ptDisk || other->PanelType == ptZIPArchive) &&
           l1 == l2 && StrNICmp(Path, other->Path, l1) == 0;
}

//
// ****************************************************************************
// CFilesWindow
//

void IconThreadThreadFBodyAux(const char* path, SHFILEINFO& shi, CIconSizeEnum iconSize)
{
    CALL_STACK_MESSAGE_NONE
    __try
    {
        // nenechame si vracet default icon; v pripade selhani se uplatni simple icons
        if (!GetFileIcon(path, FALSE, &shi.hIcon, iconSize, FALSE, FALSE))
            shi.hIcon = NULL;

        //Presli jsme na vlastni implementaci (mensi pametova narocnost, fungujici XOR ikonky)
        //Navic nepodporuje ziskani EXTRALARGE a JUMBO ikon, je potreba pristupovat do system image list
        //SHGetFileInfo(path, 0, &shi, sizeof(shi),
        //              SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 4))
    {
        FGIExceptionHasOccured++;
        shi.hIcon = NULL;
    }
}

unsigned IconThreadThreadFBody(void* parameter)
{
    CALL_STACK_MESSAGE1("IconThreadThreadFBody()");

    SetThreadNameInVCAndTrace("IconsReader");
    TRACE_I("Begin");
    CFilesWindow* window = (CFilesWindow*)parameter;

    // aby chodily shell-extensiony tahajici ikony pres IconHandler a dalsi COM/OLE sracky
    if (OleInitialize(NULL) != S_OK)
        TRACE_E("Error in OleInitialize.");

    IShellIconOverlayIdentifier** iconReadersIconOverlayIds = ShellIconOverlays.CreateIconReadersIconOverlayIds();

    HANDLE handles[2];
    handles[0] = window->ICEventTerminate;
    handles[1] = window->ICEventWork;
    DWORD wait = WAIT_TIMEOUT;
    BOOL run = TRUE;
    BOOL firstRound = TRUE; // pri chybe se posila REFRESH, ale jen poprve

    CSalamanderThumbnailMaker thumbMaker(window);

    while (run)
    {
        if (wait == WAIT_TIMEOUT) // jinak je uz wait naplnen z work modu
            wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        switch (wait)
        {
        case WAIT_OBJECT_0 + 1: // work
        {
            CALL_STACK_MESSAGE1("IconThreadThreadFBody::work");
            window->IconCacheValid = FALSE; // potreba pro refreshe (sleep/wake-up icon readera), jinak nastavuje hl.thread

            // j.r. puvodnich 200ms je uz asi zbytecne dlouha doba, zkracuji na 20ms
            // j.r. 20ms bylo malo, thread stihal startovat pri prostem drzeni Enter na vstupu do adresare
            // petr: hlavni thread se prekresluje prednostne, protoze je na vyssi priorite + kdyz tu byl
            //       tenhle sleep, blikaly overlaye icon (napr. Tortoise SVN) jeste vic nez blikaji ted
            // dame hl. threadu chvilku na kresleni a pripadne rychle preruseni pri zmene adresare
            // (pouziva se uz jen jako "pauza", ve ktere stihne RefreshDirectory() vnutit nove ikony do icon-cache, viz 'WaitBeforeReadingIcons')
            if (window->WaitBeforeReadingIcons > 0)
                Sleep(window->WaitBeforeReadingIcons);
            if (window->WaitOneTimeBeforeReadingIcons > 0)
            {
                DWORD time = window->WaitOneTimeBeforeReadingIcons;
                window->WaitOneTimeBeforeReadingIcons = 0;
                Sleep(time); // cekame pred zahajenim cteni icon-overlays, behem tohoto cekani by mely dorazit vsechny notifikace o zmenach od Tortoise SVN (viz IconOverlaysChangedOnPath())
            }

            HANDLES(EnterCriticalSection(&window->ICSleepSection));

            // nemame zacit novou praci (wake-up -> sleep -> wake-up) nebo se terminovat?
            wait = WaitForMultipleObjects(2, handles, FALSE, 0);

            //        BOOL postRefresh = FALSE;
            if (wait == WAIT_TIMEOUT && !window->ICStopWork)
            {
                //          TRACE_I("Start reading.");
                window->ICWorking = TRUE;

                CIconSizeEnum iconSize = window->GetIconSizeForCurrentViewMode();

                CIconList* iconList;
                int iconListIndex;
                SHFILEINFO shi; // z historickych duvodu (SHGetFileInfo) se pro vsechny typy ikon pouziva shi.hIcon

                // priprava plne cesty k nacitanym souborum/adresarum (jen pro window->Is(ptDisk))
                char path[MAX_PATH + 10];
                path[0] = 0;
                WCHAR wPath[MAX_PATH + 10];
                wPath[0] = 0;
                char* name = path;
                WCHAR* wName = wPath;
                BOOL pathIsInvalid = FALSE;
                BOOL isGoogleDrivePath = FALSE;
                if (window->Is(ptDisk))
                {
                    int l = (int)strlen(window->GetPath());
                    memmove(path, window->GetPath(), l);
                    if (path[l - 1] != '\\')
                        path[l++] = '\\';
                    name = path + l; // ukazatel na misto jmena ve fullnamu
                    *name = 0;
                    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path, l, wPath, MAX_PATH + 10);
                    wName = wPath + l;
                    *wName = 0;
                    pathIsInvalid = !PathContainsValidComponents(path, FALSE);
                    if (pathIsInvalid)
                        TRACE_I("Path contains invalid components, shell cannot read icons from such paths! Path: " << path);
                    isGoogleDrivePath = ShellIconOverlays.IsGoogleDrivePath(path);
                }

                BOOL readOnlyVisibleItems = window->InactWinOptimizedReading; // refreshe ze snooperu v neaktivnim okne: cteme jen viditelne ikony/thumbnaily/overlaye, setrime strojovy cas (jsme "na pozadi")
                                                                              //          if (readOnlyVisibleItems) TRACE_I("Refresh in inactive window, reading only visible icons...");
                BOOL readOnlyVisibleItemsDueToUMI = FALSE;                    // popis viz dole
                if (!readOnlyVisibleItems && UserMenuIconBkgndReader.IsReadingIcons())
                {
                    //            TRACE_I("Reading of usermenu icons is in progress, reading only visible icons...");
                    readOnlyVisibleItems = TRUE;
                    readOnlyVisibleItemsDueToUMI = TRUE;
                }

                BOOL readThumbnails = window->UseThumbnails; // mame se pokusit nacitat thumbnaily?

                if (window->StopThumbnailLoading)
                    readThumbnails = FALSE; // nechteny wake-up - potlacime aspon load thumbnailu

                BOOL pluginFSIconsFromPlugin = window->Is(ptPluginFS) &&
                                               window->GetPluginIconsType() == pitFromPlugin;
                BOOL pluginFSIconsFromRegistry = window->Is(ptPluginFS) &&
                                                 window->GetPluginIconsType() == pitFromRegistry;

                BOOL waitBeforeFirstReadIcon = FALSE; // TRUE jen pri skoku na SECOND_ROUND:
                BOOL repeatedRound = FALSE;           // TRUE pri opakovani nacitani ikon/thumbnailu kvuli probihajicimu cteni usermenu ikon

            SECOND_ROUND: // pokud se na disku nepovede precist nejaka ikona, provede se nakonec jeste druhy pokus

                DWORD wanted = -1;                                 // neplatny -> nic neudela, pak sleepne
                if (window->Is(ptDisk) || pluginFSIconsFromPlugin) // disk + FS/icons-from-plugin
                {
                    wanted = 0; // nejprve nactem nove ikonky, pak teprve stare
                }
                else
                {
                    if (window->Is(ptZIPArchive) || pluginFSIconsFromRegistry) // archiv + FS/icons-from-registry
                    {
                        wanted = 3; // nase ikonky jsou dane svou icon-location
                    }
                    else
                        TRACE_E("Unexpected situation.");
                }
                // pred zacatkem prace nastavime FALSE do "ReadingDone" vsech polozek icon-cache + do "IconOverlayDone" vsech polozek v panelu
                int x;
                if (!repeatedRound)
                    for (x = 0; x < window->IconCache->Count; x++)
                        window->IconCache->At(x).SetReadingDone(0);
                if (firstRound && !repeatedRound)
                {
                    for (x = 0; x < window->Files->Count; x++)
                        window->Files->At(x).IconOverlayDone = 0;
                    for (x = 0; x < window->Dirs->Count; x++)
                        window->Dirs->At(x).IconOverlayDone = 0;
                }

                BOOL failed = FALSE;
                BOOL destroyPluginIcon = TRUE;

                int selectMode = 1;
                // 1 = postupny pruchod (VisibleItemsArray.IsArrValid() == FALSE),
                // 2 = pruchod podle VisibleItemsArray,
                // 3 = pruchod podle VisibleItemsArraySurround,
                // 4 = postupny pruchod (VisibleItemsArray.IsArrValid() == TRUE)

                BOOL canReadIconOverlays = firstRound && window->Is(ptDisk) && iconReadersIconOverlayIds != NULL;
                BOOL readIconOverlaysNow = FALSE; // TRUE = ted se ctou overlaye, FALSE = ctou se ikony + thumbnaily

                //          TRACE_I("wanted=" << wanted << ", selectMode=" << selectMode);

                int lastVisArrVersion = -1;
                BOOL someNameSkipped = FALSE;
                int thumbnailFlag = 0;
                int i = 0;
                while (1)
                {
                    BOOL callWaitForObjects = TRUE;                                                                        // jen optimalizace - behem hledani polozky (netrva skoro zadny cas) se nevola WaitForMultipleObjects
                    if (i < (readIconOverlaysNow ? window->Files->Count + window->Dirs->Count : window->IconCache->Count)) // nacteni ikony ze souboru/adresare nebo zjisteni icon-overlaye pro soubor/adresar
                    {
                        CIconData* iconData = readIconOverlaysNow ? NULL : &window->IconCache->At(i);

                        BOOL skipName = FALSE;
                        if (selectMode == 1)
                        {
                            int visArrVer;
                            if (window->VisibleItemsArray.IsArrValid(&visArrVer))
                            {
                                i = 0;
                                lastVisArrVersion = visArrVer;
                                selectMode = 2;
                                //                  TRACE_I("selectMode=" << selectMode);
                                readIconOverlaysNow = FALSE;
                                //                  TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                continue;
                            }
                        }
                        else
                        {
                            if (selectMode == 2 || selectMode == 3)
                            {
                                int visArrVer;
                                BOOL visArrValid;
                                BOOL cont;
                                if (selectMode == 2)
                                {
                                    if (readIconOverlaysNow)
                                        cont = window->VisibleItemsArray.ArrContainsIndex(i, &visArrValid, &visArrVer);
                                    else
                                        cont = window->VisibleItemsArray.ArrContains(iconData->NameAndData,
                                                                                     &visArrValid, &visArrVer);
                                }
                                else
                                {
                                    if (readIconOverlaysNow)
                                        cont = window->VisibleItemsArraySurround.ArrContainsIndex(i, &visArrValid, &visArrVer);
                                    else
                                        cont = window->VisibleItemsArraySurround.ArrContains(iconData->NameAndData,
                                                                                             &visArrValid, &visArrVer);
                                }
                                if (!cont && visArrValid && visArrVer == lastVisArrVersion)
                                    skipName = TRUE;
                                else
                                {
                                    if (!visArrValid)
                                    {
                                        i = 0;
                                        selectMode = 1;
                                        //                      TRACE_I("selectMode=" << selectMode);
                                        readIconOverlaysNow = FALSE;
                                        //                      TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                        continue;
                                    }
                                    else
                                    {
                                        if (visArrVer != lastVisArrVersion)
                                        {
                                            i = 0;
                                            lastVisArrVersion = visArrVer;
                                            selectMode = 2;
                                            //                        TRACE_I("selectMode=" << selectMode);
                                            readIconOverlaysNow = FALSE;
                                            //                        TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                            continue;
                                        }
                                    }
                                }
                            }
                            else // selectMode == 4
                            {
                                int visArrVer;
                                if (window->VisibleItemsArray.IsArrValid(&visArrVer) && visArrVer != lastVisArrVersion)
                                {
                                    i = 0;
                                    lastVisArrVersion = visArrVer;
                                    selectMode = 2;
                                    //                    TRACE_I("selectMode=" << selectMode);
                                    readIconOverlaysNow = FALSE;
                                    //                    TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                    continue;
                                }
                            }
                        }

                        if (!skipName)
                        {
                            if (readIconOverlaysNow) // nove ikonky/thumbnaily pro zvolenou oblast (viz 'selectMode') jsou nactene, ted nacitame icon-overlays
                            {
                                CFileData* fileData = i < window->Dirs->Count ? &window->Dirs->At(i) : &window->Files->At(i - window->Dirs->Count);
                                if (fileData->IconOverlayDone == 0 && (i > 0 || strcmp(fileData->Name, "..") != 0))
                                {
                                    fileData->IconOverlayDone = 1; // oznacime si, ze tento icon-overlay uz jsme ziskavali, at to zbytecne nedelame znovu

                                    char fileName[MAX_PATH];
                                    DWORD fileAttrs = fileData->Attr;
                                    memcpy(fileName, fileData->Name, fileData->NameLen + 1);
                                    int minPriority = 100;
                                    if (i >= window->Dirs->Count && fileData->IsLink || // soubor je link
                                        fileData->IsOffline ||                          // soubor nebo adresar je offline (slow)
                                        i < window->Dirs->Count && fileData->Shared)    // adresar je sharovany
                                    {
                                        minPriority = 9; // overlaye pro link, share a slow files (offline) maji prioritu 10, takze bereme jen overlaye s vyssi prioritou (nizsi cislo nez 10)
                                    }

                                    if (window->ICSleep)
                                        goto GO_SLEEP_MODE;
                                    HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                                    // nechame nacist ikonu ze souboru, icon-reader behem nacitani muze prejit do sleep-modu
                                    *name = 0;
                                    //                    TRACE_I("Getting icon overlay index for: " << fileName << "...");
                                    SLOW_CALL_STACK_MESSAGE5("IconThreadThreadFBody::GetIconOverlayIndex(%s%s, 0x%08X, %d)",
                                                             path, fileName, fileAttrs, isGoogleDrivePath);
                                    DWORD iconOverlayIndex = ShellIconOverlays.GetIconOverlayIndex(wPath, wName, path, name,
                                                                                                   fileName, fileAttrs,
                                                                                                   minPriority, iconReadersIconOverlayIds,
                                                                                                   isGoogleDrivePath);
                                    //                    TRACE_I("Getting icon overlay index is done.");

                                    HANDLES(EnterCriticalSection(&window->ICSleepSection));
                                    if (window->ICSleep)
                                        goto GO_SLEEP_MODE; // panel uz chce prejit do sleep-modu

                                    CFileData* fileDataCheck = i < window->Dirs->Count ? &window->Dirs->At(i) : i < window->Files->Count + window->Dirs->Count ? &window->Files->At(i - window->Dirs->Count)
                                                                                                                                                               : NULL;
                                    if (fileData != fileDataCheck || strcmp(fileName, fileData->Name) != 0)
                                    {
                                        if (fileData != fileDataCheck)
                                            TRACE_E("IconThreadThreadFBody::GetIconOverlayIndex: PRUSER!!! (fileData != fileDataCheck)");
                                        else
                                            TRACE_E("IconThreadThreadFBody::GetIconOverlayIndex: PRUSER!!! (file name changed)");
                                    }
                                    else
                                    {
                                        BOOL redraw = fileData->IconOverlayIndex != iconOverlayIndex;
                                        fileData->IconOverlayIndex = iconOverlayIndex;

                                        int visArrVer;
                                        BOOL visArrValid;
                                        if (redraw && // je potreba prekreslit index (zmena icon-overlaye)
                                            (window->VisibleItemsArray.ArrContainsIndex(i, &visArrValid, &visArrVer) || !visArrValid))
                                        { // pokud vime, ze je polozka videt nebo pokud nevime nic o viditelnosti polozek, nechame index prekreslit
                                            PostMessage(window->HWindow, WM_USER_REFRESHINDEX2, i, 0);
                                        }
                                    }
                                }
                                else
                                    callWaitForObjects = FALSE; // zadna prace -> zadne zdrzovani
                            }
                            else
                            {
                                if (iconData->GetReadingDone() == 0 &&
                                    iconData->GetFlag() == wanted)
                                {
                                    iconData->SetReadingDone(1);    // oznacime si, ze s touto ikonou uz jsme pracovali, at to zbytecne nezkousime znovu jeste behem tohoto cyklu
                                    if (wanted == 0 || wanted == 2) // nacitani ikonek primo ze souboru nebo z plug-inu
                                    {
                                        if (!pluginFSIconsFromPlugin) // ikona na disku
                                        {
                                            if (strlen(iconData->NameAndData) + (name - path) < MAX_PATH)
                                            {
                                                strcpy(name, iconData->NameAndData);

                                                if (window->ICSleep)
                                                    goto GO_SLEEP_MODE;
                                                HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                                                if (waitBeforeFirstReadIcon)
                                                {
                                                    waitBeforeFirstReadIcon = FALSE;
                                                    //                            TRACE_I("Waiting 500ms before reading first icon in second round to have bigger chance to succeed.");
                                                    Sleep(500); // dame si chvili oraz (pred druhym pokusem o nacteni ikony)
                                                }

                                                // nechame nacist ikonu ze souboru, icon-reader behem nacitani muze prejit do sleep-modu
                                                CALL_STACK_MESSAGE3("IconThreadThreadFBody::GetFileIcon(%s, %d)", path, iconSize);

                                                if (!pathIsInvalid)
                                                {
                                                    //                            TRACE_I("Getting icon for: " << name << "...");
                                                    IconThreadThreadFBodyAux(path, shi, iconSize);
                                                    if (shi.hIcon == NULL)
                                                        TRACE_I("Unable to get icon from: " << path);
                                                    //                            else
                                                    //                              TRACE_I("Getting icon is done.");
                                                }
                                                else
                                                {
                                                    shi.hIcon = NULL;
                                                }

                                                HANDLES(EnterCriticalSection(&window->ICSleepSection));
                                            }
                                            else
                                            {
                                                shi.hIcon = NULL;
                                                *name = 0;
                                                TRACE_I("Too long filename to get icon from: " << path << iconData->NameAndData);
                                            }
                                        }
                                        else // ikona v plug-inovem FS - nacitani je neprerusitelne (hrozi zruseni PluginData)
                                        {
                                            const CFileData* f = iconData->GetFSFileData();
                                            if (f != NULL)
                                            {
                                                shi.hIcon = window->PluginData.GetPluginIcon(f, iconSize, destroyPluginIcon);
                                                if (shi.hIcon == NULL)
                                                {
                                                    TRACE_I("Unable to get icon from FS item: " << iconData->NameAndData);
                                                }
                                            }
                                            else
                                            {
                                                shi.hIcon = NULL;
                                                TRACE_E("Unexpected error: Icon Cache doesn't contain FSFileData for item from FS with "
                                                        "pitFromPlugin icon type! Item: "
                                                        << iconData->NameAndData);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (wanted == 3) // nacitani ikonek z icon-location
                                        {
                                            shi.hIcon = NULL;
                                            char* nameAndData = iconData->NameAndData;
                                            int size = (int)strlen(nameAndData) + 4;
                                            size -= (size & 0x3);         // size % 4  (zarovnani po ctyrech bytech)
                                            char* s = nameAndData + size; // preskok zarovnani z nul
                                            BOOL doExtractIcons = FALSE;
                                            BOOL doLoadImage = FALSE;
                                            int index = -1;
                                            char* num = strrchr(s, ','); // cislo ikony je za posledni carkou
                                            if (num != NULL)
                                            {
                                                *num = 0;
                                                index = atoi(num + 1);
                                                if (strlen(s) < MAX_PATH)
                                                {
                                                    strcpy(path, s);
                                                    doExtractIcons = TRUE;
                                                    //                            TRACE_I("ExtractIcons for: " << nameAndData << "...");
                                                }
                                                else
                                                    TRACE_I("Too long filename to get icon from: " << s << ", " << index);
                                                *num = ',';
                                            }
                                            else
                                            {
                                                if (strlen(s) < MAX_PATH)
                                                {
                                                    strcpy(path, s);
                                                    doLoadImage = TRUE;
                                                    //                            TRACE_I("LoadImage for: " << nameAndData << "...");
                                                }
                                                else
                                                    TRACE_I("Too long filename to get icon from: " << s);
                                            }

                                            if (window->ICSleep)
                                                goto GO_SLEEP_MODE;
                                            HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                                            if (waitBeforeFirstReadIcon)
                                            {
                                                waitBeforeFirstReadIcon = FALSE;
                                                //                          TRACE_I("Waiting 500ms before reading first icon in second round to have bigger chance to succeed.");
                                                Sleep(500); // dame si chvili oraz (pred druhym pokusem o nacteni ikony)
                                            }

                                            if (doExtractIcons)
                                            {
                                                // nechame nacist ikonu ze souboru (z resourcu vytahne podle indexu), icon-reader
                                                // behem nacitani muze prejit do sleep-modu
                                                CALL_STACK_MESSAGE4("IconThreadThreadFBody::ExtractIcons(%s, %d, %d, ...)", path, index, IconSizes[iconSize]);
                                                if (ExtractIcons(path, index, IconSizes[iconSize], IconSizes[iconSize], &shi.hIcon, NULL, 1, IconLRFlags) != 1)
                                                {
                                                    TRACE_I("Unable to get icon from: " << path << ", " << index);
                                                    shi.hIcon = NULL;
                                                }
                                                //                          else
                                                //                            TRACE_I("ExtractIcons is done.");
                                            }

                                            if (doLoadImage)
                                            {
                                                {
                                                    // nechame nacist ikonu ze souboru (nejspis .ico), icon-reader behem nacitani
                                                    // muze prejit do sleep-modu
                                                    CALL_STACK_MESSAGE2("IconThreadThreadFBody::LoadImage(%s)", path);
                                                    shi.hIcon = (HICON)NOHANDLES(LoadImage(NULL, path, IMAGE_ICON, IconSizes[iconSize], IconSizes[iconSize],
                                                                                           LR_LOADFROMFILE | IconLRFlags));
                                                    //                            TRACE_I("LoadImage " << (shi.hIcon == NULL ? "has failed, now trying ExtractIcons..." : "is done."));
                                                }
                                                if (shi.hIcon == NULL) // pres LoadImage se to nepovedlo, zkusime jeste ExtractIcons (napr. ikona bez indexu ze zipfldr.dll pod XP: .zip archiv zabaleny v .7z archivu)
                                                {
                                                    // nechame nacist prvni ikonu ze souboru, icon-reader behem nacitani muze prejit do sleep-modu
                                                    CALL_STACK_MESSAGE3("IconThreadThreadFBody::ExtractIcons(%s, (0), %d, ...)", path, IconSizes[iconSize]);
                                                    if (ExtractIcons(path, 0, IconSizes[iconSize], IconSizes[iconSize], &shi.hIcon, NULL, 1, IconLRFlags) != 1)
                                                    {
                                                        TRACE_I("Unable to get first icon from: " << path);
                                                        shi.hIcon = NULL;
                                                    }
                                                    //                            else
                                                    //                              TRACE_I("ExtractIcons is done.");
                                                }
                                            }

                                            HANDLES(EnterCriticalSection(&window->ICSleepSection));
                                        }
                                        else // wanted == 4 nebo 6; nacitani thumbnailu z plug-inu ("thumbnail loader")
                                        {
                                            shi.hIcon = NULL; // opatreni proti chybne dealokaci ikony (zadna tu nevznika)

                                            char* s = iconData->NameAndData;
                                            int len = (int)strlen(s);
                                            int size = len + 4;
                                            size -= (size & 0x3); // size % 4  (zarovnani po ctyrech bytech)
                                            if (strlen(s) + (name - path) < MAX_PATH)
                                            {
                                                strcpy(name, s);

                                                //                          TRACE_I("Load thumbnail for: " << name << "...");
                                                CPluginInterfaceForThumbLoaderEncapsulation** loader;
                                                loader = (CPluginInterfaceForThumbLoaderEncapsulation**)(s + size + sizeof(CQuadWord) + sizeof(FILETIME));
                                                while (*loader != NULL)
                                                {
                                                    int thumbnailSize = window->GetThumbnailSize();
                                                    thumbMaker.Clear(thumbnailSize);
                                                    CALL_STACK_MESSAGE3("IconThreadThreadFBody::LoadThumbnail(%s, %d)", path, wanted == 4);
                                                    if ((*loader)->LoadThumbnail(path, thumbnailSize, thumbnailSize, &thumbMaker, wanted == 4))
                                                    {
                                                        thumbnailFlag = wanted == 4 /* prvni kolo nacitani thumbnailu */ ? (thumbMaker.IsOnlyPreview() ? 6 /* nekvalitni/mensi */ : 5 /* kvalitni */) : 5 /* v druhem kole uz jsou vsechny ziskane thumbnaily kvalitni */;
                                                        thumbMaker.HandleIncompleteImages();
                                                        break; // thumbnail je mozna nacteny (kazdopadne se nema zkouset dalsi plugin)
                                                    }
                                                    loader++; // zkusime dalsi plugin v rade, treba thumbnail nacte
                                                }
                                                if (*loader == NULL)
                                                    thumbMaker.Clear(); // nepovedeny thumbnail -> radsi udelame cistku
                                                                        //                          TRACE_I("Load thumbnail is done.");
                                            }
                                            else
                                            {
                                                *name = 0;
                                                TRACE_I("Too long filename to get thumbnail from: " << path << s);
                                                thumbMaker.Clear();
                                            }
                                        }
                                    }

                                    if (window->ICSleep) // panel uz chce prejit do sleep-modu
                                    {
                                        thumbMaker.Clear(); // thumbnail uz nebude potreba

                                        // pokud to neni ikona z plug-inu, ktery si nepreje ruseni ikony, zrusime ikonu
                                        if (shi.hIcon != NULL && (!pluginFSIconsFromPlugin || destroyPluginIcon))
                                        {
                                            ::NOHANDLES(DestroyIcon(shi.hIcon));
                                        }
                                        goto GO_SLEEP_MODE;
                                    }

                                    if (wanted <= 3) // ziskavali jsme ikonu
                                    {
                                        if (shi.hIcon == NULL)
                                            failed = TRUE;
                                        else
                                        {
                                            if (window->IconCache->GetIcon(iconData->GetIndex(),
                                                                           &iconList, &iconListIndex))
                                            {
                                                HANDLES(EnterCriticalSection(&window->ICSectionUsingIcon));

                                                iconList->ReplaceIcon(iconListIndex, shi.hIcon);
                                                iconData->SetFlag(1); // uz je nactena

                                                HANDLES(LeaveCriticalSection(&window->ICSectionUsingIcon));

                                                // najdeme index polozky, ktere jsme nacetli ikonu

                                                if (pluginFSIconsFromPlugin) // pitFromPlugin: nechame plugin, aby polozky porovnal sam (musi jit o porovnani beze shod zadnych dvou polozek listingu)
                                                {
                                                    const CFileData* file = iconData->GetFSFileData();
                                                    if (file != NULL)
                                                    {
                                                        CPluginDataInterfaceEncapsulation* dataIface = &window->PluginData;
                                                        CFilesArray* arr = window->Dirs;
                                                        int z;
                                                        for (z = 0; z < arr->Count; z++)
                                                        {
                                                            if (dataIface->CompareFilesFromFS(file, &arr->At(z)) == 0)
                                                            {
                                                                PostMessage(window->HWindow, WM_USER_REFRESHINDEX, z, 0);
                                                                break;
                                                            }
                                                        }
                                                        if (z == window->Dirs->Count) // nebyl to adresar
                                                        {
                                                            arr = window->Files;
                                                            int j;
                                                            for (j = 0; j < arr->Count; j++)
                                                            {
                                                                if (dataIface->CompareFilesFromFS(file, &arr->At(j)) == 0)
                                                                {
                                                                    PostMessage(window->HWindow, WM_USER_REFRESHINDEX,
                                                                                window->Dirs->Count + j, 0);
                                                                    break;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                else // nehrozi duplicitni jmena (nebo nejsou na prekazku jako napr. u archivu, kde
                                                {    // nemuzou byt ruzne ikony pri shodnych jmenech)
                                                    char* name2 = iconData->NameAndData;
                                                    CFilesArray* arr = window->Dirs;
                                                    int z;
                                                    for (z = 0; z < arr->Count; z++)
                                                    {
                                                        if (strcmp(name2, arr->At(z).Name) == 0)
                                                        {
                                                            PostMessage(window->HWindow, WM_USER_REFRESHINDEX, z, 0);
                                                            break;
                                                        }
                                                    }
                                                    if (z == window->Dirs->Count) // nebyl to adresar
                                                    {
                                                        arr = window->Files;
                                                        int j;
                                                        for (j = 0; j < arr->Count; j++)
                                                        {
                                                            if (strcmp(name2, arr->At(j).Name) == 0)
                                                            {
                                                                PostMessage(window->HWindow, WM_USER_REFRESHINDEX,
                                                                            window->Dirs->Count + j, 0);
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            // pokud to neni ikona z plug-inu, ktery si nepreje ruseni ikony, zrusime ikonu
                                            if (!pluginFSIconsFromPlugin || destroyPluginIcon)
                                            {
                                                ::NOHANDLES(DestroyIcon(shi.hIcon));
                                            }
                                        }
                                    }
                                    else // ziskavali jsme thumbnail
                                    {
                                        if (thumbMaker.ThumbnailReady())
                                        {
                                            CThumbnailData* thumbnailData;
                                            if (window->IconCache->GetThumbnail(iconData->GetIndex(),
                                                                                &thumbnailData))
                                            {
                                                BOOL thumbnailCreated = FALSE;

                                                HANDLES(EnterCriticalSection(&window->ICSectionUsingThumb));
                                                thumbMaker.TransformThumbnail();
                                                if (thumbMaker.RenderToThumbnailData(thumbnailData))
                                                {
                                                    iconData->SetFlag(thumbnailFlag); // uz je nacteny
                                                    if (thumbnailFlag == 6 /* nekvalitni/mensi thumbnail v prvnim kole nacitani thumbnailu*/)
                                                        iconData->SetReadingDone(0); // bude nasledovat druhe kolo cteni, takze jestli neni "done"
                                                    thumbnailCreated = TRUE;
                                                }
                                                HANDLES(LeaveCriticalSection(&window->ICSectionUsingThumb));

                                                if (thumbnailCreated)
                                                {
                                                    // najdeme index souboru (adresare nemaji thubnaily), kteremu jsme nacetli thumbnail
                                                    char* name2 = iconData->NameAndData;
                                                    int z;
                                                    for (z = 0; z < window->Files->Count; z++)
                                                    {
                                                        if (strcmp(name2, window->Files->At(z).Name) == 0)
                                                        {
                                                            PostMessage(window->HWindow, WM_USER_REFRESHINDEX,
                                                                        window->Dirs->Count + z, 0);
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        thumbMaker.Clear(); // thumbnail uz nebude potreba
                                    }
                                }
                                else
                                    callWaitForObjects = FALSE; // zadna prace -> zadne zdrzovani
                            }
                        }
                        else
                        {
                            someNameSkipped = TRUE;     // preskocili jsme aspon jedno jmeno
                            callWaitForObjects = FALSE; // zadna prace -> zadne zdrzovani
                        }
                    }
                    else
                    {
                        if (canReadIconOverlays && !readIconOverlaysNow)
                        { // ted jdeme cist icon-overlaye
                            i = 0;
                            readIconOverlaysNow = TRUE;
                            //                TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                            continue;
                        }
                        else
                        {
                            readIconOverlaysNow = FALSE;
                            //                TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                        }

                        if (!readOnlyVisibleItems && (selectMode == 2 || selectMode == 3))
                        {
                            i = 0;
                            selectMode++;
                            //                TRACE_I("selectMode=" << selectMode);
                            continue;
                        }

                        // prvni kolo cteni ikon je za nami, takze vsechny icon-overlaye uz jsou nactene -> zamezime zbytecnemu snazeni o jejich dalsi cteni
                        canReadIconOverlays = FALSE;

                        // poradi nacitani: nove ikony, nove thumbnaily, stare ikony, stare thumbnaily
                        BOOL done = FALSE; // TRUE == breakni, uz mame nacteno
                        switch (wanted)
                        {
                        case 0: // nove ikony uz jsme nacetli
                        {
                            // pokud se maji cist thumbnaily a jde o prvni kolo cteni (pluginy nefunguji
                            // nahodne jako system, takze nenactou poprve = nenactou nikdy), nacteme
                            // nove thumbnaily (wanted == 4)
                            if (readThumbnails && firstRound)
                                wanted = 4;
                            else
                                wanted = 2; // jinak obnovime (nacteme znovu) stare (prevzate) ikony
                            break;
                        }

                        case 4: // nove thumbnaily uz jsme nacetli
                        {
                            wanted = 2; // obnovime (nacteme znovu) stare (prevzate) ikony
                            break;
                        }

                        case 2: // stare ikony uz jsme nacetli
                        {
                            if (readThumbnails && firstRound)
                                wanted = 6; // obnovime (nacteme znovu) stare (prevzate + nekvalitni/mensi) thumbnaily
                            else
                                done = TRUE;
                            break;
                        }

                        default:
                            done = TRUE;
                            break;
                        }
                        if (done)
                            break; // hotovo - wanted 0 a 2 nebo 0, 4, 2 a 6 nebo jen 3 nebo -1 (chyba)

                        //              TRACE_I("wanted=" << wanted);

                        if (selectMode == 4)
                        {
                            i = 0;
                            selectMode = 2;
                            //                TRACE_I("selectMode=" << selectMode);
                            readIconOverlaysNow = FALSE;
                            //                TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                            continue;
                        }

                        i = -1;                     // aby se 'i' dostalo na nulu
                        callWaitForObjects = FALSE; // zadna prace -> zadne zdrzovani
                    }

                    i++;
                    if (callWaitForObjects)
                    {
                        wait = WaitForMultipleObjects(2, handles, FALSE, 0);
                        // nebudeme ignorovat signal "work", protoze kazdy "sleep->wake-up" znamena zacit praci od zacatku
                        if (wait != WAIT_TIMEOUT)
                            break; // zpracuj wait udalost
                    }
                    // else wait = WAIT_TIMEOUT;  // zbytecne, wait uz je roven WAIT_TIMEOUT
                }
                repeatedRound = FALSE;

                if (wait == WAIT_TIMEOUT && readOnlyVisibleItemsDueToUMI)
                { // nemusi byt nactene vsechny ikony kvuli poskytnute priorite pro ikony do usermenu (ctou se drive nez ikony mimo viditelnou plochu panelu)
                    if (UserMenuIconBkgndReader.IsReadingIcons())
                    {
                        //              TRACE_I("Visible icons done, giving priority to usermenu icons...");
                        while (1)
                        {
                            if (window->ICSleep)
                                goto GO_SLEEP_MODE;
                            HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                            wait = WaitForMultipleObjects(2, handles, FALSE, 100); // dame cas pro nacitani ikon usermenu

                            HANDLES(EnterCriticalSection(&window->ICSleepSection));
                            if (window->ICSleep)
                                goto GO_SLEEP_MODE; // panel uz chce prejit do sleep-modu

                            if (wait != WAIT_TIMEOUT)
                            {
                                //                  TRACE_I("Handling event...");
                                break; // zpracuj wait udalost
                            }
                            int visArrVer; // zkontrolujeme, jestli se nezmenila viditelna oblast panelu, pokud ano, musime jit cist ikony
                            if (someNameSkipped && window->VisibleItemsArray.IsArrValid(&visArrVer) && visArrVer != lastVisArrVersion)
                            {
                                //                  TRACE_I("Change of visible items array...");
                                break;
                            }
                            if (!UserMenuIconBkgndReader.IsReadingIcons())
                            {
                                //                  TRACE_I("Usermenu icons done...");
                                break; // pokud uz jsou ikony usermenu hotove, docteme ikony v panelu
                            }
                        }
                    }
                    if (wait == WAIT_TIMEOUT) // je duvod zopakovat cteni ikon (zmena viditelne oblasti nebo dokonceni cteni ikon usermenu)
                    {
                        if (!UserMenuIconBkgndReader.IsReadingIcons()) // jsou-li ikony usermenu hotove, docteme ikony mimo viditelnou plochu
                        {
                            //                if (someNameSkipped) TRACE_I("Usermenu icons done, going to read the rest of icons in panel...");
                            readOnlyVisibleItems = FALSE;
                            readOnlyVisibleItemsDueToUMI = FALSE;
                        }
                        //              else
                        //                if (someNameSkipped) TRACE_I("Going to reread visible icons in panel...");
                        if (someNameSkipped)
                        {
                            repeatedRound = TRUE; // jde o kolo navic (nechceme, aby se znovu cetly ikon-overlays)
                            goto SECOND_ROUND;
                        }
                        //              else
                        //                TRACE_I("All items in panel are visible, so no reason to reread icons...");
                    }
                }

                if (wait == WAIT_TIMEOUT) // work is done -> informuj hl. thread
                {
                    if (window->Is(ptDisk) && failed && firstRound)
                    {                                   // dame si to znovu (ne vsechny ikonky se povedly)
                        firstRound = FALSE;             // jen jedno kolo navic
                        waitBeforeFirstReadIcon = TRUE; // aby se ikona necetla okamzite znovu (mala sance na uspech)
                                                        //              TRACE_I("Going to second round of reading (some icons have not been read in the first round).");
                        goto SECOND_ROUND;
                        // postRefresh = TRUE;
                    }
                    else
                        firstRound = TRUE;

                    //            TRACE_I("Stop reading.");
                    // posleme notifikaci o ukonceni nacitani ikonek v panelu
                    if (window->HWindow == NULL ||
                        !PostMessage(window->HWindow, WM_USER_ICONREADING_END, 0, 0))
                    { // neco nevyslo ("always false"), nastavime IconCacheValid = TRUE tady
                        window->IconCacheValid = TRUE;
                    }

                    //            if (window->HWindow != NULL)  // prubezne prekreslovani staci
                    //              InvalidateRect(window->HWindow, NULL, TRUE);
                }
                else
                {

                GO_SLEEP_MODE:

                    // preruseni (sleep-icon-cache-thread nebo nova prace nebo terminate)
                    firstRound = TRUE;
                    //            TRACE_I("Reading terminated.");
                }

                window->ICWorking = FALSE;
            }

            window->ICSleep = FALSE;
            HANDLES(LeaveCriticalSection(&window->ICSleepSection));

            /*    // nahrazeno pres goto SECOND_ROUND (na sitovem disku to zamrzi, kdyz se zacne cely obsah cist znovu)
        if (postRefresh)  // odsun Sleep(500) z kriticke sekce - zbytecne tuhlo ...
        {
          HANDLES(EnterCriticalSection(&TimeCounterSection));  // sejmeme cas, kdy je treba refreshe
          int t1 = MyTimeCounter++;
          HANDLES(LeaveCriticalSection(&TimeCounterSection));
          Sleep(500);  // chvilka na vydech
          PostMessage(window->HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }
*/

            break;
        }

        default: // terminate
        {
            run = FALSE;
            break;
        }
        }
    }

    ShellIconOverlays.ReleaseIconReadersIconOverlayIds(iconReadersIconOverlayIds);

    OleUninitialize();

    TRACE_I("End");
    return 0;
}

unsigned IconThreadThreadFEH(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return IconThreadThreadFBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread IconReader: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI IconThreadThreadF(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return IconThreadThreadFEH(param);
}

CFilesWindow::CFilesWindow(CMainWindow* parent)
    : Columns(20, 10), ColumnsTemplate(20, 10), VisibleItemsArray(FALSE), VisibleItemsArraySurround(TRUE)
{
    CALL_STACK_MESSAGE1("CFilesWindow::CFilesWindow()");
    NarrowedNameColumn = FALSE;
    FullWidthOfNameCol = 0;
    WidthOfMostOfNames = 0;
    ColumnsTemplateIsForDisk = FALSE; // jen nulovani, nastavi se v BuildColumnsTemplate()
    StopThumbnailLoading = FALSE;
    UserWorkedOnThisPath = FALSE;

    UnpackedAssocFiles.SetPanel(this);
    QuickRenameWindow.SetPanel(this);

    FilesMap.SetPanel(this);
    ScrollObject.SetPanel(this);
    HiddenDirsFilesReason = 0;
    HiddenDirsCount = HiddenFilesCount = 0;
    IconCacheValid = FALSE;
    InactWinOptimizedReading = FALSE;
    WaitBeforeReadingIcons = 0;
    WaitOneTimeBeforeReadingIcons = 0;
    EndOfIconReadingTime = GetTickCount() - 10000;
    ICEventTerminate = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    ICEventWork = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    ICSleep = FALSE;
    ICWorking = FALSE;
    ICStopWork = FALSE;
    HANDLES(InitializeCriticalSection(&ICSleepSection));
    HANDLES(InitializeCriticalSection(&ICSectionUsingIcon));
    HANDLES(InitializeCriticalSection(&ICSectionUsingThumb));
    DWORD ThreadID;
    IconCacheThread = NULL;
    if (ICEventTerminate != NULL && ICEventWork != NULL)
        IconCacheThread = HANDLES(CreateThread(NULL, 0, IconThreadThreadF, this, 0, &ThreadID));
    if (ICEventTerminate == NULL ||
        ICEventWork == NULL ||
        IconCacheThread == NULL)
    {
        TRACE_E("Unable to start icon-reader thread.");
        IconCache = NULL;
    }
    else
    {
        //    SetThreadPriority(IconCacheThread, THREAD_PRIORITY_IDLE); // nefunguje pak nacitani
        IconCache = new CIconCache();
    }

    OpenedDrivesList = NULL;

    Parent = parent;
    ViewTemplate = &parent->ViewTemplates.Items[2]; // detailed view
    BuildColumnsTemplate();
    CopyColumnsTemplateToColumns();
    ListBox = NULL;
    StatusLine = NULL;
    DirectoryLine = NULL;
    StatusLineVisible = TRUE;
    DirectoryLineVisible = TRUE;
    HeaderLineVisible = TRUE;

    SortType = stName;
    ReverseSort = FALSE;
    SortedWithRegSet = FALSE;    // na uvodnim stavu nezalezi, nastavi se v metode SortDirectory()
    SortedWithDetectNum = FALSE; // na uvodnim stavu nezalezi, nastavi se v metode SortDirectory()
    LastFocus = INT_MAX;
    SetValidFileData(VALID_DATA_ALL);
    AutomaticRefresh = TRUE;
    NextFocusName[0] = 0;
    DontClearNextFocusName = FALSE;
    LastRefreshTime = 0;
    FilesActionInProgress = FALSE;
    CanDrawItems = TRUE;
    FileBasedCompression = FALSE;
    FileBasedEncryption = FALSE;
    SupportACLS = FALSE;
    DeviceNotification = NULL;
    ContextMenu = NULL;
    ContextSubmenuNew = new CMenuNew;
    UseSystemIcons = FALSE;
    UseThumbnails = FALSE;
    NeedRefreshAfterEndOfSM = FALSE;
    RefreshAfterEndOfSMTime = 0;
    PluginFSNeedRefreshAfterEndOfSM = FALSE;
    SmEndNotifyTimerSet = FALSE;
    RefreshDirExTimerSet = FALSE;
    RefreshDirExLParam = 0;
    InactiveRefreshTimerSet = FALSE;
    InactRefreshLParam = 0;
    LastInactiveRefreshStart = LastInactiveRefreshEnd = 0;

    NeedRefreshAfterIconsReading = FALSE;
    RefreshAfterIconsReadingTime = 0;

    PathHistory = new CPathHistory();

    DontDrawIndex = -1;
    DrawOnlyIndex = -1;

    FocusFirstNewItem = FALSE;

    ExecuteAssocEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
    AssocUsed = FALSE;

    FilterEnabled = FALSE;
    Filter.SetMasksString("*.*");
    int errPos;
    Filter.PrepareMasks(errPos);

    QuickSearchMode = FALSE;
    CaretHeight = 1; // dummy
    QuickSearch[0] = 0;
    QuickSearchMask[0] = 0;
    SearchIndex = INT_MAX;
    FocusedIndex = 0;
    FocusVisible = FALSE;

    DropTargetIndex = -1;
    SingleClickIndex = -1;
    SingleClickAnchorIndex = -1;
    GetCursorPos(&OldSingleClickMousePos);

    TrackingSingleClick = FALSE;
    DragBox = FALSE;
    DragBoxVisible = FALSE;
    ScrollingWindow = FALSE;

    SkipCharacter = FALSE;
    SkipSysCharacter = FALSE;

    //  ShiftSelect = FALSE;
    DragSelect = FALSE;
    BeginDragDrop = FALSE;
    DragDropLeftMouseBtn = FALSE;
    BeginBoxSelect = FALSE;
    PersistentTracking = FALSE;

    TrackingSingleClick = 0;

    CutToClipChanged = FALSE;

    PerformingDragDrop = FALSE;

    GetPluginIconIndex = InternalGetPluginIconIndex;

    EnumFileNamesSourceUID = -1;

    TemporarilySimpleIcons = FALSE;
    NumberOfItemsInCurDir = 0;

    NeedIconOvrRefreshAfterIconsReading = FALSE;
    LastIconOvrRefreshTime = GetTickCount() - ICONOVR_REFRESH_PERIOD;
    IconOvrRefreshTimerSet = FALSE;
}

CFilesWindow::~CFilesWindow()
{
    CALL_STACK_MESSAGE1("CFilesWindow::~CFilesWindow()");

    if (DeviceNotification != NULL)
        TRACE_E("CFilesWindow::~CFilesWindow(): unexpected situation: DeviceNotification != NULL");

    ClearHistory();

    if (PathHistory != NULL)
        delete PathHistory;

    if (IconCacheThread != NULL)
    {
        SetEvent(ICEventTerminate); // nacitaci ikonek terminuj se !
        if (WaitForSingleObject(IconCacheThread, 1000) == WAIT_TIMEOUT)
        { // ma sekundu na jednoduchy odchod, pak nutny kill (window se dealokuje)
            TRACE_E("Terminating Icon Thread");
            TerminateThread(IconCacheThread, 666);
            WaitForSingleObject(IconCacheThread, INFINITE); // pockame az thread skutecne skonci, nekdy mu to dost trva
        }
        HANDLES(CloseHandle(IconCacheThread));
    }

    HANDLES(DeleteCriticalSection(&ICSectionUsingThumb));
    HANDLES(DeleteCriticalSection(&ICSectionUsingIcon));
    HANDLES(DeleteCriticalSection(&ICSleepSection));
    if (ICEventTerminate != NULL)
        HANDLES(CloseHandle(ICEventTerminate));
    if (ICEventWork != NULL)
        HANDLES(CloseHandle(ICEventWork));

    if (IconCache != NULL)
        delete IconCache;
    if (ContextSubmenuNew != NULL)
        delete ContextSubmenuNew;
    if (ExecuteAssocEvent != NULL)
        HANDLES(CloseHandle(ExecuteAssocEvent));
}

void CFilesWindow::ClearHistory()
{
    if (PathHistory != NULL)
        PathHistory->ClearHistory();

    OldSelection.Clear();
}

void CFilesWindow::SleepIconCacheThread()
{
    CALL_STACK_MESSAGE1("CFilesWindow::SleepIconCacheThread()");
    ICSleep = TRUE;          // pro preruseni smycky nacitani ikon (ICSleepSection nemusi vubec opustit)
    ICStopWork = TRUE;       // pro preruseni smycky nacitani ikon pokud uz je ICStopWork zpracovany
    ResetEvent(ICEventWork); // pro preruseni smycky nacitani ikon pokud jeste neni ICStopWork zpracovany
    // pockame az icon-reader vstoupi do casti, ve ktere se da prejit do sleep-modu
    HANDLES(EnterCriticalSection(&ICSleepSection));
    ICSleep = ICWorking; // TRUE jen pokud icon-reader visi v SHGetFileInfo
    HANDLES(LeaveCriticalSection(&ICSleepSection));
}

void CFilesWindow::WakeupIconCacheThread()
{
    CALL_STACK_MESSAGE_NONE
    ICStopWork = FALSE;    // aby se prace hned nazacatku neprerusila
    SetEvent(ICEventWork); // prejdi do work modu, na reakci necekame
    MSG msg;               // musime zlikvidovat pripadnou WM_USER_ICONREADING_END, ktera by nastavila IconCacheValid = TRUE
    while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
        ;
}

BOOL CFilesWindow::CheckAndRestorePath(const char* path)
{
    CALL_STACK_MESSAGE2("CFilesWindow::CheckAndRestorePath(%s)", path);

    // sitove cesty nebudeme testovat, pokud jsme na ne zrovna pristupovali
    BOOL tryNet = (!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath());

    return SalCheckAndRestorePath(HWindow, path, tryNet);
}

BOOL CFilesWindow::CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin)
{
    CALL_STACK_MESSAGE1("CFilesWindow::CanUnloadPlugin()");

    if (Is(ptDisk))
    {
        if (UseThumbnails && // nacitaji se thumbnaily
            !IconCacheValid) // icon-reader jeste neskoncil nacitani
        {
            CPluginData* p = Plugins.GetPluginData(plugin);
            if (p != NULL) // "always true"
            {
                if (p->ThumbnailMasks.GetMasksString()[0] != 0)
                { // jde o plugin, ktery poskytuje thumbnaily - nevime jiste, jestli i pro
                    // tento panel, ale vyloucit se to neda, takze musime zastavit cteni ikon
                    SleepIconCacheThread();
                    p->ThumbnailMasksDisabled = TRUE; // behem unload/remove pluginu nelze tento plugin pouzit pro load thumbnailu
                    StopThumbnailLoading = TRUE;      // pro pripad, ze by se odnekud zavolal WakeupIconCacheThread (data o "thumbnail-loaderech" v icon-cache nelze pouzivat)
                    UseThumbnails = FALSE;            // aby nenastal nechteny wake-up icon-readeru (volani WakeupIconCacheThread())
                    if (!CriticalShutdown)
                    {
                        HANDLES(EnterCriticalSection(&TimeCounterSection));
                        int t1 = MyTimeCounter++;
                        HANDLES(LeaveCriticalSection(&TimeCounterSection));
                        PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1); // postarame se o nove naplneni icon-cache (idealne probehne az po unloadu/remove pluginu)
                    }
                }
            }
            else
                TRACE_E("CFilesWindow::CanUnloadPlugin(): Unexpected situation!");
        }
    }
    else
    {
        BOOL used = FALSE;
        if ((Is(ptZIPArchive) || Is(ptPluginFS)) &&
            PluginData.NotEmpty() && PluginData.GetPluginInterface() == plugin)
            used = TRUE;
        else
        { // FS nemusi pouzivat PluginData, proto musime jeste testnout PluginFS
            if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
                GetPluginFS()->GetPluginInterface() == plugin)
                used = TRUE;
            else
            {
                if (Is(ptZIPArchive))
                { // archiv nemusi pouzivat PluginData, proto musime jeste testnout asociace archivu
                    // tato cast je dulezita jen pro ukoncovani Salamandera - jinak plug-in by se klidne
                    // mohl unloadnout behem pouziti archivatoru (kazda funkce archivatoru si plug-in naloadi)
                    // POZOR: vyjimkou jsou icon-overlays z pluginu, po unloadu pluginu by se prestaly kreslit
                    //        (pri unloadu pluginu uvolnujeme jeho pole icon-overlays)
                    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
                    if (format != 0) // nasli jsme podporovany archiv
                    {
                        format--;
                        CPluginData* data;
                        int index = PackerFormatConfig.GetUnpackerIndex(format);
                        if (index < 0) // view: jde o interni zpracovani (plug-in)?
                        {
                            data = Plugins.Get(-index - 1);
                            if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                                used = TRUE;
                        }
                        if (PackerFormatConfig.GetUsePacker(format)) // ma edit?
                        {
                            index = PackerFormatConfig.GetPackerIndex(format);
                            if (index < 0) // jde o interni zpracovani (plug-in)?
                            {
                                data = Plugins.Get(-index - 1);
                                if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                                    used = TRUE;
                            }
                        }
                    }
                }
            }
        }
        if (used)
        {
            if (Is(ptZIPArchive) || Is(ptPluginFS)) // archiv -> jen ho opustime; FS -> vracime se na posledni diskovou cestu
            {
                char path[MAX_PATH];
                strcpy(path, GetPath());

                DWORD err, lastErr;
                BOOL pathInvalid, cut;
                BOOL tryNet = FALSE; // uz zadny zdrzovani se siti, zbytecne...
                if (SalCheckAndRestorePathWithCut(HWindow, path, tryNet, err, lastErr, pathInvalid, cut, TRUE))
                { // prepneme se na cestu, ktera by mela jit bez potizi nacist
                    ChangePathToDisk(parent, path, -1, NULL, NULL, TRUE, TRUE, FALSE, NULL, FALSE, FSTRYCLOSE_UNLOADCLOSEFS);
                }
                else // puvodni cesta (ani jeji podcesta) neni pristupna -> jdeme na fixed-drive (nelze volat
                     // primo ChangePathToDisk, protoze jinak se vypisuje chyba - napr. "X: not ready")
                {
                    ChangeToRescuePathOrFixedDrive(parent, NULL, TRUE, TRUE, FSTRYCLOSE_UNLOADCLOSEFS);
                }
                if (!Is(ptDisk))
                {
                    return FALSE; // zmena cesty na disk se nepovedla, unload neni mozny
                }
            }
        }
    }
    return TRUE;
}

void CFilesWindow::RedrawFocusedIndex()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RedrawFocusedIndex()");
    RedrawIndex(FocusedIndex);
}

void CFilesWindow::DirectoryLineSetText()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DirectoryLineSetText()");
    char ZIPbuf[2 * MAX_PATH];
    const char* path = NULL;
    if (Is(ptZIPArchive))
    {
        strcpy(ZIPbuf, GetZIPArchive());
        if (GetZIPPath()[0] != 0)
        {
            if (GetZIPPath()[0] != '\\')
                strcat(ZIPbuf, "\\");
            strcat(ZIPbuf, GetZIPPath());
        }
        path = ZIPbuf;
        PathHistory->AddPath(1, GetZIPArchive(), GetZIPPath(), NULL, NULL);
    }
    else
    {
        if (Is(ptDisk))
        {
            PathHistory->AddPath(0, GetPath(), NULL, NULL, NULL);
            path = GetPath();
        }
        else
        {
            if (Is(ptPluginFS))
            {
                int l = (int)strlen(GetPluginFS()->GetPluginFSName());
                memcpy(ZIPbuf, GetPluginFS()->GetPluginFSName(), l);
                ZIPbuf[l++] = ':';
                if (!GetPluginFS()->NotEmpty() || !GetPluginFS()->GetCurrentPath(ZIPbuf + l))
                    ZIPbuf[l] = 0;
                else
                {
                    PathHistory->AddPath(2, GetPluginFS()->GetPluginFSName(), ZIPbuf + l,
                                         GetPluginFS()->GetInterface(), GetPluginFS());
                }
                path = ZIPbuf;
            }
        }
    }

    if (path == NULL)
        return;

    if (FilterEnabled)
    {
        char buf[3 * MAX_PATH]; // zip path (2x) + filter (1x) = 3x MAX_PATH
        int pathLen = (int)strlen(path);
        if (Is(ptDisk) || Is(ptZIPArchive))
        {
            int l = pathLen;
            memcpy(buf, path, l);
            if (buf[l - 1] != '\\')
                buf[l++] = '\\';
            lstrcpyn(buf + l, Filter.GetMasksString(), MAX_PATH);
        }
        else
        {
            if (Is(ptPluginFS))
            {
                int l = pathLen;
                memcpy(buf, path, l);
                buf[l++] = ':';
                //        if (FilterInverse) buf[l++] = '-';
                lstrcpyn(buf + l, Filter.GetMasksString(), MAX_PATH);
            }
        }
        DirectoryLine->SetText(buf, pathLen);
    }
    else
    {
        DirectoryLine->SetText(path);
    }
}

void CFilesWindow::SelectUnselect(BOOL forceIncludeDirs, BOOL select, BOOL showMaskDlg)
{
    CALL_STACK_MESSAGE4("CFilesWindow::SelectUnselect(%d, %d, %d)", forceIncludeDirs, select, showMaskDlg);
    if (showMaskDlg)
    {
        BeginStopRefresh(); // cmuchal si da pohov
    }
    if (!showMaskDlg || CSelectDialog(HLanguage, select ? IDD_SELECTMASK : IDD_DESELECTMASK,
                                      select ? IDD_SELECTMASK : IDD_DESELECTMASK /* helpID */,
                                      HWindow, MainWindow->SelectionMask)
                                .Execute() == IDOK)
    {
        BOOL includeDirs = Configuration.IncludeDirs | forceIncludeDirs;
        const char* maskStr = showMaskDlg ? MainWindow->SelectionMask : "*.*";
        CMaskGroup mask(maskStr);
        int err;
        if (mask.PrepareMasks(err))
        {
            int dirsCount = Dirs->Count;
            int count = dirsCount + Files->Count;
            int start;
            if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
                start = 1;
            else
                start = 0;
            int i = includeDirs ? start : Dirs->Count;
            BOOL changed = FALSE;
            for (; i < count; i++)
            {
                CFileData* d = (i < dirsCount) ? &Dirs->At(i) : &Files->At(i - dirsCount);
                if (!showMaskDlg || mask.AgreeMasks(d->Name, i < dirsCount ? NULL : d->Ext)) // v pripade *.* nebudeme volat agree mask
                {
                    SetSel(select, d);
                    changed = TRUE;
                }
            }
            if (changed)
            {
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
            }
            else
                SalMessageBox(HWindow, LoadStr(IDS_NOMATCHESFOUND), LoadStr(IDS_INFOTITLE),
                              MB_OK | MB_ICONINFORMATION);
        }
    }
    if (showMaskDlg)
    {
        UpdateWindow(MainWindow->HWindow);
        EndStopRefresh(); // ted uz zase cmuchal nastartuje
    }
}

void CFilesWindow::InvertSelection(BOOL forceIncludeDirs)
{
    CALL_STACK_MESSAGE2("CFilesWindow::InvertSelection(%d)", forceIncludeDirs);
    BOOL includeDirs = Configuration.IncludeDirs | forceIncludeDirs;
    int count = GetSelCount();
    int firstIndex = 0;
    if (includeDirs)
    {
        if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
            firstIndex = 1;
    }
    else
    {
        firstIndex = Dirs->Count;
    }

    int lastIndex = Dirs->Count + Files->Count - 1;
    if (firstIndex <= lastIndex)
    {
        int i;
        for (i = firstIndex; i <= lastIndex; i++)
        {
            CFileData* item = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            SetSel(item->Selected != 1, item);
        }
        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
    }
}

void CFilesWindow::SelectUnselectByFocusedItem(BOOL select, BOOL byName)
{
    CALL_STACK_MESSAGE3("CFilesWindow::SelectUnselectByFocusedItem(%d, %d)", select, byName);
    if (FocusedIndex >= 0 && FocusedIndex < Dirs->Count + Files->Count)
    {

        //    if (!byName && FocusedIndex < Dirs->Count)
        //    {
        //
        //    }

        BOOL isDir = FocusedIndex < Dirs->Count;
        const CFileData* focusedItem = isDir ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);

        int firstIndex = 0;
        if (Configuration.IncludeDirs)
        {
            if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
                firstIndex = 1;
        }
        else
        {
            firstIndex = Dirs->Count;
        }
        int lastIndex = Dirs->Count + Files->Count - 1;
        int lastSelectdCount = SelectedCount;
        const char* focusedStr = byName ? focusedItem->Name : (isDir ? "" : focusedItem->Ext);
        int focusedLen = byName ? (isDir ? focusedItem->NameLen : (int)(focusedItem->Ext - focusedItem->Name)) : (isDir ? 0 : (int)lstrlen(focusedItem->Ext));
        if (!isDir && byName && *focusedItem->Ext != 0)
            focusedLen--; // preskocim '.'
        int i;
        for (i = firstIndex; i <= lastIndex; i++)
        {
            BOOL itemIsDir = i < Dirs->Count;
            CFileData* item = itemIsDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            const char* str = byName ? item->Name : (itemIsDir ? "" : item->Ext);
            int len = byName ? (itemIsDir ? item->NameLen : (int)(item->Ext - item->Name)) : (itemIsDir ? 0 : (int)lstrlen(item->Ext));
            if (!itemIsDir && byName && *item->Ext != 0)
                len--; // preskocim '.'
            if (len == focusedLen && StrNICmp(str, focusedStr, len) == 0)
                SetSel(select, item);
        }
        if (SelectedCount != lastSelectdCount)
        {
            RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
        }
    }
}

void CFilesWindow::StoreGlobalSelection()
{
    CALL_STACK_MESSAGE1("CFilesWindow::StoreGlobalSelection()");
    int count = GetSelCount();
    if (count != 0)
    {
        BeginStopRefresh(); // cmuchal si da pohov

        BOOL clipboard = FALSE;
        CSaveSelectionDialog dlg(HWindow, &clipboard);
        if (dlg.Execute() == IDOK)
        {
            int totalCount = Dirs->Count + Files->Count;
            if (clipboard)
            {
                // seznam mame hodit na clipboard

                // napocitame potrebnou velikost bufferu (nazev1CRLFnazev2CRLF...nazevNCRLF)
                DWORD size = 0;
                int i;
                for (i = 0; i < totalCount; i++)
                {
                    CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                    if (f->Selected)
                        size += f->NameLen + 2; // nameCRLF
                }
                if (size > 0)
                {
                    char* buff = (char*)malloc(size);
                    if (buff != NULL)
                    {
                        char* p = buff;
                        for (i = 0; i < totalCount; i++)
                        {
                            CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                            if (f->Selected)
                            {
                                memcpy(p, f->Name, f->NameLen);
                                p += f->NameLen;
                                memcpy(p, "\r\n", 2);
                                p += 2;
                            }
                        }
                        CopyTextToClipboard(buff, size);
                        free(buff);
                    }
                    else
                        TRACE_E(LOW_MEMORY);
                }
            }
            else
            {
                // seznam mame hodit do GlobalSelection
                GlobalSelection.Clear();
                int i;
                for (i = 0; i < totalCount; i++)
                {
                    CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                    if (f->Selected)
                    {
                        if (!GlobalSelection.Add(i < Dirs->Count, f->Name))
                            break; // low memory
                    }
                }
                GlobalSelection.Sort();
            }
            IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
        }
        UpdateWindow(MainWindow->HWindow);

        EndStopRefresh(); // ted uz zase cmuchal nastartuje
    }
}

void CFilesWindow::RestoreGlobalSelection()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RestoreGlobalSelection()");

    BOOL clipboardValid = IsTextOnClipboard();
    BOOL globalValid = GlobalSelection.GetCount() > 0;
    if (clipboardValid || globalValid)
    {
        BeginStopRefresh(); // cmuchal si da pohov

        CLoadSelectionOperation operation = lsoCOPY;
        BOOL clipboard = !globalValid;
        CLoadSelectionDialog dlg(HWindow, &operation, &clipboard, clipboardValid, globalValid);
        if (dlg.Execute() == IDOK)
        {
            CNames* selection = &GlobalSelection;
            CNames clipboardSelection;
            if (clipboard)
            {
                clipboardSelection.LoadFromClipboard(HWindow);
                clipboardSelection.Sort();
                selection = &clipboardSelection;
            }

            int count = Files->Count + Dirs->Count;
            int i;
            for (i = 0; i < count; i++)
            {
                BOOL isDir = i < Dirs->Count;
                CFileData* file = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (clipboard)
                    isDir = FALSE; // pokud jedeme pres clipboard, vse je ve Files
                switch (operation)
                {
                case lsoCOPY:
                {
                    SetSel(selection->Contains(isDir, file->Name), file);
                    break;
                }

                case lsoOR:
                {
                    if (selection->Contains(isDir, file->Name))
                        SetSel(TRUE, file);
                    break;
                }

                case lsoDIFF:
                {
                    if (file->Selected)
                        SetSel(!selection->Contains(isDir, file->Name), file);
                    break;
                }

                case lsoAND:
                {
                    SetSel(file->Selected && selection->Contains(isDir, file->Name), file);
                    break;
                }

                default:
                {
                    TRACE_E("Unknown operation: " << operation);
                    break;
                }
                }
            }
            RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
        }
        UpdateWindow(MainWindow->HWindow);
        EndStopRefresh(); // ted uz zase cmuchal nastartuje
    }
}

void CFilesWindow::StoreSelection()
{
    CALL_STACK_MESSAGE1("CFilesWindow::StoreSelection()");
    OldSelection.Clear();
    int count = GetSelCount();
    if (count != 0)
    {
        OldSelection.SetCaseSensitive(IsCaseSensitive());
        int totalCount = Files->Count + Dirs->Count;
        int i;
        for (i = 0; i < totalCount; i++)
        {
            BOOL isDir = i < Dirs->Count;
            CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (f->Selected)
            {
                if (!OldSelection.Add(isDir, f->Name))
                    break; // low memory
            }
        }
        OldSelection.Sort();
        IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
    }
}

void CFilesWindow::Reselect()
{
    CALL_STACK_MESSAGE1("CFilesWindow::Reselect()");
    int count = Files->Count + Dirs->Count;
    int i;
    for (i = 0; i < count; i++)
    {
        BOOL isDir = i < Dirs->Count;
        CFileData* file = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
        if (OldSelection.Contains(isDir, file->Name))
            SetSel(TRUE, file);
        else
            SetSel(FALSE, file);
    }
    RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
}

void CFilesWindow::ShowHideNames(int mode)
{
    BOOL refreshPanel = FALSE;
    switch (mode)
    {
    case 0: // show all
    {
        if (HiddenNames.GetCount() > 0)
        {
            HiddenNames.Clear();
            refreshPanel = TRUE;
        }
        break;
    }

    case 1: // hide selected names
    {
        int count = GetSelCount();
        if (count > 0)
        {
            int totalCount = Files->Count + Dirs->Count;
            int startIndex = 0;
            if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0) // ".." v poli nechceme
                startIndex = 1;
            int i;
            for (i = 0; i < totalCount; i++)
            {
                BOOL isDir = i < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (f->Selected)
                {
                    if (!HiddenNames.Add(isDir, f->Name))
                        break; // low_memory, nebudeme pokracovat
                    refreshPanel = TRUE;
                }
            }
        }
        break;
    }

    case 2: // hide unselected name
    {
        int totalCount = Files->Count + Dirs->Count;
        int startIndex = 0;
        if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0) // ".." v poli nechceme
            startIndex = 1;
        int i;
        for (i = startIndex; i < totalCount; i++)
        {
            BOOL isDir = i < Dirs->Count;
            CFileData* f = (isDir) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (!f->Selected)
            {
                if (!HiddenNames.Add(isDir, f->Name))
                    break; // low_memory, nebudeme pokracovat
                refreshPanel = TRUE;
            }
        }
        break;
    }

    default:
    {
        TRACE_E("ShowHideNames: unknown mode=" << mode);
    }
    }

    if (refreshPanel)
    {
        if (mode == 1 || mode == 2)
            HiddenNames.SetCaseSensitive(IsCaseSensitive());
        HiddenNames.Sort();
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));
        PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
    }
}

void CFilesWindow::SetAutomaticRefresh(BOOL value, BOOL force)
{
    CALL_STACK_MESSAGE_NONE
    if (force || AutomaticRefresh != value)
    {
        AutomaticRefresh = value;
        /* // "vyhozeni" refresh znacky z dir-liny
    // tady nam to padalo; volal se zdestroyeny objekt
    if (DirectoryLine != NULL)                       
      DirectoryLine->SetAutomatic(AutomaticRefresh);
*/
    }
}

void CFilesWindow::GotoRoot()
{
    CALL_STACK_MESSAGE1("CFilesWindow::GotoRoot()");
    TopIndexMem.Clear(); // dlouhy skok

    char root[MAX_PATH];
    if (Is(ptDisk) || Is(ptZIPArchive))
    {
        if (Is(ptZIPArchive) && GetZIPPath()[0] != 0) // nejsme v rootu archivu -> jdeme do nej
        {
            ChangePathToArchive(GetZIPArchive(), "");
        }
        else // jdeme do rootu windows cesty
        {
            if (IsUNCRootPath(GetPath()) && Plugins.GetFirstNethoodPluginFSName(root))
            {
                ChangePathToPluginFS(root, "");
            }
            else
            {
                GetRootPath(root, GetPath());
                if (root[0] == '\\')
                    root[strlen(root) - 1] = 0; // UNC nebude koncit '\\'
                ChangePathToDisk(HWindow, root);
            }
        }
    }
    else
    {
        if (Is(ptPluginFS))
        {
            if (GetPluginFS()->GetRootPath(root))
            {
                char fsname[MAX_PATH];
                strcpy(fsname, GetPluginFS()->GetPluginFSName()); // pro pripad zmeny, lokalni kopie jmena
                ChangePathToPluginFS(fsname, root);
            }
        }
    }
}

void CFilesWindow::GotoHotPath(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::GotoHotPath(%d)", index);
    if (index < 0 || index >= HOT_PATHS_COUNT)
        return;
    //---  prepnuti na hot-path
    char path[2 * MAX_PATH];
    if (MainWindow->GetExpandedHotPath(HWindow, index, path, 2 * MAX_PATH))
        ChangeDir(path);
}

void CFilesWindow::SetUnescapedHotPath(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::SetUnescapedHotPath(%d)", index);
    if (index < 0 || index >= HOT_PATHS_COUNT)
        return;
    char path[2 * MAX_PATH];
    GetGeneralPath(path, 2 * MAX_PATH, TRUE);
    MainWindow->SetUnescapedHotPath(index, path);
}

BOOL CFilesWindow::SetUnescapedHotPathToEmptyPos()
{
    CALL_STACK_MESSAGE1("CFilesWindow::SetUnescapedHotPathToEmptyPos()");
    int index = MainWindow->GetUnassignedHotPathIndex();
    if (index != -1)
    {
        char path[2 * MAX_PATH];
        GetGeneralPath(path, 2 * MAX_PATH, TRUE);
        MainWindow->SetUnescapedHotPath(index, path);
        return TRUE;
    }
    return FALSE;
}

#ifndef _WIN64

BOOL AreNextPathComponents(const char* relPath, const char* nextComp)
{
    int len = (int)strlen(nextComp);
    return StrNICmp(relPath, nextComp, len) == 0 && (relPath[len] == '\\' || relPath[len] == 0);
}

#endif // _WIN64

void CFilesWindow::OpenActiveFolder()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OpenActiveFolder()");
    if (Is(ptDisk) && CheckPath(TRUE) != ERROR_USER_TERMINATED)
    {
        UserWorkedOnThisPath = TRUE;
        const char* path = GetPath();

#ifndef _WIN64
        // provedeme zamenu "C:\\Windows\\sysnative\\*" za "C:\\Windows\\system32\\*", 64-bitovy
        // proces Exploreru o "sysnative" nic nevi, nebudeme s tim otravovat lidi,
        // zaroven provedeme zamenu "C:\\Windows\\system32\\*" za "C:\\Windows\\SysWOW64\\*"
        // (az na skupinu adresaru vyjmutych z redirectoru, ktere tim smeruji zpet do System32)
        char dirName[MAX_PATH];
        dirName[0] = 0;
        if (Windows64Bit && WindowsDirectory[0] != 0)
        {
            BOOL done = FALSE;
            lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
            if (SalPathAppend(dirName, "Sysnative", MAX_PATH))
            {
                int len = (int)strlen(dirName);
                if (StrNICmp(path, dirName, len) == 0 && (path[len] == '\\' || path[len] == 0))
                {
                    lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
                    SalPathAppend(dirName, "System32", MAX_PATH); // kdyz se vesel Sysnative, System32 se vejde taky
                    memmove(dirName + strlen(dirName), path + len, strlen(path + len) + 1);
                    path = dirName;
                    done = TRUE;
                }
            }
            if (!done)
            {
                lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
                if (SalPathAppend(dirName, "System32", MAX_PATH))
                {
                    int len = (int)strlen(dirName);
                    if (StrNICmp(path, dirName, len) == 0 && (path[len] == '\\' || path[len] == 0))
                    {
                        // zjistime jestli nejde o adresare vyjmute z redirectoru
                        if (path[len] == '\\' &&
                            (AreNextPathComponents(path + len + 1, "catroot") ||
                             AreNextPathComponents(path + len + 1, "catroot2") ||
                             Windows7AndLater && AreNextPathComponents(path + len + 1, "DriverStore") ||
                             AreNextPathComponents(path + len + 1, "drivers\\etc") ||
                             AreNextPathComponents(path + len + 1, "LogFiles") ||
                             AreNextPathComponents(path + len + 1, "spool")))
                        {
                            done = TRUE;
                        }
                        if (!done)
                        {
                            lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
                            SalPathAppend(dirName, "SysWOW64", MAX_PATH); // kdyz se vesel System32, SysWOW64 se vejde taky
                            memmove(dirName + strlen(dirName), path + len, strlen(path + len) + 1);
                            path = dirName;
                        }
                    }
                }
            }
        }
#endif // _WIN64

        char itemName[MAX_PATH];
        itemName[0] = 0;
        if (FocusedIndex < Dirs->Count + Files->Count)
        {
            CFileData* item = (FocusedIndex < Dirs->Count) ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);
            // hack pro lidi, kteri potrebuji focusnout unicode nazev v Exploreru, zkusime to pres short name
            AlterFileName(itemName, item->DosName != NULL ? item->DosName : item->Name, -1, Configuration.FileNameFormat, 0, FocusedIndex < Dirs->Count);
            if (FocusedIndex < Dirs->Count && FocusedIndex == 0 && strcmp(itemName, "..") == 0)
                itemName[0] = 0;
        }

        OpenFolderAndFocusItem(HWindow, path, itemName);
    }
    else if (Is(ptPluginFS) &&
             GetPluginFS()->NotEmpty() &&
             GetPluginFS()->IsServiceSupported(FS_SERVICE_OPENACTIVEFOLDER))
    {
        UserWorkedOnThisPath = TRUE;
        GetPluginFS()->OpenActiveFolder(GetPluginFS()->GetPluginFSName(), HWindow);
    }
}

BOOL CFilesWindow::CommonRefresh(HWND parent, int suggestedTopIndex, const char* suggestedFocusName,
                                 BOOL refreshListBox, BOOL readDirectory, BOOL isRefresh)
{
    CALL_STACK_MESSAGE6("CFilesWindow::CommonRefresh(, %d, %s, %d, %d, %d)", suggestedTopIndex,
                        suggestedFocusName, refreshListBox, readDirectory, isRefresh);

    //TRACE_I("common refresh: begin");
    if (readDirectory) // pokud se ma jen promitnout top-index a focus-name, tohle neni potreba (muze byt jen na skodu, proto nevolame)
    {
        DirectoryLineSetText();
        if (Parent->GetActivePanel() == this)
        {
            Parent->EditWindowSetDirectory();
        }
    }

    //TRACE_I("read directory: begin");
    BOOL ret = FALSE;
    if (!readDirectory || ReadDirectory(parent, isRefresh))
        ret = TRUE;
    else
    {
        if (Is(ptDisk) || Is(ptZIPArchive))
            DetachDirectory(this); // cosi se podelalo
    }
    //TRACE_I("read directory: begin");

    if (refreshListBox)
    {
        // vyhledam polozku, kterou bych mel vybrat
        int suggestedFocusIndex = -1;
        int suggestedFocusIndexIgnCase = -1;
        if (suggestedFocusName != NULL)
        {
            int i;
            for (i = 0; i < Dirs->Count; i++)
            {
                if (StrICmp(Dirs->At(i).Name, suggestedFocusName) == 0)
                {
                    if (suggestedFocusIndexIgnCase == -1)
                        suggestedFocusIndexIgnCase = i;
                    if (strcmp(Dirs->At(i).Name, suggestedFocusName) == 0)
                    {
                        suggestedFocusIndex = i;
                        break; // nalezeno presne pozadovane jmeno
                    }
                }
            }
            if (suggestedFocusIndex == -1) // hledame i mezi soubory (napr. pri navratu ze ZIP archivu)
            {
                for (i = 0; i < Files->Count; i++)
                {
                    if (StrICmp(Files->At(i).Name, suggestedFocusName) == 0)
                    {
                        if (suggestedFocusIndexIgnCase == -1)
                            suggestedFocusIndexIgnCase = i + Dirs->Count;
                        if (strcmp(Files->At(i).Name, suggestedFocusName) == 0)
                        {
                            suggestedFocusIndex = i + Dirs->Count;
                            break; // nalezeno presne pozadovane jmeno
                        }
                    }
                }
            }
            // pokud nebylo nalezeno presne pozadovane jmeno, pouzijeme jmeno shodne az na velikost pismen (je-li)
            if (suggestedFocusIndex == -1)
                suggestedFocusIndex = suggestedFocusIndexIgnCase;
        }

        //TRACE_I("refresh listbox: begin");
        RefreshListBox(0, suggestedTopIndex, suggestedFocusIndex, TRUE, !isRefresh);
        //TRACE_I("refresh listbox: end");
    }

    DirectoryLine->InvalidateIfNeeded();
    //TRACE_I("common refresh: end");
    return ret;
}
