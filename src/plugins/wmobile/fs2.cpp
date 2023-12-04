// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CPluginFSInterface
//

#define FILE_ATTRIBUTES_MASK (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)

CPluginFSInterface::CPluginFSInterface()
{
    Path[0] = 0;
    PathError = FALSE;
    FatalError = FALSE;
}

void WINAPI
CPluginFSInterface::ReleaseObject(HWND parent)
{
    // pokud je FS inicializovan, zrusime pri zavirani nase kopie souboru v disk-cache
    if (Path[0] != 0)
        EmptyCache();
}

BOOL WINAPI
CPluginFSInterface::GetRootPath(char* userPart)
{
    userPart[0] = '\\';
    userPart[1] = 0;
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetCurrentPath(char* userPart)
{
    strcpy(userPart, Path);
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
{
    lstrcpyn(buf, Path, bufSize); // pokud se nevejde cesta, jmeno se urcite take nevejde (ohlasi chybu)
    if (isDir == 2)
        return SalamanderGeneral->CutDirectory(buf, NULL); // up-dir
    else
        return CRAPI::PathAppend(buf, file.Name, bufSize);
}

BOOL WINAPI
CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    if (Path[0] == 0)
        return FALSE; // preklad neni mozny, at si chybu ohlasi sam Salamander

    char root[MAX_PATH] = "\\"; //JR ve Windows Mobile je root vzdy "\\"

    if (*path != '\\')
        strcpy(root, Path); // cesty typu "path" prebiraji akt. cestu na FS

    success = CRAPI::PathAppend(root, path, MAX_PATH);
    if (success && (int)strlen(root) < 1) // kratsi nez root neni mozne (to by byla opet rel. cesta)
    {
        success = SalamanderGeneral->SalPathAddBackslash(root, MAX_PATH);
    }
    if (success)
        success = (int)(strlen(root) + strlen(fsName) + 1) < pathSize; // vejde se?
    if (success)
        sprintf(path, "%s:%s", fsName, root);
    else
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_PATHTOOLONG),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
    }
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    return SalamanderGeneral->IsTheSamePath(Path, userPart);
}

BOOL WINAPI
CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    return TRUE; //JR REVIEW: Koho jineho by byla?
}

BOOL WINAPI
CPluginFSInterface::ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                               const char* userPart, char* cutFileName, BOOL* pathWasCut,
                               BOOL forceRefresh, int mode)
{
    if (mode != 3 && (pathWasCut != NULL || cutFileName != NULL))
    {
        TRACE_E("Incorrect value of 'mode' in CPluginFSInterface::ChangePath().");
        mode = 3;
    }
    if (pathWasCut != NULL)
        *pathWasCut = FALSE;
    if (cutFileName != NULL)
        *cutFileName = 0;
    if (FatalError)
    {
        FatalError = FALSE;
        return FALSE; // ListCurrentPath selhala kvuli pameti, fatal error
    }

    if (forceRefresh)
        EmptyCache();

    char buf[2 * MAX_PATH + 100];
    char errBuf[MAX_PATH];
    errBuf[0] = 0;
    char path[MAX_PATH];
    int err = 0;

    lstrcpyn(path, userPart, MAX_PATH);

    BOOL fileNameAlreadyCut = FALSE;
    if (PathError) // chyba pri listovani cesty (chyba vypsana uzivateli uz v ListCurrentPath)
    {              // zkusime cestu oriznout
        PathError = FALSE;
        if (!SalamanderGeneral->CutDirectory(path, NULL))
            return FALSE; // neni kam zkracovat, fatal error
        fileNameAlreadyCut = TRUE;
        if (pathWasCut != NULL)
            *pathWasCut = TRUE;
    }
    while (1)
    {
        DWORD attr = CRAPI::GetFileAttributes(path, TRUE);
        if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) // uspech, vybereme cestu jako aktualni
        {
            if (errBuf[0] != 0) // pokud mame hlaseni, vypiseme ho zde (vzniklo behem zkracovani)
            {
                sprintf(buf, LoadStr(IDS_PATH_ERROR), userPart, errBuf);
                SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
            }
            strcpy(Path, path);
            return TRUE;
        }
        else // neuspech, zkusime cestu zkratit
        {
            err = CRAPI::GetLastError();

            if (mode != 3 && attr != 0xFFFFFFFF || // soubor misto cesty -> ohlasime jako chybu
                mode != 1 && attr == 0xFFFFFFFF)   // neexistence cesty -> ohlasime jako chybu
            {
                if (attr != 0xFFFFFFFF)
                {
                    sprintf(errBuf, LoadStr(IDS_ERR_FILEINPATH));
                }
                else
                    SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);

                // pokud je otevreni FS casove narocne a chceme upravit chovani Change Directory (Shift+F7)
                // jako u archivu, staci zakomentovat nasledujici radek s prikazem "break" pro 'mode' 3

                //JR o orezavani se pokusim pouze pokud RAPI hlasi ze cesta neexistuje
                if (mode == 3 || err != ERROR_FILE_NOT_FOUND)
                    break;
            }

            char* cut;
            if (!SalamanderGeneral->CutDirectory(path, &cut)) // neni kam zkracovat, fatal error
            {
                SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);
                break;
            }
            else
            {
                if (pathWasCut != NULL)
                    *pathWasCut = TRUE;
                if (!fileNameAlreadyCut) // jmeno souboru to muze byt jen pri prvnim oriznuti
                {
                    fileNameAlreadyCut = TRUE;
                    if (cutFileName != NULL && attr != 0xFFFFFFFF) // jde o soubor
                        lstrcpyn(cutFileName, cut, MAX_PATH);
                }
                else
                {
                    if (cutFileName != NULL)
                        *cutFileName = 0; // to uz byt jmeno souboru nemuze
                }
            }
        }
    }

    sprintf(buf, LoadStr(IDS_PATH_ERROR), userPart, errBuf);
    SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
    PathError = FALSE;
    return FALSE; // fatal path error
}

BOOL WINAPI
CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData,
                                    int& iconsType, BOOL forceRefresh)
{
    if (forceRefresh)
        EmptyCache();

    CFileData file;

    iconsType = pitFromRegistry;

    char buf[2 * MAX_PATH + 100];
    char curPath[MAX_PATH + 4];
    strcpy(curPath, Path);
    CRAPI::PathAppend(curPath, "*", MAX_PATH + 4);
    char* name = curPath + strlen(curPath) - 3;

    DWORD count;
    RapiNS::LPCE_FIND_DATA pFindDataArray;
    if (!CRAPI::FindAllFiles(curPath, FAF_NAME | FAF_ATTRIBUTES | FAF_SIZE_LOW | FAF_SIZE_HIGH | FAF_LASTWRITE_TIME,
                             &count, &pFindDataArray, TRUE))
    {
        DWORD err = CRAPI::GetLastError();
        sprintf(buf, LoadStr(IDS_PATH_ERROR), Path, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
        PathError = TRUE;
        return FALSE;
    }

    DWORD i = 0;

    if (strcmp(Path, "\\") != 0) ///JR Nejsme v rootu, tak pridame .. radek
    {
        file.Name = SalamanderGeneral->DupStr("..");
        if (file.Name == NULL)
            goto ONERROR;
        file.NameLen = 2;
        file.Ext = file.Name + file.NameLen;
        file.Size = CQuadWord(0, 0);
        file.Attr = FILE_ATTRIBUTE_DIRECTORY;
        file.LastWrite.dwLowDateTime = 0;
        file.LastWrite.dwHighDateTime = 0;
        file.Hidden = 0;
        file.DosName = NULL; //Windows Mobile nema 8.3 nazvy soboru
        file.IsLink = 0;
        file.IsOffline = 0;

        if (!dir->AddDir(NULL, file, NULL))
            goto ONERROR;
    }

    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);

    for (; i < count; i++)
    {
        RapiNS::CE_FIND_DATA& data = pFindDataArray[i];

        if (data.cFileName[0] != 0 &&
            (data.cFileName[0] != '.' || //JR Windows Mobile nevraci "." a ".." cesty, ale pro jistotu to osetrim
             (data.cFileName[1] != 0 && (data.cFileName[1] != '.' || data.cFileName[2] != 0))))
        {
            char cFileName[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, data.cFileName, -1, cFileName, MAX_PATH, NULL, NULL);
            cFileName[MAX_PATH - 1] = 0;

            file.Name = SalamanderGeneral->DupStr(cFileName);
            if (file.Name == NULL)
                goto ONERROR;
            file.NameLen = strlen(file.Name);
            if (!sortByExtDirsAsFiles && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                file.Ext = file.Name + file.NameLen; // adresare nemaji pripony
            }
            else
            {
                char* s;
                s = strrchr(file.Name, '.');
                if (s != NULL)
                    file.Ext = s + 1; // ".cvspass" ve Windows je pripona
                else
                    file.Ext = file.Name + file.NameLen;
            }
            file.Size = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh);
            file.Attr = data.dwFileAttributes;
            file.Attr &= ~(FILE_ATTRIBUTE_COMPRESSED);

            file.LastWrite = data.ftLastWriteTime;
            file.Hidden = (file.Attr & FILE_ATTRIBUTE_HIDDEN) != 0;
            file.DosName = NULL; //Windows Mobile nema 8.3 nazvy soboru
            file.IsOffline = 0;
            if (file.Attr & FILE_ATTRIBUTE_DIRECTORY)
                file.IsLink = 0;
            else
                file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);

            if ((file.Attr & FILE_ATTRIBUTE_DIRECTORY) == 0 && !dir->AddFile(NULL, file, NULL) ||
                (file.Attr & FILE_ATTRIBUTE_DIRECTORY) != 0 && !dir->AddDir(NULL, file, NULL))
            {
                goto ONERROR;
            }
        }
    }

    CRAPI::FreeBuffer(pFindDataArray);
    return TRUE;

ONERROR:
    TRACE_E("Low memory");
    if (file.Name != NULL)
        SalamanderGeneral->Free(file.Name);
    CRAPI::FreeBuffer(pFindDataArray);

    FatalError = TRUE;
    return FALSE;
}

BOOL WINAPI
CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    //JR Windows Mobile plugin se vzdy odpoji
    detach = FALSE;
    return TRUE;
}

void WINAPI
CPluginFSInterface::Event(int event, DWORD param)
{
    switch (event)
    {
    case FSE_ACTIVATEREFRESH: // uzivatel aktivoval Salamandera (prepnuti z jine aplikace)
        if (!CRAPI::CheckConnection())
        {
            int panel1 = param;
            int panel2 = (panel1 == PANEL_LEFT ? PANEL_RIGHT : PANEL_LEFT);

            if (CRAPI::ReInit())
            {
                SalamanderGeneral->PostRefreshPanelPath(panel1);
                if (SalamanderGeneral->GetPanelPluginFS(panel2) != NULL)
                    SalamanderGeneral->PostRefreshPanelPath(panel2);
            }
            else if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_YESNO_CONNETCLOSEPLUGIN), TitleWMobileQuestion,
                                                       MSGBOX_QUESTION) == IDYES)
            {
                SalamanderGeneral->DisconnectFSFromPanel(SalamanderGeneral->GetMainWindowHWND(), panel1);
                if (SalamanderGeneral->GetPanelPluginFS(panel2) != NULL)
                    SalamanderGeneral->DisconnectFSFromPanel(SalamanderGeneral->GetMainWindowHWND(), panel2);
            }
        }
        break;
    case FSE_CLOSEORDETACHCANCELED:
    case FSE_OPENED:
    case FSE_ATTACHED:
    case FSE_DETACHED:
        //noop
        break;
    }
}

DWORD WINAPI
CPluginFSInterface::GetSupportedServices()
{
    return 0 |
           FS_SERVICE_CONTEXTMENU |
           //JR TODO FS_SERVICE_SHOWPROPERTIES |
           FS_SERVICE_CHANGEATTRS |
           FS_SERVICE_COPYFROMDISKTOFS |
           FS_SERVICE_MOVEFROMDISKTOFS |
           FS_SERVICE_MOVEFROMFS |
           FS_SERVICE_COPYFROMFS |
           FS_SERVICE_DELETE |
           FS_SERVICE_VIEWFILE |
           //JR TODO: jeste neni podporovano v Salamandru         FS_SERVICE_EDITFILE |
           FS_SERVICE_CREATEDIR |
           FS_SERVICE_ACCEPTSCHANGENOTIF |
           FS_SERVICE_QUICKRENAME |
           //JR TODO: jeste neni podporovano v Salamandru         FS_SERVICE_CALCULATEOCCUPIEDSPACE |
           FS_SERVICE_COMMANDLINE |
           //JR TODO: FS_SERVICE_SHOWINFO |
           FS_SERVICE_GETFREESPACE |
           FS_SERVICE_GETFSICON |
           FS_SERVICE_GETNEXTDIRLINEHOTPATH;
    //         FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM;
}

/*
BOOL WINAPI
CPluginFSInterface::GetChangeDriveOrDisconnectItem(const char *fsName, char *&title, HICON &icon, BOOL &destroyIcon)
{
  char txt[2 * MAX_PATH + 102];
  // text bude cesta na FS (v Salamanderovskem formatu)
  txt[0] = '\t';
  strcpy(txt + 1, fsName);
  sprintf(txt + strlen(txt), ":%s\t", Path);
  // zdvojime pripadne '&', aby se tisk cesty provedl korektne
  SalamanderGeneral->DuplicateAmpersands(txt, 2 * MAX_PATH + 102);

  // pripojime informaci o volnem miste
  CQuadWord space;
  GetFSFreeSpace(&space);
  if (space != CQuadWord(-1, -1)) SalamanderGeneral->PrintDiskSize(txt + strlen(txt), space, 0);

  title = SalamanderGeneral->DupStr(txt);
  if (title == NULL) return FALSE;  // low-memory, zadna polozka nebude

  icon = GetFSIcon(destroyIcon);
  return TRUE;
}

*/

HICON WINAPI
CPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
{
    destroyIcon = TRUE;
    // return LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FS)); // zlobi barvy v Alt+F1/2: As Other Panel
    return (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FS),
                            IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
}

void WINAPI
CPluginFSInterface::GetFSFreeSpace(CQuadWord* retValue)
{
    retValue->LoDWord = -1;
    retValue->HiDWord = -1;

    RapiNS::STORE_INFORMATION si;
    if (Path[0] != 0 && CRAPI::GetStoreInformation(&si))
    {
        retValue->LoDWord = si.dwFreeSize;
        retValue->HiDWord = 0;
    }
}

BOOL WINAPI
CPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    const char* root = text; // ukazatel za root cestu

    while (*root != 0 && *root != ':')
        root++; //JR Preskocim 'FSNAME'
    if (*root == ':')
        root++; //JR Preskocim ':'
    if (*root == '\\')
        root++; //JR Preskocim '\\'

    const char* s = text + offset;
    const char* end = text + pathLen;
    if (s >= end)
        return FALSE;
    if (s < root)
        offset = (int)(root - text);
    else
    {
        if (*s == '\\')
            s++;
        while (s < end && *s != '\\')
            s++;
        offset = (int)(s - text);
    }
    return s < end;
}

void WINAPI
CPluginFSInterface::ShowInfoDialog(const char* fsName, HWND parent)
{
}

BOOL WINAPI
CPluginFSInterface::ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo)
{
    //JR Nejdrive vyzkousim command na aktualni ceste pokud neni zadana abs. cesta
    char commandLine[MAX_PATH]; // CRAPI::CreateProcess neumi delsi nez MAX_PATH

    if (*command != '\\')
    {
        lstrcpyn(commandLine, Path, MAX_PATH);
        if (!CRAPI::PathAppend(commandLine, command, MAX_PATH))
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    }

    if (*command == '\\' || !CRAPI::CreateProcess(commandLine, NULL))
    {
        // Neuspech, tak jeste bez cesty, jen to, co zadal uzivatel
        if (lstrlen(command) >= MAX_PATH)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        if (!CRAPI::CreateProcess(command, NULL))
        {
            DWORD err = CRAPI::GetLastError();
            SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(err),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);

            return FALSE;
        }
    }

    *command = 0;
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                char* newName, BOOL& cancel)
{
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // zadost o standardni dialog

    // zkontrolujeme zadane jmeno (syntakticky)
    char* s = newName;
    char buf[2 * MAX_PATH];
    while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
           *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
        s++;
    if (newName[0] == 0 || *s != 0)
    {
        SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(ERROR_INVALID_NAME),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        return FALSE; // chybne jmeno, nechame opravit
    }

    // zpracovani masky v newName
    SalamanderGeneral->MaskName(buf, 2 * MAX_PATH, file.Name, newName);
    lstrcpyn(newName, buf, MAX_PATH);

    // provedeni operace prejmenovani
    char nameFrom[MAX_PATH];
    char nameTo[MAX_PATH];
    strcpy(nameFrom, Path);
    strcpy(nameTo, Path);
    if (!CRAPI::PathAppend(nameFrom, file.Name, MAX_PATH) ||
        !CRAPI::PathAppend(nameTo, newName, MAX_PATH))
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (maskovani)
        return FALSE; // chyba -> std. dialog znovu
    }

    //JR TODO: ConfirmOverwrite + Delete

    if (!CRAPI::MoveFile(nameFrom, nameTo))
    {
        // (pripadny prepis souboru tady neresime, oznacime ho taky za chybu...)
        DWORD err = CRAPI::GetLastError();
        SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(err),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (maskovani)
        return FALSE; // chyba -> std. dialog znovu
    }
    else // operace probehla dobre - hlaseni zmeny na ceste (vyvola refresh) + vracime uspech
    {
        char cefsFileName[2 * MAX_PATH];
        if (SalamanderGeneral->StrICmp(nameFrom, nameTo) != 0)
        { // pokud nejde jen o zmenu velikosti pismen (CEFS neni case-sensitive)
            // odstranime z disk-cache zdroj operace (puvodni jmeno jiz neni platne)
            sprintf(cefsFileName, "%s:%s", fsName, nameFrom);
            // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
            // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
            SalamanderGeneral->ToLowerCase(cefsFileName);
            SalamanderGeneral->RemoveOneFileFromCache(cefsFileName);
            // pokud je mozny i prepis souboru, mel by se z disk-cache odstranit i cil operace (nastala "zmena souboru")
        }

        // zmena na ceste Path (bez podadresaru jen u prejmenovani souboru)
        sprintf(cefsFileName, "%s:%s", fsName, Path);
        SalamanderGeneral->PostChangeOnPathNotification(cefsFileName, isDir);

        return TRUE;
    }
}

void WINAPI
CPluginFSInterface::AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs)
{

    // otestujeme shodnost cest nebo aspon jejich prefixu (sanci maji jen cesty
    // na nas FS, diskove cesty a cesty na jine FS v 'path' se vylouci automaticky,
    // protoze se nemuzou nikdy shodovat s 'fsName'+':' na zacatku 'path2' nize)
    char path1[2 * MAX_PATH];
    char path2[2 * MAX_PATH];
    lstrcpyn(path1, path, 2 * MAX_PATH);
    sprintf(path2, "%s:%s", fsName, Path);
    SalamanderGeneral->SalPathRemoveBackslash(path1);
    SalamanderGeneral->SalPathRemoveBackslash(path2);
    int len1 = (int)strlen(path1);
    BOOL refresh = SalamanderGeneral->StrNICmp(path1, path2, len1) == 0 &&
                   (path2[len1] == 0 || includingSubdirs && path2[len1] == '\\');
    if (refresh)
        SalamanderGeneral->PostRefreshPanelFS(this); // pokud je FS v panelu, provedeme jeho refresh
}

BOOL WINAPI
CPluginFSInterface::CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
{
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // zadost o standardni dialog

    int type;
    BOOL isDir;
    char* secondPart;
    char nextFocus[MAX_PATH];
    char path[2 * MAX_PATH];
    int error;
    nextFocus[0] = 0;
    if (!SalamanderGeneral->SalParsePath(parent, newName, type, isDir, secondPart,
                                         TitleWMobileError, nextFocus,
                                         FALSE, NULL, NULL, &error, 2 * MAX_PATH))
    {
        if (error == SPP_EMPTYPATHNOTALLOWED) // prazdny retezec -> koncime bez provedeni operace
        {
            cancel = TRUE;
            return TRUE; // na navratove hodnote uz nezalezi
        }

        if (error == SPP_INCOMLETEPATH) // relativni cesta na FS, postavime absolutni cestu vlastnimi silami
        {
            int errTextID;
            if (!SalamanderGeneral->SalGetFullName(newName, &errTextID, Path, nextFocus))
            {
                char errBuf[MAX_PATH];
                errBuf[0] = 0;
                SalamanderGeneral->GetGFNErrorText(errTextID, errBuf, MAX_PATH);
                SalamanderGeneral->ShowMessageBox(errBuf, TitleWMobileError, MSGBOX_ERROR);
                return FALSE;
            }

            strcpy(path, fsName);
            strcat(path, ":");

            if (strlen(fsName) + strlen(newName) + 1 >= 2 * MAX_PATH)
            {
                SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                                 TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
                // 'newName' se vraci v puvodni verzi
                return FALSE; // chyba -> std. dialog znovu
            }
            else
                strcat(path, newName);

            strcpy(newName, path);
            secondPart = newName + strlen(fsName) + 1;
            type = PATH_TYPE_FS;
        }
        else
            return FALSE; // chyba -> std. dialog znovu
    }

    if (type != PATH_TYPE_FS)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SORRY_CREATEDIR1),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }

    if ((secondPart - newName) - 1 != (int)strlen(fsName) ||
        SalamanderGeneral->StrNICmp(newName, fsName, (int)(secondPart - newName) - 1) != 0)
    { // to neni CEFS
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SORRY_CREATEDIR2),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }

    if (secondPart[0] != '\\')
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SORRY_CREATEDIR3),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }

    // v plne ceste na toto FS mohl uzivatel take pouzit "." a ".." - odstranime je
    if (!SalamanderGeneral->SalRemovePointsFromPath(secondPart + 1))
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_INVALIDPATH),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' se vraci po uprave (expanzi cesty) + mozne uprave nekterych ".." a "."
        return FALSE; // chyba -> std. dialog znovu
    }

    // orizneme zbytecny backslash
    SalamanderGeneral->SalPathRemoveBackslash(secondPart);

    // konecne vytvorime adresar

    if (!CRAPI::CreateDirectory(secondPart, NULL))
    {
        DWORD err = CRAPI::GetLastError();
        SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(err),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }

    // operace probehla dobre - hlaseni zmeny na ceste (vyvola refresh) + vracime uspech
    // zmena na ceste (bez podadresaru)
    SalamanderGeneral->CutDirectory(secondPart); // musi jit (nemuze byt root)
    sprintf(path, "%s:%s", fsName, secondPart);
    SalamanderGeneral->PostChangeOnPathNotification(path, FALSE);
    strcpy(newName, nextFocus); // pokud byl zadan primo jen nazev adresare, vyfokusime ho v panelu

    return TRUE;
}

void WINAPI
CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                             CSalamanderForViewFileOnFSAbstract* salamander,
                             CFileData& file)
{
    if (!CRAPI::CheckConnection())
        CRAPI::ReInit();

    // sestavime unikatni jmeno souboru pro disk-cache (standardni salamanderovsky format cesty)
    char uniqueFileName[2 * MAX_PATH];
    strcpy(uniqueFileName, AssignedFSName);
    strcat(uniqueFileName, ":");
    strcat(uniqueFileName, Path);
    CRAPI::PathAppend(uniqueFileName + strlen(AssignedFSName) + 1, file.Name, MAX_PATH);
    // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
    // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
    SalamanderGeneral->ToLowerCase(uniqueFileName);

    // ziskame jmeno kopie souboru v disk-cache
    BOOL fileExists;
    const char* tmpFileName = salamander->AllocFileNameInCache(parent, uniqueFileName, file.Name, NULL, fileExists);
    if (tmpFileName == NULL)
        return; // fatal error

    // zjistime jestli je treba pripravit kopii souboru do disk-cache (download)
    BOOL newFileOK = FALSE;
    CQuadWord newFileSize(0, 0);
    if (!fileExists) // priprava kopie souboru (download) je nutna
    {
        const char* name = uniqueFileName + strlen(AssignedFSName) + 1;

        HWND mainWnd = parent;
        HWND parentWin;
        while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
            mainWnd = parentWin;
        // disablujeme 'mainWnd'

        CProgressDlg dlg(mainWnd, LoadStr(IDS_READ), LoadStr(IDS_READING), ooStatic); // aby nemodalni dialog mohl byt na stacku, dame 'ooStatic'

        dlg.Create();
        EnableWindow(mainWnd, FALSE);
        SetForegroundWindow(dlg.HWindow);

        dlg.Set(name, 0, TRUE);

        LPCTSTR errFileName = "";
        DWORD err = CRAPI::CopyFileToPC(name, tmpFileName, TRUE, &dlg, 0, 0, &errFileName);

        EnableWindow(mainWnd, TRUE);
        DestroyWindow(dlg.HWindow); // zavreme progress dialog

        if (err == 0) // kopie se povedla
        {
            newFileOK = TRUE; // pokud nevyjde zjistovani velikosti souboru, zustane newFileSize nula (neni az tak dulezite)
            HANDLE hFile = HANDLES_Q(CreateFile(tmpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                NULL, OPEN_EXISTING, 0, NULL));
            if (hFile != INVALID_HANDLE_VALUE)
            { // chybu ignorujeme, velikost souboru neni az tak dulezita
                DWORD err2;
                SalamanderGeneral->SalGetFileSize(hFile, newFileSize, err2); // chyby ignorujeme
                HANDLES(CloseHandle(hFile));
            }
        }
        else if (err != -1)
        {
            char buf[2 * MAX_PATH + 100];
            sprintf(buf, LoadStr(IDS_PATH_ERROR), errFileName, SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
            return;
        }
    }

    // otevreme viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!fileExists && !newFileOK || // viewer otevreme jen pokud je kopie souboru v poradku
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // pri chybe nulujeme "lock"
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // jeste musime zavolat FreeFileNameInCache do paru k AllocFileNameInCache (propojime
    // viewer a disk-cache)
    salamander->FreeFileNameInCache(uniqueFileName, fileExists, newFileOK,
                                    newFileSize, fileLock, fileLockOwner, FALSE);
}

BOOL WINAPI
CPluginFSInterface::Delete(const char* fsName, int mode, HWND parent, int panel,
                           int selectedFiles, int selectedDirs, BOOL& cancelOrError)
{
    cancelOrError = FALSE;
    if (mode == 1)
        return FALSE; // zadost o standardni dotaz

    char buf[2 * MAX_PATH]; // buffer pro texty chyb

    char rootPath[MAX_PATH], fileName[MAX_PATH], dfsFileName[2 * MAX_PATH];

    strcpy(rootPath, Path);

    // vyzvedneme hodnoty "Confirm on" z konfigurace
    BOOL ConfirmOnNotEmptyDirDelete, ConfirmOnSystemHiddenFileDelete, ConfirmOnSystemHiddenDirDelete;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMNEDIRDEL, &ConfirmOnNotEmptyDirDelete, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEDEL, &ConfirmOnSystemHiddenFileDelete, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHDIRDEL, &ConfirmOnSystemHiddenDirDelete, 4, NULL);

    BOOL skipAllSHFD = FALSE;   // skip all deletes of system or hidden files
    BOOL yesAllSHFD = FALSE;    // delete all system or hidden files
    BOOL skipAllSHDD = FALSE;   // skip all deletes of system or hidden dirs
    BOOL yesAllSHDD = FALSE;    // delete all system or hidden dirs
    BOOL skipAllErrors = FALSE; // skip all errors

    BOOL success = TRUE; // FALSE v pripade chyby nebo preruseni uzivatelem
    BOOL changeInSubdirs = FALSE;

    const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    CFileInfoArray array(10, 10);

    //JR nacteme vsechny soubory, ktere budeme mazat
    for (int block1 = 1;; block1++)
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // smazeme soubor/adresar
        //JR FindAllFilesInTree volam i pro jednotlive soubory.
        //JR Zjisti se tim, jestli stale jeste existuji a jake maji aktualni atributy
        if (f != NULL)
            success = CRAPI::FindAllFilesInTree(rootPath, f->Name, array, block1, FALSE);

        // zjistime jestli ma cenu pokracovat (pokud neni chyba a existuje jeste nejaka dalsi oznacena polozka)
        if (!success || focused || f == NULL)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    if (!success)
        return FALSE;

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disablujeme 'mainWnd'

    BOOL showProgressDialog = array.Count > 1;
    BOOL enableMainWnd = TRUE;
    CProgressDlg delDlg(mainWnd, LoadStr(IDS_DELETE), LoadStr(IDS_DELETING), ooStatic); // aby nemodalni dialog mohl byt na stacku, dame 'ooStatic'

    if (showProgressDialog)
    {
        EnableWindow(mainWnd, FALSE);
        delDlg.Create();
    }

    if (!showProgressDialog || delDlg.HWindow != NULL) // dialog se podarilo otevrit
    {
        if (showProgressDialog)
            SetForegroundWindow(delDlg.HWindow);

        int block = 0;
        int i;
        for (i = 0; success && i < array.Count; i++)
        {
            CFileInfo& fi = array[i];

            strcpy(fileName, rootPath);
            if (!CRAPI::PathAppend(fileName, fi.cFileName, MAX_PATH))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_PATHTOOLONG),
                                                  TitleWMobileError, MSGBOX_ERROR);
                success = FALSE;
                break;
            }

            if (showProgressDialog)
            {
                float progress = ((float)i / (float)array.Count);
                delDlg.Set(fileName, (DWORD)(progress * 1000), TRUE); // delayedPaint == TRUE, abychom nebrzdili
            }

            if (showProgressDialog && delDlg.GetWantCancel())
            {
                success = FALSE;
                break;
            }

            if (ConfirmOnNotEmptyDirDelete && i < array.Count - 1)
            {
                //JR block obsahuje vic nez jeden prvek => je to neprazdny top level adresar
                if (fi.block != block && fi.block == array[i + 1].block)
                {
                    //JR Posledni cesta v blocku je vlastni adresar
                    int j;
                    for (j = i + 1; j < array.Count && array[j].block == fi.block; j++)
                    {
                    }
                    j--;

                    sprintf(buf, LoadStr(IDS_YESNO_DELETENOEMPTYDIR), array[j].cFileName);
                    int res = SalamanderGeneral->ShowMessageBox(buf, TitleWMobileQuestion, MSGBOX_EX_QUESTION);
                    if (res == IDNO)
                    {
                        i = j; //JR preskocim zbytek blocku
                        continue;
                    }
                    else if (res != IDYES)
                    {
                        success = FALSE;
                        break;
                    }
                }
            }

            block = fi.block;

            if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                BOOL skip = FALSE;
                if (ConfirmOnSystemHiddenDirDelete &&
                    (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllSHDD && !yesAllSHDD)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, fileName,
                                                                    LoadStr(IDS_YESNO_DELETEHIDDENDIR), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            yesAllSHDD = TRUE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllSHDD = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break; // DIALOG_CANCEL
                        }
                    }
                    else // skip all or delete all
                    {
                        if (skipAllSHDD)
                            skip = TRUE;
                    }
                }

                if (success && !skip) // neni cancel ani skip
                {
                    skip = FALSE;
                    while (1)
                    {
                        if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                            CRAPI::SetFileAttributes(fileName, FILE_ATTRIBUTE_ARCHIVE);

                        if (!CRAPI::RemoveDirectory(fileName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = CRAPI::GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                                switch (res)
                                {
                                case DIALOG_RETRY:
                                    break;

                                case DIALOG_SKIPALL:
                                    skipAllErrors = TRUE;
                                case DIALOG_SKIP:
                                    skip = TRUE;
                                    break;

                                default:
                                    success = FALSE;
                                    break; // DIALOG_CANCEL
                                }
                            }
                            else
                                skip = TRUE;
                        }
                        else
                        {
                            sprintf(dfsFileName, "%s:%s", fsName, fileName);
                            // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
                            // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
                            SalamanderGeneral->ToLowerCase(dfsFileName);
                            // odstranime z disk-cache kopii smazaneho souboru (je-li cachovany)
                            SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
                            changeInSubdirs = TRUE; // zmeny mohly nastat i v podadresarich
                            break;                  // uspesny delete
                        }
                        if (!success || skip)
                            break;
                    }
                }
            }
            else
            {
                BOOL skip = FALSE;
                if (ConfirmOnSystemHiddenFileDelete &&
                    (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllSHFD && !yesAllSHFD)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, fileName,
                                                                    LoadStr(IDS_YESNO_DELETEHIDDENFILE), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            yesAllSHFD = TRUE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllSHFD = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break; // DIALOG_CANCEL
                        }
                    }
                    else // skip all or delete all
                    {
                        if (skipAllSHFD)
                            skip = TRUE;
                    }
                }

                if (success && !skip) // neni cancel ani skip
                {
                    skip = FALSE;
                    while (1)
                    {
                        if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                            CRAPI::SetFileAttributes(fileName, FILE_ATTRIBUTE_ARCHIVE);

                        if (!CRAPI::DeleteFile(fileName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = CRAPI::GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                                switch (res)
                                {
                                case DIALOG_RETRY:
                                    break;

                                case DIALOG_SKIPALL:
                                    skipAllErrors = TRUE;
                                case DIALOG_SKIP:
                                    skip = TRUE;
                                    break;

                                default:
                                    success = FALSE;
                                    break; // DIALOG_CANCEL
                                }
                            }
                            else
                                skip = TRUE;
                        }
                        else
                        {
                            sprintf(dfsFileName, "%s:%s", fsName, fileName);
                            // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
                            // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
                            SalamanderGeneral->ToLowerCase(dfsFileName);
                            // odstranime z disk-cache kopii smazaneho souboru (je-li cachovany)
                            SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
                            break; // uspesny delete
                        }
                        if (!success || skip)
                            break;
                    }
                }
            }
        }

        if (showProgressDialog)
        {
            // enablujeme 'mainWnd' (jinak ho nemuze Windows vybrat jako foreground/aktivni okno)
            EnableWindow(mainWnd, TRUE);
            enableMainWnd = FALSE;

            DestroyWindow(delDlg.HWindow); // zavreme progress dialog
        }
    }

    // enablujeme 'mainWnd' (nenastala zmena foreground okna - progress se vubec neotevrel)
    if (showProgressDialog && enableMainWnd)
        EnableWindow(mainWnd, TRUE);

    SalamanderGeneral->RestoreFocusInSourcePanel();

    // zmena na ceste Path (bez podadresaru jen pokud se mazaly jen soubory)
    sprintf(dfsFileName, "%s:%s", fsName, Path);
    SalamanderGeneral->PostChangeOnPathNotification(dfsFileName, changeInSubdirs);

    return success;
}

static BOOL WINAPI CEFS_IsTheSamePath(const char* path1, const char* path2)
{
    while (*path1 != 0 && LowerCase[*path1] == LowerCase[*path2])
    {
        path1++;
        path2++;
    }
    if (*path1 == '\\')
        path1++;
    if (*path2 == '\\')
        path2++;
    return *path1 == 0 && *path2 == 0;
}

static void GetFileData(const char* name, char (&buf)[100])
{
    buf[0] = '?';
    buf[1] = 0;
    char tmp[50];

    WIN32_FIND_DATA data;

    HANDLE find = FindFirstFile(name, &data);
    if (find == INVALID_HANDLE_VALUE)
        return;

    CQuadWord size(data.nFileSizeLow, data.nFileSizeHigh);
    SalamanderGeneral->NumberToStr(buf, size);

    FILETIME time;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&data.ftLastWriteTime, &time);
    FileTimeToSystemTime(&time, &st);

    if (!GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, tmp, 50))
        sprintf(tmp, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);

    strcat(buf, ", ");
    strcat(buf, tmp);

    if (!GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, tmp, 50))
        sprintf(tmp, "%u:%u:%u", st.wHour, st.wMinute, st.wSecond);

    strcat(buf, ", ");
    strcat(buf, tmp);

    FindClose(find);
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                     int panel, int selectedFiles, int selectedDirs,
                                     char* targetPath, BOOL& operationMask,
                                     BOOL& cancelOrHandlePath, HWND dropTarget)
{
    char path[2 * MAX_PATH];
    operationMask = FALSE;
    cancelOrHandlePath = FALSE;
    if (mode == 1) // prvni volani CopyOrMoveFromFS
    {
        if (*targetPath == 0)
        {
            int targetPanel = (panel == PANEL_LEFT ? PANEL_RIGHT : PANEL_LEFT);
            int type;
            char* fs;
            if (SalamanderGeneral->GetPanelPath(targetPanel, path, 2 * MAX_PATH, &type, &fs))
            {
                if (type == PATH_TYPE_FS && fs - path == (int)strlen(fsName) &&
                    SalamanderGeneral->StrNICmp(path, fsName, (int)(fs - path)) == 0)
                {
                    strcpy(targetPath, path);
                }
            }
        }
        // pokud je nejaka cesta navrzena, pridame k ni masku *.* (budeme zpracovavat operacni masky)
        if (*targetPath != 0)
        {
            SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
            SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_TARGET); // default akce = prace s cestou v cilovem panelu
        }
        return FALSE; // zadost o standardni dialog
    }

    if (mode == 4) // chyba pri std. Salamanderovskem zpracovani cilove cesty
    {
        // 'targetPath' obsahuje chybnou cestu, uzivatelovi byla chyba ohlasena, zbyva jen
        // ho nechat provest editaci cilove cesty
        return FALSE; // zadost o standardni dialog
    }

    char buf[3 * MAX_PATH + 100];
    char nextFocus[MAX_PATH];
    nextFocus[0] = 0;

    BOOL diskPath = TRUE;  // pri 'mode'==3 je v 'targetPath' windowsova cesta (FALSE = cesta na toto FS)
    char* userPart = NULL; // ukazatel do 'targetPath' na user-part FS cesty (pri 'diskPath' FALSE)
    BOOL rename = FALSE;   // je-li TRUE, jde o prejmenovani/kopirovani adresare do sebe sama

    if (mode == 2) // prisel retezec ze std. dialogu od usera
    {
        // sami zpracujeme relativni cesty (to Salamander nemuze udelat)
        if ((targetPath[0] != '\\' || targetPath[1] != '\\') && // neni UNC cesta
            (targetPath[0] == 0 || targetPath[1] != ':'))       // neni normalni diskova cesta
        {                                                       // neni windowsova cesta, ani archivova cesta
            userPart = strchr(targetPath, ':');
            if (userPart == NULL) // cesta nema fs-name, je tedy relativni
            {                     // relativni cesta s ':' zde neni mozna (nelze rozlisit od plne cesty na nejaky FS)

                // pro diskove cesty by bylo vyhodne pouzit SalGetFullName:
                // SalamanderGeneral->SalGetFullName(targetPath, &errTextID, Path, nextFocus) + osetrit chyby
                // pak uz by stacilo jen vlozit fs-name pred ziskanou cestu
                // tady si ale radsi predvedeme vlastni implementaci (pouziti SalRemovePointsFromPath a dalsich):

                char* s = strchr(targetPath, '\\');
                if (s == NULL || *(s + 1) == 0)
                {
                    int l;
                    if (s != NULL)
                        l = (int)(s - targetPath);
                    else
                        l = (int)strlen(targetPath);
                    if (l > MAX_PATH - 1)
                        l = MAX_PATH - 1;
                    memcpy(nextFocus, targetPath, l);
                    nextFocus[l] = 0;
                }

                strcpy(path, fsName);
                s = path + strlen(path);
                *s++ = ':';
                userPart = s;
                BOOL tooLong = FALSE;
                int rootLen = 1;
                *s = '\\';
                if (targetPath[0] == '\\') // "\\path" -> skladame root + newName
                {
                    s += rootLen;
                    int len = (int)strlen(targetPath + 1); // bez uvodniho '\\'
                    if (len + rootLen >= MAX_PATH)
                        tooLong = TRUE;
                    else
                    {
                        memcpy(s, targetPath + 1, len);
                        *(s + len) = 0;
                    }
                }
                else // "path" -> skladame Path + newName
                {
                    int pathLen = (int)strlen(Path);
                    if (pathLen < rootLen)
                        rootLen = pathLen;
                    strcpy(s + rootLen, Path + rootLen); // root uz je tam nakopirovany
                    tooLong = !CRAPI::PathAppend(s, targetPath, MAX_PATH);
                }

                if (tooLong)
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                                     TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                    // 'targetPath' se vraci v nezmenene podobe (co uzivatel zadal)
                    return FALSE; // chyba -> std. dialog znovu
                }

                strcpy(targetPath, path);
                userPart = targetPath + (userPart - path);
            }
            else
                userPart++;

            // FS cilova cesta ('targetPath' - plna cesta, 'userPart' - ukazatel do plne cesty na user-part)
            // na tomto miste muze plugin zpracovat FS cesty (jak sve vlastni, tak cizi)
            // Salamander zatim neumi tyto cesty zpracovat, casem mozna umozni sled zakladnich
            // operaci pres TEMP (napr. download z FTP do TEMPu, pak upload z TEMPu na FTP - pokud
            // lze udelat efektivneji (u FTP to lze), mel by to plugin zvladnout zde)

            if ((userPart - targetPath) - 1 == (int)strlen(fsName) &&
                SalamanderGeneral->StrNICmp(targetPath, fsName, (int)(userPart - targetPath) - 1) == 0)
            { // je to CEFS (jinak nechame zpracovat standardne Salamanderem)
                BOOL invPath = (userPart[0] != '\\');

                int rootLen = 0;
                if (!invPath)
                {
                    rootLen = 1;
                    int userPartLen = (int)strlen(userPart);
                    if (userPartLen < rootLen)
                        rootLen = userPartLen;
                }

                // v plne ceste na toto FS mohl uzivatel take pouzit "." a ".." - odstranime je
                if (invPath || !SalamanderGeneral->SalRemovePointsFromPath(userPart + rootLen))
                {
                    // navic by se dalo vypsat 'err' (pri 'invPath' TRUE), zde pro jednoduchost ignorujeme
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_INVALIDPATH),
                                                     TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                    // 'targetPath' se vraci po uprave (expanzi cesty) + mozne uprave nekterych ".." a "."
                    return FALSE; // chyba -> std. dialog znovu
                }

                // orizneme zbytecny backslash
                int l = (int)strlen(userPart);
                BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
                if (l > 1 && userPart[l - 1] == '\\') // typ cesty "\path\"
                    userPart[l - 1] = 0;              // orez backslashe

                // rozanalyzovani cesty - nalezeni existujici casti, neexistujici casti a operacni masky
                //
                // - zjistit jaka cast cesty existuje a jestli je to soubor nebo adresar,
                //   podle vysledku vybrat o co jde:
                //   - zapis na cestu (prip. s neexistujici casti) s maskou - maska je posledni neexistujici cast cesty,
                //     za kterou jiz neni backslash (overit jestli u vice zdrojovych souboru/adresaru je
                //     v masce '*' nebo aspon '?', jinak nesmysl -> jen jedno cilove jmeno)
                //   - rucni "change-case" jmena podadresare pres Move (zapis na cestu, ktera je zaroven zdroj
                //     operace (je focusena/oznacena-jako-jedina v panelu); jmena se muzou lisit ve velikosti pismen)
                //   - zapis do archivu (v ceste je soubor archivu nebo to ani nemusi byt archiv, pak jde o
                //     chybu "Salamander nevi jak tento soubor otevrit")
                //   - prepis souboru (cela cesta je jen jmeno ciloveho souboru; nesmi koncit na backslash)

                // zjisteni kam az cesta existuje (rozdeleni na existujici a neexistujici cast)
                HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                char* end = targetPath + strlen(targetPath);
                char* afterRoot = userPart + 1; //JR root = '\'
                char lastChar = 0;
                BOOL pathIsDir = TRUE;
                BOOL pathError = FALSE;

                // pokud je v ceste maska, odrizneme ji bez volani GetFileAttributes
                if (end > afterRoot) // jeste neni jen root
                {
                    char* end2 = end;
                    BOOL cut = FALSE;
                    while (*--end2 != '\\') // je jiste, ze aspon za root-cestou je jeden '\\'
                    {
                        if (*end2 == '*' || *end2 == '?')
                            cut = TRUE;
                    }
                    if (cut) // ve jmene je maska -> orizneme
                    {
                        end = end2;
                        lastChar = *end;
                        *end = 0;
                    }
                }

                while (end > afterRoot) // jeste neni jen root
                {
                    DWORD attrs = CRAPI::GetFileAttributes(userPart);
                    if (attrs != 0xFFFFFFFF) // tato cast cesty existuje
                    {
                        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // je to soubor
                        {
                            // existujici cesta nema obsahovat jmeno souboru (viz SalSplitGeneralPath), orizneme...
                            *end = lastChar;   // opravime 'targetPath'
                            pathIsDir = FALSE; // existujici cast cesty je soubor
                            while (*--end != '\\')
                                ;            // je jiste, ze aspon za root-cestou je jeden '\\'
                            lastChar = *end; // aby se nezrusila cesta
                            break;
                        }
                        else
                            break;
                    }
                    else
                    {
                        DWORD err = CRAPI::GetLastError();
                        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_INVALID_NAME &&
                            err != ERROR_PATH_NOT_FOUND && err != ERROR_BAD_PATHNAME &&
                            err != ERROR_DIRECTORY) // divna chyba - jen vypiseme
                        {
                            sprintf(buf, LoadStr(IDS_PATH_ERROR), targetPath, SalamanderGeneral->GetErrorText(err));
                            SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                            pathError = TRUE;
                            break; // ohlasime chybu
                        }
                    }

                    *end = lastChar; // obnova 'targetPath'
                    while (*--end != '\\')
                        ; // je jiste, ze aspon za root-cestou je jeden '\\'
                    lastChar = *end;
                    *end = 0;
                }
                *end = lastChar; // opravime 'targetPath'
                SetCursor(oldCur);

                if (!pathError) // rozdeleni probehlo bez chyby
                {
                    if (*end == '\\')
                        end++;

                    const char* dirName = NULL;
                    const char* curPath = NULL;
                    if (selectedFiles + selectedDirs <= 1)
                    {
                        const CFileData* f;
                        if (selectedFiles == 0 && selectedDirs == 0)
                            f = SalamanderGeneral->GetPanelFocusedItem(panel, NULL);
                        else
                        {
                            int index = 0;
                            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, NULL);
                        }
                        dirName = f->Name;

                        sprintf(path, "%s:%s", fsName, Path);
                        curPath = path;
                    }

                    char newDirs[MAX_PATH], *mask;
                    if (SalamanderGeneral->SalSplitGeneralPath(parent, TitleWMobile, TitleWMobileError, selectedFiles + selectedDirs,
                                                               targetPath, afterRoot, end, pathIsDir,
                                                               backslashAtEnd, dirName, curPath, mask, newDirs,
                                                               CEFS_IsTheSamePath))
                    {
                        if (newDirs[0] != 0) // na cilove ceste je potreba vytvorit nejake podadresare
                        {
                            if (!CRAPI::CheckAndCreateDirectory(userPart, parent, true, NULL, 0, NULL))
                            {
                                char* e = targetPath + strlen(targetPath); // oprava 'targetPath' (spojeni 'targetPath' a 'mask')
                                if (e > targetPath && *(e - 1) != '\\')
                                    *e++ = '\\';
                                if (e != mask)
                                    memmove(e, mask, strlen(mask) + 1); // je-li potreba, prisuneme masku
                                pathError = TRUE;
                            }
                        }
                        else if (dirName != NULL && curPath != NULL && SalamanderGeneral->StrICmp(dirName, mask) == 0 &&
                                 CEFS_IsTheSamePath(targetPath, curPath))
                        {
                            // prejmenovani/kopirovani adresare do sebe sama (az na velikost pismen v nazvu) - "change-case"
                            // nelze povazovat za operacni masku (zadana cilova cesta existuje, rozdeleni na masku je
                            // vysledkem analyzy)

                            rename = TRUE;
                        }
                    }
                    else
                        pathError = TRUE;
                }

                if (pathError)
                {
                    // 'targetPath' se vraci po uprave (expanzi cesty) + uprave ".." a "." + mozna pridane masky
                    return FALSE; // chyba -> std. dialog znovu
                }
                diskPath = FALSE; // cestu na toto FS se podarilo rozanalyzovat
            }
        }

        if (diskPath)
        {
            // windowsova cesta, cesta do archivu nebo na neznamy FS - pustime std. zpracovani
            operationMask = TRUE; // operacni masky podporujeme
            cancelOrHandlePath = TRUE;
            return FALSE; // nechame cestu zpracovat v Salamanderovi
        }
    }

    const char* opMask = NULL; // maska operace
    if (mode == 5)             // cil operace byl zadan pres drag&drop
    {
        // pokud jde o diskovou cestu, pak jen nastavime masku operace a pokracujeme (stejne s 'mode'==3);
        // pokud jde o cestu do archivu, vyhodime error "not supported"; pokud jde o cestu do CEFS, dame
        // 'diskPath'=FALSE a napocitame 'userPart' (ukazuje na user-part CEFS cesty); pokud jde o cestu
        // do jineho FS, vyhodime error "not supported"

        BOOL ok = FALSE;
        opMask = "*.*";
        int type;
        char* secondPart;
        BOOL isDir;
        if (targetPath[0] != 0 && targetPath[1] == ':' ||   // diskova cesta (C:\path)
            targetPath[0] == '\\' && targetPath[1] == '\\') // UNC cesta (\\server\share\path)
        {                                                   // pridame na konec backslash, aby slo k kazdem pripade o cestu (pri 'mode'==5 jde vzdy o cestu)
            SalamanderGeneral->SalPathAddBackslash(targetPath, MAX_PATH);
        }
        if (SalamanderGeneral->SalParsePath(parent, targetPath, type, isDir, secondPart,
                                            TitleWMobileError, NULL, FALSE,
                                            NULL, NULL, NULL, 2 * MAX_PATH))
        {
            switch (type)
            {
            case PATH_TYPE_WINDOWS:
            {
                if (*secondPart != 0)
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_TARGETPATHNOEXISTS),
                                                     TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                }
                else
                    ok = TRUE;
                break;
            }

            case PATH_TYPE_FS:
            {
                userPart = secondPart;
                if ((userPart - targetPath) - 1 == (int)strlen(fsName) &&
                    SalamanderGeneral->StrNICmp(targetPath, fsName, (int)(userPart - targetPath) - 1) == 0)
                { // je to CEFS
                    diskPath = FALSE;
                    ok = TRUE;
                }
                else // jine FS, jen ohlasime "not supported"
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_COPYTOOTHERFS), TitleWMobileError,
                                                     MB_OK | MB_ICONEXCLAMATION);
                }
                break;
            }

            //case PATH_TYPE_ARCHIVE:
            default: // archiv, jen ohlasime "not supported"
            {
                SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_COPYTOARCHIVES),
                                                 TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                break;
            }
            }
        }
        if (!ok)
        {
            cancelOrHandlePath = TRUE;
            return TRUE;
        }
    }

    // 'mode' je 2, 3 nebo 5

    // najdeme si masku operace (cilova cesta je v 'targetPath')
    if (opMask == NULL)
    {
        opMask = targetPath;
        while (*opMask != 0)
            opMask++;
        opMask++;
    }

    // priprava bufferu pro jmena
    char sourceName[MAX_PATH]; // buffer pro plne jmeno na disku (zdroj operace lezi u CEFS na disku)
    strcpy(sourceName, Path);
    char* endSource = sourceName + strlen(sourceName); // misto pro jmena z panelu
    if (endSource > sourceName && *(endSource - 1) != '\\')
    {
        *endSource++ = '\\';
        *endSource = 0;
    }
    int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // max. pocet znaku pro jmeno z panelu

    char cefsSourceName[2 * MAX_PATH]; // buffer pro plne jmeno na CEFS (pro hledani zdroje operace v disk-cache)
    sprintf(cefsSourceName, "%s:%s", fsName, sourceName);
    // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
    // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
    SalamanderGeneral->ToLowerCase(cefsSourceName);
    char* endCEFSSource = cefsSourceName + strlen(cefsSourceName);                // misto pro jmena z panelu
    int endCEFSSourceSize = 2 * MAX_PATH - (int)(endCEFSSource - cefsSourceName); // max. pocet znaku pro jmeno z panelu

    char targetName[MAX_PATH]; // buffer pro plne jmeno na disku (pokud cil operace lezi na disku)
    targetName[0] = 0;
    char* endTarget = targetName;
    int endTargetSize = MAX_PATH;

    if (diskPath) // windowsova cilova cesta
        strcpy(targetName, targetPath);
    else
        strcpy(targetName, userPart);

    endTarget = targetName + strlen(targetName); // misto pro cilove jmeno
    if (endTarget > targetName && *(endTarget - 1) != '\\')
    {
        *endTarget++ = '\\';
        *endTarget = 0;
    }
    endTargetSize = MAX_PATH - (int)(endTarget - targetName); // max. pocet znaku pro jmeno z panelu

    const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;                     // FALSE v pripade chyby nebo preruseni uzivatelem
    BOOL skipAllErrors = FALSE;              // skip all errors
    BOOL sourcePathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na zdrojove ceste (operace move)
    BOOL subdirsOfSourcePathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich zdrojove cesty
    BOOL targetPathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na cilove ceste
    BOOL subdirsOfTargetPathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich cilove cesty
    BOOL skipAllOverwrite = FALSE;
    BOOL skipAllOverwriteSystemHidden = FALSE;

    rename = !copy && CEFS_IsTheSamePath(sourceName, targetName);

    // vyzvedneme hodnoty "Confirm on" z konfigurace
    BOOL ConfirmOnFileOverwrite, ConfirmOnSystemHiddenFileOverwrite;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    CFileInfoArray array(10, 10);

    while (1)
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // provedeme copy/move na souboru/adresari
        if (f != NULL)
        {
            if (rename)
            {
                CFileInfo fi;
                strcpy(fi.cFileName, f->Name);
                fi.dwFileAttributes = f->Attr;
                fi.size = 100; //JR Operace prejmenovani trva priblizne stejne dlouho bez ohledu na velikost
                fi.block = 0;

                array.Add(fi);
                if (array.State != etNone)
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                    TRACE_E("Low memory");
                    success = false;
                }
            }
            else
                success = CRAPI::FindAllFilesInTree(Path, f->Name, array, 0, TRUE);
        }

        // zjistime jestli ma cenu pokracovat (pokud neni cancel a existuje jeste nejaka dalsi oznacena polozka)
        if (!success || focused || f == NULL)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disablujeme 'mainWnd'

    CProgress2Dlg dlg(mainWnd, LoadStr(copy ? IDS_COPY : IDS_MOVE), LoadStr(copy ? IDS_COPYING : IDS_MOVING), LoadStr(IDS_TO), ooStatic); // aby nemodalni dialog mohl byt na stacku, dame 'ooStatic'

    dlg.Create();
    EnableWindow(mainWnd, FALSE);
    SetForegroundWindow(dlg.HWindow);

    INT64 totalsize = 0, copied = 0;
    int i;
    for (i = 0; i < array.Count; i++)
        totalsize += (array[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 100 : array[i].size;

    for (/*int*/ i = 0; i < array.Count; i++)
    {
        CFileInfo& fi = array[i];

        char* targetFile = SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, fi.cFileName, opMask);

        if ((int)strlen(fi.cFileName) >= endSourceSize || (int)strlen(targetFile) >= endTargetSize)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        lstrcpyn(endSource, fi.cFileName, endSourceSize);
        lstrcpyn(endCEFSSource, fi.cFileName, endCEFSSourceSize);
        // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
        // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
        SalamanderGeneral->ToLowerCase(endCEFSSource);

        // slozime cilove jmeno - zjednodusene o test chyby LoadStr(IDS_ERR_NAMETOOLONG)
        lstrcpyn(endTarget, targetFile, endTargetSize);

        isDir = (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (copy && SalamanderGeneral->StrICmp(sourceName, targetName) == 0)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(isDir ? IDS_ERR_COPYDIRTOITSELF : IDS_ERR_COPYFILETOITSELF), TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        dlg.Set(sourceName, targetName, FALSE);

        BOOL skip = FALSE;
        BOOL fileMoved = FALSE;
        if (isDir && !rename)
        {
            if (fi.block == -1)
            {
                while (1)
                {
                    if (!(diskPath ? SalamanderGeneral->CheckAndCreateDirectory(targetName, parent, true, NULL, 0, NULL) : CRAPI::CheckAndCreateDirectory(targetName, parent, true, NULL, 0, NULL)))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = diskPath ? GetLastError() : CRAPI::GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, targetName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                        else
                            skip = TRUE;
                    }
                    else
                    {
                        targetPathChanged = TRUE;
                        subdirsOfTargetPathChanged = TRUE;
                        break; // uspesne vytvoreny adresar
                    }
                } // end of while(1)
            }
        }
        else
        {
            DWORD attr = 0xFFFFFFFF;

            if (!rename || SalamanderGeneral->StrICmp(sourceName, targetName) != 0) //Neni to prosta zmena velikosti
            {
                if (diskPath)
                    attr = SalamanderGeneral->SalGetFileAttributes(targetName);
                else
                    attr = CRAPI::GetFileAttributes(targetName);
            }

            if (attr != 0xFFFFFFFF)
            {
                if (!skipAllOverwrite)
                {
                    if (ConfirmOnFileOverwrite)
                    {
                        char sourceData[100], targetData[100];

                        CRAPI::GetFileData(sourceName, sourceData);
                        if (diskPath)
                            GetFileData(targetName, targetData);
                        else
                            CRAPI::GetFileData(targetName, targetData);

                        int res = SalamanderGeneral->DialogOverwrite(parent, BUTTONS_YESALLSKIPCANCEL, targetName, targetData, sourceName, sourceData);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllOverwrite = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break;
                        }
                    }
                }
                else
                    skip = TRUE;

                if (success && !skip && ConfirmOnSystemHiddenFileOverwrite && (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllOverwriteSystemHidden)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, targetName,
                                                                    LoadStr(IDS_YESNO_OVERWRITEHIDDENFILE), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnSystemHiddenFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllOverwriteSystemHidden = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break;
                        }
                    }
                    else
                        skip = TRUE;
                }
            }

            if (success && !skip)
            {
                while (1)
                {
                    DWORD err = 0;
                    LPCTSTR errFileName = "";
                    if (diskPath) // windowsova cilova cesta //JR
                    {
                        if (attr != 0xFFFFFFFF &&
                            (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY)))
                            SetFileAttributes(targetName, FILE_ATTRIBUTE_ARCHIVE);

                        err = CRAPI::CopyFileToPC(sourceName, targetName, FALSE, &dlg, copied, totalsize, &errFileName);
                    }
                    else
                    {
                        if (attr != 0xFFFFFFFF &&
                            (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY)))
                            CRAPI::SetFileAttributes(targetName, FILE_ATTRIBUTE_ARCHIVE);

                        if (copy)
                        {
                            err = CRAPI::CopyFile(sourceName, targetName, FALSE, &dlg, copied, totalsize, &errFileName);
                        }
                        else
                        {
                            if (attr != 0xFFFFFFFF)
                                CRAPI::DeleteFile(targetName);

                            if (!CRAPI::MoveFile(sourceName, targetName))
                                err = CRAPI::GetLastError();
                            else if (dlg.GetWantCancel())
                                err = -1;
                            if (err == 0)
                                fileMoved = TRUE;
                        }
                    }

                    if (err != 0)
                    {
                        if (err == -1) //JR preruseno uzivatelem
                            success = FALSE;
                        else if (!skipAllErrors)
                        {
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, errFileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                        else
                            skip = TRUE;
                    }
                    else
                    {
                        targetPathChanged = TRUE;
                        if (fileMoved)
                            sourcePathChanged = TRUE;
                        break; // uspesne nakopirovano
                    }

                    if (!success || skip)
                        break;
                } // end of while(1)
            }
        }

        if (success && !copy && !skip && !fileMoved) // jde o "move" a soubor nebyl skipnuty -> smazeme zdrojovy soubor
        {
            while (1)
            {
                if (isDir)
                {
                    if (fi.block != -1)
                    {
                        if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                            CRAPI::SetFileAttributes(sourceName, FILE_ATTRIBUTE_ARCHIVE);

                        if (!CRAPI::RemoveDirectory(sourceName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = CRAPI::GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, cefsSourceName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                                switch (res)
                                {
                                case DIALOG_RETRY:
                                    break;

                                case DIALOG_SKIPALL:
                                    skipAllErrors = TRUE;
                                case DIALOG_SKIP:
                                    skip = TRUE;
                                    break;

                                default:
                                    success = FALSE;
                                    break; // DIALOG_CANCEL
                                }
                            }
                            else
                                skip = TRUE;
                        }
                        else
                        {
                            // odstranime z disk-cache kopii smazaneho souboru (je-li cachovany)
                            SalamanderGeneral->RemoveFilesFromCache(cefsSourceName);

                            sourcePathChanged = TRUE;
                            subdirsOfSourcePathChanged = TRUE;
                            break; // uspesny RemoveDirectory
                        }
                    }
                    else
                        break;
                }
                else
                {
                    // zrusime soubor na CEFS
                    if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                        CRAPI::SetFileAttributes(sourceName, FILE_ATTRIBUTE_ARCHIVE);

                    if (!CRAPI::DeleteFile(sourceName))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = CRAPI::GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, cefsSourceName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                        else
                            skip = TRUE;
                    }
                    else
                    {
                        // odstranime z disk-cache kopii smazaneho souboru (je-li cachovany)
                        SalamanderGeneral->RemoveOneFileFromCache(cefsSourceName);

                        sourcePathChanged = TRUE;
                        break; // uspesny delete
                    }
                }
                if (!success || skip)
                    break;
            }
        }

        if (success)
        {
            copied += isDir ? 100 : fi.size;

            float progress = (totalsize ? ((float)copied / (float)totalsize) : 0) * 1000;
            dlg.SetProgress((DWORD)progress, 0, FALSE);
        }
        else
            break; // zjistime jestli ma cenu pokracovat (pokud neni cancel)
    }

    EnableWindow(mainWnd, TRUE);
    DestroyWindow(dlg.HWindow); // zavreme progress dialog

    // zmena na zdrojove ceste Path (hlavne operace move)
    if (sourcePathChanged)
    {
        sprintf(path, "%s:%s", fsName, Path);
        SalamanderGeneral->PostChangeOnPathNotification(path, subdirsOfSourcePathChanged);
    }

    // zmena na cilove ceste targetPath
    if (targetPathChanged)
        SalamanderGeneral->PostChangeOnPathNotification(targetPath, subdirsOfTargetPathChanged);

    SalamanderGeneral->RestoreFocusInSourcePanel();

    if (success)
        strcpy(targetPath, nextFocus); // uspech
    else
        cancelOrHandlePath = TRUE; // error/cancel
    return TRUE;                   // uspech nebo error/cancel
}

static BOOL FindAllFilesInTree(LPCTSTR rootPath, char (&path)[MAX_PATH], LPCTSTR fileName, CFileInfoArray& array, BOOL dirFirst, int block)
{
    HANDLE find = INVALID_HANDLE_VALUE;

    WIN32_FIND_DATA data;
    char fullPath[MAX_PATH];
    strcpy(fullPath, rootPath);
    if (!SalamanderGeneral->SalPathAppend(fullPath, path, MAX_PATH) ||

        !SalamanderGeneral->SalPathAppend(fullPath, fileName, MAX_PATH))
        goto ONERROR_TOOLONG;

    find = FindFirstFile(fullPath, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err == ERROR_NO_MORE_FILES || err == ERROR_FILE_NOT_FOUND)
            return TRUE; //JR prazdny adresar, koncim

        char buf[2 * MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_PATH_ERROR), fullPath, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
        return FALSE;
    }

    for (;;)
    {
        //JR TODO: Toto nefunguje!
        if (SalamanderGeneral->GetSafeWaitWindowClosePressed())
        {
            if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_YESNO_CANCEL), TitleWMobileQuestion,
                                                  MSGBOX_QUESTION) == IDYES)
                goto ONERROR;
        }

        if (data.cFileName[0] != 0 &&
            (data.cFileName[0] != '.' || //JR Windows Mobile nevraci "." a ".." cesty, ale pro jistotu to osetrim
             (data.cFileName[1] != 0 && (data.cFileName[1] != '.' || data.cFileName[2] != 0))))
        {
            CFileInfo fi;
            strcpy(fi.cFileName, path);
            if (!SalamanderGeneral->SalPathAppend(fi.cFileName, data.cFileName, MAX_PATH))
                goto ONERROR_TOOLONG;

            fi.dwFileAttributes = data.dwFileAttributes;

            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                fi.size = 0;

                if (dirFirst)
                {
                    fi.block = -1;
                    array.Add(fi);
                    if (array.State != etNone)
                    {
                        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                        TRACE_E("Low memory");
                        goto ONERROR;
                    }
                }

                int len = (int)strlen(path);
                if (!SalamanderGeneral->SalPathAppend(path, data.cFileName, MAX_PATH))
                    goto ONERROR_TOOLONG;

                if (!FindAllFilesInTree(rootPath, path, "*.*", array, dirFirst, block))
                    goto ONERROR; //JR Chyba uz byla hlasena

                path[len] = 0;
            }
            else
                fi.size = data.nFileSizeLow;

            fi.block = block;
            array.Add(fi);
            if (array.State != etNone)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                TRACE_E("Low memory");
                goto ONERROR;
            }
        }

        if (!FindNextFile(find, &data))
        {
            if (GetLastError() == ERROR_NO_MORE_FILES)
                break; //JR Vse v poradku, koncim

            DWORD err = GetLastError();
            SalamanderGeneral->ShowMessageBox(SalamanderGeneral->GetErrorText(err), TitleWMobileError, MSGBOX_ERROR);
            FindClose(find);
            return FALSE;
        }
    }

    FindClose(find);
    return TRUE;

ONERROR_TOOLONG:
    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_PATHTOOLONG),
                                      TitleWMobileError, MSGBOX_ERROR);
ONERROR:
    if (find != INVALID_HANDLE_VALUE)
        FindClose(find);
    return FALSE;
}

static BOOL FindAllFilesInTree(const char* rootPath, const char* fileName, CFileInfoArray& array, BOOL dirFirst, int block)
{
    char path[MAX_PATH];
    path[0] = 0;

    return FindAllFilesInTree(rootPath, path, fileName, array, dirFirst, block);
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                           const char* sourcePath, SalEnumSelection2 next,
                                           void* nextParam, int sourceFiles, int sourceDirs,
                                           char* targetPath, BOOL* invalidPathOrCancel)
{
    if (invalidPathOrCancel != NULL)
        *invalidPathOrCancel = FALSE;

    if (mode == 1)
    {
        // pridame k cil. ceste masku *.* (budeme zpracovavat operacni masky)
        SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
        return TRUE;
    }

    char cefsFileName[2 * MAX_PATH];
    char buf[3 * MAX_PATH + 100];

    if (mode != 2 && mode != 3)
        return FALSE; // neznamy 'mode'

    // v 'targetPath' je neupravena cesta zadana uzivatelem (jedine co o ni vime je, ze je
    // z tohoto FS, jinak by tuto metodu Salamander nevolal)
    char* userPart = strchr(targetPath, ':') + 1; // v 'targetPath' musi byt fs-name + ':'

    BOOL invPath = (userPart[0] != '\\');

    // provedeme kontrolu, jestli je mozne provest operaci v tomto FS; zaroven v plne ceste
    // na toto FS mohl uzivatel pouzit "." a ".." - odstranime je
    int rootLen = 0;
    if (!invPath)
    {
        rootLen = 1;
        int userPartLen = (int)strlen(userPart);
        if (userPartLen < rootLen)
            rootLen = userPartLen;
    }

    if (invPath || !SalamanderGeneral->SalRemovePointsFromPath(userPart + rootLen))
    {
        // navic by se dalo vypsat 'err' (pri 'invPath' TRUE), zde pro jednoduchost ignorujeme
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_INVALIDPATH),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'targetPath' se vraci po mozne uprave nekterych ".." a "."
        if (invalidPathOrCancel != NULL)
            *invalidPathOrCancel = TRUE;
        return FALSE; // nechame uzivatele cestu opravit
    }

    // orizneme zbytecny backslash
    int l = (int)strlen(userPart);
    BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
    if (l > 1 && userPart[l - 1] == '\\') // typ cesty "\path\"
        userPart[l - 1] = 0;              // orez backslashe

    // rozanalyzovani cesty - nalezeni existujici casti, neexistujici casti a operacni masky
    //
    // - zjistit jaka cast cesty existuje a jestli je to soubor nebo adresar,
    //   podle vysledku vybrat o co jde:
    //   - zapis na cestu (prip. s neexistujici casti) s maskou - maska je posledni neexistujici cast cesty,
    //     za kterou jiz neni backslash (overit jestli u vice zdrojovych souboru/adresaru je
    //     v masce '*' nebo aspon '?', jinak nesmysl -> jen jedno cilove jmeno)
    //   - zapis do archivu (v ceste je soubor archivu nebo to ani nemusi byt archiv, pak jde o
    //     chybu "Salamander nevi jak tento soubor otevrit")
    //   - prepis souboru (cela cesta je jen jmeno ciloveho souboru; nesmi koncit na backslash)

    // zjisteni kam az cesta existuje (rozdeleni na existujici a neexistujici cast)
    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    char* end = targetPath + strlen(targetPath);
    char* afterRoot = userPart + rootLen;
    char lastChar = 0;
    BOOL pathIsDir = TRUE;
    BOOL pathError = FALSE;

    // pokud je v ceste maska, odrizneme ji bez volani GetFileAttributes
    if (end > afterRoot) // jeste neni jen root
    {
        char* end2 = end;
        BOOL cut = FALSE;
        while (*--end2 != '\\') // je jiste, ze aspon za root-cestou je jeden '\\'
        {
            if (*end2 == '*' || *end2 == '?')
                cut = TRUE;
        }
        if (cut) // ve jmene je maska -> orizneme
        {
            end = end2;
            lastChar = *end;
            *end = 0;
        }
    }

    while (end > afterRoot) // jeste neni jen root
    {
        DWORD attrs = CRAPI::GetFileAttributes(userPart);
        if (attrs != 0xFFFFFFFF) // tato cast cesty existuje
        {
            if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // je to soubor
            {
                // existujici cesta nema obsahovat jmeno souboru (viz SalSplitGeneralPath), orizneme...
                *end = lastChar;   // opravime 'targetPath'
                pathIsDir = FALSE; // existujici cast cesty je soubor
                while (*--end != '\\')
                    ;            // je jiste, ze aspon za root-cestou je jeden '\\'
                lastChar = *end; // aby se nezrusila cesta
                break;
            }
            else
                break;
        }
        else
        {
            DWORD err = CRAPI::GetLastError();
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_INVALID_NAME &&
                err != ERROR_PATH_NOT_FOUND && err != ERROR_BAD_PATHNAME &&
                err != ERROR_DIRECTORY) // divna chyba - jen vypiseme
            {
                sprintf(buf, LoadStr(IDS_PATH_ERROR), targetPath, SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
                pathError = TRUE;
                break; // ohlasime chybu
            }
        }

        *end = lastChar; // obnova 'targetPath'
        while (*--end != '\\')
            ; // je jiste, ze aspon za root-cestou je jeden '\\'
        lastChar = *end;
        *end = 0;
    }
    *end = lastChar; // opravime 'targetPath'
    SetCursor(oldCur);

    char* opMask = NULL;
    if (!pathError) // rozdeleni probehlo bez chyby
    {
        if (*end == '\\')
            end++;

        char newDirs[MAX_PATH];
        if (SalamanderGeneral->SalSplitGeneralPath(parent, TitleWMobile, TitleWMobileError, sourceFiles + sourceDirs,
                                                   targetPath, afterRoot, end, pathIsDir,
                                                   backslashAtEnd, NULL, NULL, opMask, newDirs,
                                                   NULL /* 'isTheSamePathF' neni potreba*/))
        {
            if (newDirs[0] != 0) // na cilove ceste je potreba vytvorit nejake podadresare
            {
                if (!CRAPI::CheckAndCreateDirectory(userPart, parent, true, NULL, 0, NULL))
                {
                    char* e = targetPath + strlen(targetPath); // oprava 'targetPath' (spojeni 'targetPath' a 'opMask')
                    if (e > targetPath && *(e - 1) != '\\')
                        *e++ = '\\';
                    if (e != opMask)
                        memmove(e, opMask, strlen(opMask) + 1); // je-li potreba, prisuneme masku
                    pathError = TRUE;
                }
            }
        }
        else
            pathError = TRUE;
    }

    if (pathError)
    {
        // 'targetPath' se vraci po uprave ".." a "." + mozna pridane masky
        if (invalidPathOrCancel != NULL)
            *invalidPathOrCancel = TRUE;
        return FALSE; // chyba v ceste - nechame usera opravit
    }

    // popis cile operace ziskaneho predchazejici casti kodu:
    // 'targetPath' je cesta na toto FS ('userPart' ukazuje na user-part FS cesty), 'opMask' je operacni maska

    // priprava bufferu pro jmena
    char sourceName[MAX_PATH]; // buffer pro plne jmeno na disku
    strcpy(sourceName, sourcePath);
    char* endSource = sourceName + strlen(sourceName); // misto pro jmena z enumerace 'next'
    if (endSource > sourceName && *(endSource - 1) != '\\')
    {
        *endSource++ = '\\';
        *endSource = 0;
    }
    int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // max. pocet znaku pro jmeno z enumerace 'next'

    char targetName[MAX_PATH]; // buffer pro plne cilove jmeno na disku (cil operace lezi u CEFS na disku)
    strcpy(targetName, userPart);
    char* endTarget = targetName + strlen(targetName); // misto pro cilove jmeno
    if (endTarget > targetName && *(endTarget - 1) != '\\')
    {
        *endTarget++ = '\\';
        *endTarget = 0;
    }
    int endTargetSize = MAX_PATH - (int)(endTarget - targetName); // max. pocet znaku pro cilove jmeno

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    CFileInfoArray array(10, 10);

    BOOL success = TRUE; // FALSE v pripade chyby nebo preruseni uzivatelem

    BOOL isDir;
    const char* name;
    const char* dosName; // dummy
    CQuadWord size;
    DWORD attr1;
    FILETIME lastWrite;
    int errorOccured;

    while ((name = next(parent, 0, &dosName, &isDir, &size, &attr1, &lastWrite, nextParam, &errorOccured)) != NULL)
    { // provedeme copy/move na souboru/adresari
        success = FindAllFilesInTree(sourcePath, name, array, TRUE, 0);

        // zjistime jestli ma cenu pokracovat (pokud neni cancel)
        if (!success)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    BOOL skipAllErrors = FALSE;              // skip all errors
    BOOL sourcePathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na zdrojove ceste (operace move)
    BOOL subdirsOfSourcePathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich zdrojove cesty
    BOOL targetPathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na cilove ceste
    BOOL subdirsOfTargetPathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich cilove cesty
    BOOL skipAllOverwrite = FALSE;
    BOOL skipAllOverwriteSystemHidden = FALSE;

    // vyzvedneme hodnoty "Confirm on" z konfigurace
    BOOL ConfirmOnFileOverwrite, ConfirmOnSystemHiddenFileOverwrite;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disablujeme 'mainWnd'

    CProgress2Dlg dlg(mainWnd, LoadStr(copy ? IDS_COPY : IDS_MOVE), LoadStr(copy ? IDS_COPYING : IDS_MOVING), LoadStr(IDS_TO), ooStatic); // aby nemodalni dialog mohl byt na stacku, dame 'ooStatic'

    dlg.Create();
    EnableWindow(mainWnd, FALSE);
    SetForegroundWindow(dlg.HWindow);

    INT64 totalsize = 0, copied = 0;
    int i;
    for (i = 0; i < array.Count; i++)
        totalsize += (array[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 100 : array[i].size;

    for (/*int*/ i = 0; i < array.Count; i++)
    {
        CFileInfo& fi = array[i];

        char* targetFile = SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, fi.cFileName, opMask);

        if ((int)strlen(fi.cFileName) >= endSourceSize || (int)strlen(targetFile) >= endTargetSize)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        // sestaveni plneho jmena, orez na MAX_PATH je teoreticky zbytecny, prakticky bohuzel ne
        lstrcpyn(endSource, fi.cFileName, endSourceSize);

        // slozime cilove jmeno - zjednodusene o test chyby LoadStr(IDS_ERR_NAMETOOLONG)
        // ('name' je jen z rootu zdrojove cesty - zadne podadresare - maskou upravime cele 'name')
        lstrcpyn(endTarget, targetFile, endTargetSize);

        isDir = (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (SalamanderGeneral->StrICmp(sourceName, targetName) == 0)
        {
            SalamanderGeneral->SalMessageBox(parent,
                                             LoadStr(copy ? (isDir ? IDS_ERR_COPYDIRTOITSELF : IDS_ERR_COPYFILETOITSELF) : (isDir ? IDS_ERR_MOVEDIRTOITSELF : IDS_ERR_MOVEFILETOITSELF)),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        dlg.Set(sourceName, targetName, FALSE);

        BOOL skip = FALSE;
        if (isDir)
        {
            if (fi.block == -1)
            {
                while (1)
                {
                    if (!CRAPI::CheckAndCreateDirectory(targetName, parent, true, NULL, 0, NULL))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = CRAPI::GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, targetName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                        else
                            skip = TRUE;
                    }
                    else
                    {
                        targetPathChanged = TRUE;
                        subdirsOfTargetPathChanged = TRUE;
                        break; // uspesne vytvoreny adresar
                    }
                } // end of while(1)
            }
        }
        else
        {
            // nakopirujeme soubor primo na CEFS

            DWORD attr = CRAPI::GetFileAttributes(targetName);

            if (attr != 0xFFFFFFFF)
            {
                if (!skipAllOverwrite)
                {
                    if (ConfirmOnFileOverwrite)
                    {
                        char sourceData[100], targetData[100];

                        GetFileData(sourceName, sourceData);
                        CRAPI::GetFileData(targetName, targetData);

                        int res = SalamanderGeneral->DialogOverwrite(parent, BUTTONS_YESALLSKIPCANCEL, targetName, targetData, sourceName, sourceData);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;
                        case DIALOG_SKIPALL:
                            skipAllOverwrite = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;
                        default:
                            success = FALSE;
                            break;
                        }
                    }
                }
                else
                    skip = TRUE;

                if (success && !skip && ConfirmOnSystemHiddenFileOverwrite && (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllOverwriteSystemHidden)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, targetName,
                                                                    LoadStr(IDS_YESNO_OVERWRITEHIDDENFILE), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnSystemHiddenFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;
                        case DIALOG_SKIPALL:
                            skipAllOverwriteSystemHidden = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;
                        default:
                            success = FALSE;
                            break;
                        }
                    }
                    else
                        skip = TRUE;
                }
            }
            else
                attr = 0;

            if (success && !skip)
            {
                while (1)
                {
                    if (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                        CRAPI::SetFileAttributes(targetName, FILE_ATTRIBUTE_ARCHIVE);

                    LPCTSTR errFileName = "";
                    DWORD err = CRAPI::CopyFileToCE(sourceName, targetName, FALSE, &dlg, copied, totalsize, &errFileName);
                    if (err != 0)
                    {
                        if (err == -1) //JR preruseno uzivatelem
                            success = FALSE;
                        else if (!skipAllErrors)
                        {
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, errFileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                        else
                            skip = TRUE;
                    }
                    else
                    {
                        targetPathChanged = TRUE;

                        sprintf(cefsFileName, "%s:%s", fsName, targetName);
                        SalamanderGeneral->ToLowerCase(cefsFileName);
                        SalamanderGeneral->RemoveOneFileFromCache(cefsFileName);

                        break; // uspesne nakopirovano
                    }
                    if (!success || skip)
                        break;
                }
            }
        }

        if (success && !copy && !skip) // jde o "move" a soubor nebyl skipnuty -> smazeme zdrojovy soubor
        {

            // zrusime soubor na disku
            while (1)
            {
                if (isDir)
                {
                    if (fi.block != -1)
                    {
                        SalamanderGeneral->ClearReadOnlyAttr(sourceName, fi.dwFileAttributes);

                        if (!RemoveDirectory(sourceName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, sourceName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                                switch (res)
                                {
                                case DIALOG_RETRY:
                                    break;

                                case DIALOG_SKIPALL:
                                    skipAllErrors = TRUE;
                                case DIALOG_SKIP:
                                    skip = TRUE;
                                    break;

                                default:
                                    success = FALSE;
                                    break; // DIALOG_CANCEL
                                }
                            }
                            else
                                skip = TRUE;
                        }
                        else
                        {
                            sourcePathChanged = TRUE;
                            subdirsOfSourcePathChanged = TRUE;
                            break; // uspesny RemoveDirectory
                        }
                    }
                    else
                        skip = TRUE;
                }
                else
                {
                    SalamanderGeneral->ClearReadOnlyAttr(sourceName, fi.dwFileAttributes);

                    if (!DeleteFile(sourceName))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, sourceName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                        else
                            skip = TRUE;
                    }
                    else
                    {
                        sourcePathChanged = TRUE;
                        break; // uspesny delete
                    }
                }
                if (!success || skip)
                    break;
            }
        }

        if (success)
        {
            copied += isDir ? 100 : fi.size;

            float progress = (totalsize ? ((float)copied / (float)totalsize) : 0) * 1000;
            dlg.SetProgress((DWORD)progress, 0, FALSE);
        }
        else
            break; // zjistime jestli ma cenu pokracovat (pokud neni cancel)
    }

    EnableWindow(mainWnd, TRUE);
    DestroyWindow(dlg.HWindow); // zavreme progress dialog

    // zmena na zdrojove ceste (hlavne operace move), muze byt jen diskova cesta
    if (sourcePathChanged)
        SalamanderGeneral->PostChangeOnPathNotification(sourcePath, subdirsOfSourcePathChanged);

    // zmena na cilove ceste (mela by byt FS - 'targetPath', u CEFS je to ale diskova cesta 'userPart')
    if (targetPathChanged)
    {
        sprintf(cefsFileName, "%s:%s", fsName, userPart);
        SalamanderGeneral->PostChangeOnPathNotification(cefsFileName, subdirsOfTargetPathChanged);
    }

    SalamanderGeneral->RestoreFocusInSourcePanel();

    if (success)
        return TRUE; // operace uspesne dokoncena
    else
    {
        if (invalidPathOrCancel != NULL)
            *invalidPathOrCancel = TRUE;
        return TRUE; // cancel
    }
}

BOOL WINAPI
CPluginFSInterface::ChangeAttributes(const char* fsName, HWND parent, int panel,
                                     int selectedFiles, int selectedDirs)
{
    // priprava bufferu pro jmena
    char fileName[MAX_PATH]; // buffer pro plne jmeno na disku (zdroj operace lezi u DFS na disku)
    strcpy(fileName, Path);
    char* end = fileName + strlen(fileName); // misto pro jmena z panelu
    if (end > fileName && *(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    int endSize = MAX_PATH - (int)(end - fileName); // max. pocet znaku pro jmeno z panelu

    const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0, count = 0;
    BOOL success = TRUE;        // FALSE v pripade chyby nebo preruseni uzivatelem
    BOOL skipAllErrors = FALSE; // skip all errors

    RapiNS::CE_FIND_DATA findData;
    DWORD attr = 0, attrDiff = 0;
    SYSTEMTIME timeModified, timeCreated, timeAccessed;
    BOOL selectedDirectory = FALSE;

    //JR 1. faze - zjistim aktualni atributy
    while (1)
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // provedeme operaci na souboru/adresari
        if (f != NULL)
        {

            if ((int)strlen(f->Name) >= endSize)
            {
                SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                                 TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                success = FALSE;
                break;
            }

            lstrcpyn(end, f->Name, endSize);

            BOOL skip = FALSE;
            while (1)
            {
                if (count == 0)
                {
                    HANDLE handle = CRAPI::FindFirstFile(fileName, &findData);
                    if (handle == INVALID_HANDLE_VALUE)
                    {
                        if (skipAllErrors)
                            skip = TRUE;
                        else
                        {
                            DWORD err = CRAPI::GetLastError();
                            if (err == ERROR_NO_MORE_FILES)
                                err = ERROR_FILE_NOT_FOUND;
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                    }
                    else
                    {
                        CRAPI::FindClose(handle);

                        DWORD attrib = findData.dwFileAttributes;
                        if (attrib & FILE_ATTRIBUTE_DIRECTORY)
                            selectedDirectory = TRUE;

                        attrib &= FILE_ATTRIBUTES_MASK;
                        attr |= attrib;

                        FILETIME time;
                        FileTimeToLocalFileTime(&findData.ftLastWriteTime, &time);
                        FileTimeToSystemTime(&time, &timeModified);
                        FileTimeToLocalFileTime(&findData.ftCreationTime, &time);
                        FileTimeToSystemTime(&time, &timeCreated);
                        FileTimeToLocalFileTime(&findData.ftLastAccessTime, &time);
                        FileTimeToSystemTime(&time, &timeAccessed);

                        count++;
                        break;
                    }
                }
                else
                {
                    DWORD attrib = CRAPI::GetFileAttributes(fileName);
                    if (attr == 0xFFFFFFFF)
                    {
                        if (skipAllErrors)
                            skip = TRUE;
                        else
                        {
                            DWORD err = CRAPI::GetLastError();
                            //if (err == ERROR_NO_MORE_FILES) err = ERROR_FILE_NOT_FOUND;
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                    }
                    else
                    {
                        if (attrib & FILE_ATTRIBUTE_DIRECTORY)
                            selectedDirectory = TRUE;

                        attrib &= FILE_ATTRIBUTES_MASK;
                        attrDiff |= (attr ^ attrib);
                        attr |= attrib;

                        count++;
                        break;
                    }
                }
                if (!success || skip)
                    break;
            }
        }

        // zjistime jestli ma cenu pokracovat (pokud neni cancel a existuje jeste nejaka dalsi oznacena polozka)
        if (!success || focused || f == NULL)
            break;
    }

    char path[2 * MAX_PATH];
    if (!success || count == 0)
    {
        //JR pravdepodobne jiz doslo k vymazani souboru / adresare
        if (count == 0)
        {
            sprintf(path, "%s:%s", fsName, Path);
            SalamanderGeneral->PostChangeOnPathNotification(path, FALSE);
        }
        return FALSE;
    }

    if (count > 1)
    {
        GetSystemTime(&timeModified);
        timeCreated = timeModified;
        timeAccessed = timeModified;
    }

    CChangeAttrDialog dlgAttr(parent, ooStatic, attr, attrDiff, selectedDirectory, &timeModified, &timeCreated, &timeAccessed);
    if (dlgAttr.Execute() != IDOK)
        return FALSE;

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());

    CFileInfoArray array(count, 10);

    //JR 2.fáze - nactu vsechny soubory, u kterych budeme menit atributy

    index = 0;
    for (;;)
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // smazeme soubor/adresar
        //JR FindAllFilesInTree volam i pro jednotlive soubory.
        //JR Zjisti se tim, jestli stale jeste existuji
        if (f != NULL)
        {

            if (dlgAttr.RecurseSubDirs)
            {
                *end = 0; //JR => ve fileName je rootPath
                success = CRAPI::FindAllFilesInTree(fileName, f->Name, array, 0, FALSE);
            }
            else
            {
                CFileInfo fi;
                strcpy(fi.cFileName, f->Name);
                fi.dwFileAttributes = f->Attr;

                array.Add(fi);
                if (array.State != etNone)
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                    TRACE_E("Low memory");
                    success = false;
                }
            }
        }

        // zjistime jestli ma cenu pokracovat (pokud neni chyba a existuje jeste nejaka dalsi oznacena polozka)
        if (!success || focused || f == NULL)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    if (!success || array.Count == 0)
    {
        //JR pravdepodobne jiz doslo k vymazani souboru / adresare
        if (array.Count == 0)
        {
            sprintf(path, "%s:%s", fsName, Path);
            SalamanderGeneral->PostChangeOnPathNotification(path, FALSE);
        }
        return FALSE;
    }

    BOOL pathChanged = FALSE;                      // TRUE, pokud doslo ke zmenam na ceste
    BOOL changeInSubdirs = dlgAttr.RecurseSubDirs; // TRUE, pokud doslo i ke zmenam v podadresarich cesty
    skipAllErrors = FALSE;                         // skip all errors

    //JR 3.fáze - nastavim atributy

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disablujeme 'mainWnd'

    BOOL showProgressDialog = array.Count > 1;
    BOOL enableMainWnd = TRUE;
    CProgressDlg delDlg(mainWnd, LoadStr(IDS_ATTRIBUTES), LoadStr(IDS_CHANGING), ooStatic); // aby nemodalni dialog mohl byt na stacku, dame 'ooStatic'

    if (showProgressDialog)
    {
        EnableWindow(mainWnd, FALSE);
        delDlg.Create();
    }

    if (!showProgressDialog || delDlg.HWindow != NULL) // dialog se podarilo otevrit
    {
        if (showProgressDialog)
            SetForegroundWindow(delDlg.HWindow);

        int block = 0;
        int i;
        for (i = 0; success && i < array.Count; i++)
        {
            CFileInfo& fi = array[i];

            *end = 0;
            if (!CRAPI::PathAppend(fileName, fi.cFileName, MAX_PATH))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_PATHTOOLONG),
                                                  TitleWMobileError, MSGBOX_ERROR);
                success = FALSE;
                break;
            }

            if (showProgressDialog)
            {
                float progress = ((float)i / (float)array.Count);
                delDlg.Set(fileName, (DWORD)(progress * 1000), TRUE); // delayedPaint == TRUE, abychom nebrzdili
            }

            if (showProgressDialog && delDlg.GetWantCancel())
            {
                success = FALSE;
                break;
            }

            BOOL skip = FALSE;
            while (1)
            {
                DWORD attr2 = fi.dwFileAttributes;

                DWORD err = 0;
                if (dlgAttr.Archive == 0)
                    attr2 &= ~FILE_ATTRIBUTE_ARCHIVE;
                if (dlgAttr.Archive == 1)
                    attr2 |= FILE_ATTRIBUTE_ARCHIVE;
                if (dlgAttr.ReadOnly == 0)
                    attr2 &= ~FILE_ATTRIBUTE_READONLY;
                if (dlgAttr.ReadOnly == 1)
                    attr2 |= FILE_ATTRIBUTE_READONLY;
                if (dlgAttr.System == 0)
                    attr2 &= ~FILE_ATTRIBUTE_SYSTEM;
                if (dlgAttr.System == 1)
                    attr2 |= FILE_ATTRIBUTE_SYSTEM;
                if (dlgAttr.Hidden == 0)
                    attr2 &= ~FILE_ATTRIBUTE_HIDDEN;
                if (dlgAttr.Hidden == 1)
                    attr2 |= FILE_ATTRIBUTE_HIDDEN;

                if (fi.dwFileAttributes != attr2)
                {
                    if (!CRAPI::SetFileAttributes(fileName, attr2))
                        err = CRAPI::GetLastError();
                }

                if (err == 0 && (attr2 & FILE_ATTRIBUTE_DIRECTORY) == 0) //JR Zda se, ze u adresaru nelze menit casy
                {
                    err = CRAPI::SetFileTime(fileName,
                                             dlgAttr.ChangeTimeCreated ? &dlgAttr.TimeCreated : NULL,
                                             dlgAttr.ChangeTimeAccessed ? &dlgAttr.TimeAccessed : NULL,
                                             dlgAttr.ChangeTimeModified ? &dlgAttr.TimeModified : NULL);
                }

                if (err == 0)
                {
                    pathChanged = TRUE;
                    break; //JR povedlo se, jdeme dal
                }
                else
                {
                    if (!skipAllErrors)
                    {
                        int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                 SalamanderGeneral->GetErrorText(err), TitleWMobileError);
                        switch (res)
                        {
                        case DIALOG_RETRY:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllErrors = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break; // DIALOG_CANCEL
                        }
                    }
                    else
                        skip = TRUE;
                }

                if (!success || skip)
                    break;
            }
        }

        if (showProgressDialog)
        {
            // enablujeme 'mainWnd' (jinak ho nemuze Windows vybrat jako foreground/aktivni okno)
            EnableWindow(mainWnd, TRUE);
            enableMainWnd = FALSE;

            DestroyWindow(delDlg.HWindow); // zavreme progress dialog
        }
    }

    // enablujeme 'mainWnd' (nenastala zmena foreground okna - progress se vubec neotevrel)
    if (showProgressDialog && enableMainWnd)
        EnableWindow(mainWnd, TRUE);

    SalamanderGeneral->RestoreFocusInSourcePanel();

    // zmena na zdrojove ceste Path
    if (pathChanged)
    {
        sprintf(path, "%s:%s", fsName, Path);
        SalamanderGeneral->PostChangeOnPathNotification(path, changeInSubdirs);
    }

    return success;
}

void WINAPI
CPluginFSInterface::ShowProperties(const char* fsName, HWND parent, int panel,
                                   int selectedFiles, int selectedDirs)
{
}

void WINAPI
CPluginFSInterface::ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int menutype,
                                int panel, int selectedFiles, int selectedDirs)
{

    HMENU menu = CreatePopupMenu();
    if (menu == NULL)
    {
        TRACE_E("CPluginFSInterface::ContextMenu: Nelze vytvorit menu.");
        return;
    }
    MENUITEMINFO mi;
    char nameBuf[200];

    switch (menutype)
    {
    case fscmPathInPanel:  // kontextove menu pro aktualni cestu v panelu
    case fscmPanel:        // kontextove menu pro panel
    case fscmItemsInPanel: // kontextove menu pro polozky v panelu (oznacene/fokusle soubory a adresare)
    {
        // vlozeni prikazu Salamandera
        int i = 0;
        int index = 0;
        int salCmd;
        BOOL enabled;
        int type, lastType = sctyUnknown;
        while (SalamanderGeneral->EnumSalamanderCommands(&index, &salCmd, nameBuf, 200, &enabled, &type))
        {
            if ((menutype == fscmItemsInPanel && type != sctyForCurrentPath && type != sctyForConnectedDrivesAndFS ||
                 menutype == fscmPanel && (type == sctyForCurrentPath || type == sctyForConnectedDrivesAndFS)) &&
                salCmd != SALCMD_CHANGECASE &&
                salCmd != SALCMD_EMAIL && salCmd != SALCMD_EDITNEWFILE)
            {
                if (type != lastType && lastType != sctyUnknown) // vlozeni separatoru
                {
                    memset(&mi, 0, sizeof(mi));
                    mi.cbSize = sizeof(mi);
                    mi.fMask = MIIM_TYPE;
                    mi.fType = MFT_SEPARATOR;
                    InsertMenuItem(menu, i++, TRUE, &mi);
                }
                lastType = type;

                // vlozeni prikazu Salamandera
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
                mi.fType = MFT_STRING;
                mi.wID = salCmd + 1000;
                mi.dwTypeData = nameBuf;
                mi.cch = (UINT)strlen(nameBuf);
                mi.fState = enabled ? MFS_ENABLED : MFS_DISABLED;
                InsertMenuItem(menu, i++, TRUE, &mi);
            }
        }
        if (i > 0)
        {
            DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                         menuX, menuY, parent, NULL);
            if (cmd >= 1000)
                SalamanderGeneral->PostSalamanderCommand(cmd - 1000);
        }
    }
    break;
    }
}

void CPluginFSInterface::EmptyCache()
{
    // sestavime unikatni jmeno rootu tohoto FS v disk-cache (zasahneme vsechny kopie souboru z tohoto FS)
    char uniqueFileName[2 * MAX_PATH];
    strcpy(uniqueFileName, AssignedFSName);
    strcat(uniqueFileName, ":\\");
    // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
    // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
    SalamanderGeneral->ToLowerCase(uniqueFileName);
    SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);
}
