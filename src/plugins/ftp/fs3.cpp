// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CPluginFSInterface
//

BOOL CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    detach = FALSE;
    BOOL ret = TRUE;
    BOOL close = TRUE;
    if (!CalledFromDisconnectDialog &&                // pri volani z Disconnect dialogu se nema cenu na nic ptat
        !forceClose &&                                // pri force se nema cenu na nic ptat
        reason != FSTRYCLOSE_UNLOADCLOSEFS &&         // pri unloadu pluginu se nema cenu ptat (FS v panelu je v klidu, nic netaha, atd.)
        reason != FSTRYCLOSE_UNLOADCLOSEDETACHEDFS && // pri unloadu pluginu se nema cenu ptat (odpojene FS je v klidu, nic netaha, atd.)
        reason != FSTRYCLOSE_PLUGINCLOSEDETACHEDFS)   // disconnect (z kontextovyho menu v Alt+F1/F2) odpojeneho FS, nema smysl se na nic ptat

    {
        if (reason == FSTRYCLOSE_CHANGEPATH)
        {
            if (canDetach && !Config.DisconnectCommandUsed) // "close or detach" a nejde o prikaz "Disconnect"
            {
                int res;
                if (!Config.AlwaysNotCloseCon)
                {
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = SalamanderGeneral->GetMsgBoxParent();
                    params.Flags = MSGBOXEX_YESNOCANCEL | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                    params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                    params.Text = LoadStr(IDS_CLOSECONINPANEL);
                    params.CheckBoxText = LoadStr(IDS_ALWAYSNOTCLOSE);
                    params.CheckBoxValue = &Config.AlwaysNotCloseCon;
                    char buffer[100];
                    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_DISCONNECTBUTTON
  {MNTT_IT, IDS_KEEPCONBUTTON
  {MNTT_PE, 0
};
*/
                    sprintf(buffer, "%d\t%s\t%d\t%s", DIALOG_YES, LoadStr(IDS_DISCONNECTBUTTON),
                            DIALOG_NO, LoadStr(IDS_KEEPCONBUTTON));
                    params.AliasBtnNames = buffer;
                    res = SalamanderGeneral->SalMessageBoxEx(&params);
                    // nechame prekreslit hlavni okno (aby user cely disconnect nekoukal na zbytek dialogu)
                    UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
                }
                else
                    res = IDNO;

                detach = (res == IDNO);
                ret = (res != IDCANCEL);
                close = (res == IDYES);
            }
            else
            {
                int res = IDYES;
                if (Config.DisconnectCommandUsed && !Config.AlwaysDisconnect && !IsDetached)
                {
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = SalamanderGeneral->GetMsgBoxParent();
                    params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED |
                                   MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                    params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                    params.Text = LoadStr(IDS_WANTDISCONNECT);
                    params.CheckBoxText = LoadStr(IDS_ALWAYSCLOSE);
                    params.CheckBoxValue = &Config.AlwaysDisconnect;
                    res = SalamanderGeneral->SalMessageBoxEx(&params);
                    // nechame prekreslit hlavni okno (aby user cely disconnect nekoukal na zbytek dialogu)
                    UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
                }
                close = ret = (res == IDYES);
            }
        }
        else
        {
            if (reason == FSTRYCLOSE_CHANGEPATHFAILURE || reason == FSTRYCLOSE_ATTACHFAILURE)
            {
                close = ret = TRUE; // tenhle dotaz se ukazal jako silne matouci a nepochopitelny, proto proste jen preferujeme Disconnect
                                    /*
        // "zadna cesta na FTP neni pristupna, disconnect?"
        close = ret = (SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                                        LoadStr(IDS_NOPATHACCESSINPANEL),
                                                        LoadStr(IDS_FTPPLUGINTITLE),
                                                        MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                        MSGBOXEX_ICONQUESTION) == IDYES);
        // nechame prekreslit hlavni okno (aby user cely disconnect nekoukal na zbytek dialogu)
        UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
*/
            }
        }
    }

    if (close && ControlConnection != NULL && ControlConnection->IsConnected())
    { // zde zavirame connectionu z duvodu, aby v pripade noveho pripojeni na stejny server
        // s limitem na jedno pripojeni byla sance na uspech (s timto pripojenim uz by slo
        // o dve na jeden server -> server by odmitl kvuli svemu limitu)
        ControlConnection->CloseControlConnection(SalamanderGeneral->GetMsgBoxParent());
    }
    return ret;
}

void CPluginFSInterface::Event(int event, DWORD param)
{
    switch (event)
    {
    case FSE_DETACHED:
    {
        if (ControlConnection != NULL)
            ControlConnection->LogMessage(LoadStr(IDS_LOGMSGDETACHED), -1, TRUE);
        Logs.RefreshListOfLogsInLogsDlg();
        IsDetached = TRUE;
        break;
    }

    case FSE_ACTIVATEREFRESH:
    {
        if (RefreshPanelOnActivation)
        {
            RefreshPanelOnActivation = FALSE;
            SalamanderGeneral->PostRefreshPanelFS(this); // provedeme refresh pri aktivaci hl. okna Salamandera
        }

        CPluginFSInterface* fs = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs == this)
        {
            ActivateWelcomeMsg();
            if (Config.AlwaysShowLogForActPan)
                Logs.ActivateLog(GetLogUID()); // po aktivaci logu connectiony workera z dialogu operace je nutne po prepnuti do panelu znovu aktivovat log panelu
        }
        break;
    }

    case FSE_ATTACHED:
        if (ControlConnection != NULL)
            ControlConnection->LogMessage(LoadStr(IDS_LOGMSGATTACHED), -1, TRUE);
        // break;  // nema tu byt
    case FSE_OPENED:
    {
        RefreshValuesOfPanelCtrlCon();
        Logs.RefreshListOfLogsInLogsDlg();
        if (IsDetached && Config.AlwaysShowLogForActPan)
            Logs.ActivateLog(GetLogUID());
        IsDetached = FALSE;
        break;
    }

    case FSE_CLOSEORDETACHCANCELED:
    {
        if (Config.AlwaysShowLogForActPan)
        {
            CPluginFSInterface* fs = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
            if (fs != NULL)
                Logs.ActivateLog(fs->GetLogUID());
        }
        break;
    }

    case FSE_TIMER: // zpozdeny refresh panelu (aby se stihla vratit connectiona z operacniho dialogu do panelu)
    {
        DWORD paramLimit = 4; // max. jedna vterina cekani na connectionu (cekame, jestli se bude worker snazit predat connectionu)
        DWORD ti;
        if (ControlConnection != NULL &&
            ControlConnection->GetIsSocketConnectedLastCallTime(&ti) && GetTickCount() - ti < 5000)
        {
            paramLimit = 24; // max. 5 vterin cekani na connectionu (vime, ze se nam ji worker snazi predat)
        }
        if (param >= paramLimit || // param < paramLimit: spusti refresh jen pokud mame connectionu, param >= paramLimit pusti refresh vzdy
            ControlConnection != NULL && ControlConnection->IsConnected())
        {
            // potrebujeme refresh i v neaktivnim Salamanderovi (lidi nadavali, ze pri aktivaci Salama naskoci
            // tahani listingu a ze to melo byt davno hotove) + zpozdit refresh az po predani control connectiony
            // z operace zpet do panelu => postneme prikaz menu a v nem provedeme tvrdy refresh
            CPluginFSInterface* fsLeft = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(PANEL_LEFT);
            CPluginFSInterface* fsRight = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(PANEL_RIGHT);
            if (fsLeft == this || fsRight == this)
                SalamanderGeneral->PostMenuExtCommand(fsLeft == this ? FTPCMD_REFRESHLEFTPANEL : FTPCMD_REFRESHRIGHTPANEL, TRUE); // pockame na "sal-idle"
        }
        else
            SalamanderGeneral->AddPluginFSTimer(200, this, param + 1);
        break;
    }

    case FSE_PATHCHANGED:
    {
        LPCTSTR pToolTip = NULL;
        BOOL bLocked = FALSE, bShow = FALSE;

        if (ControlConnection)
        {
            CCertificate* cert = ControlConnection->GetCertificate();
            if (cert != NULL)
            {
                bShow = TRUE;
                bLocked = cert->IsVerified();
                pToolTip = LoadStr(bLocked ? IDS_SSL_SECURITY_OK : IDS_SSL_SECURITY_UNVERIFIED);
                cert->Release();
            }
        }
        SalamanderGeneral->ShowSecurityIcon(param, bShow, bLocked, pToolTip);
        break;
    }
    }
}

void CPluginFSInterface::ReleaseObject(HWND parent)
{
    if (ControlConnection != NULL)
    {
        if (ControlConnection->IsConnected())
        {
            ControlConnection->CloseControlConnection(SalamanderGeneral->GetMsgBoxParent());
        }
        else
        {
            // pokud jsme jeste nestihli vypsat co vedlo k zavreni connectiny, udelame to ted
            // aspon do logu (okno uz nema smysl ukazovat, uzivatele uz to nejspis nezajima)
            ControlConnection->CheckCtrlConClose(TRUE, FALSE, parent, TRUE);
        }

        // je-li zavirany FS v panelu, bude potreba vynulovat LeftPanelCtrlCon nebo RightPanelCtrlCon
        HANDLES(EnterCriticalSection(&PanelCtrlConSect));
        if (LeftPanelCtrlCon == ControlConnection)
            LeftPanelCtrlCon = NULL;
        if (RightPanelCtrlCon == ControlConnection)
            RightPanelCtrlCon = NULL;
        HANDLES(LeaveCriticalSection(&PanelCtrlConSect));

        DeleteSocket(ControlConnection);
        ControlConnection = NULL;
        Logs.RefreshListOfLogsInLogsDlg();
    }
}

DWORD
CPluginFSInterface::GetSupportedServices()
{
    return FS_SERVICE_CONTEXTMENU |
           //FS_SERVICE_SHOWPROPERTIES |
           FS_SERVICE_CHANGEATTRS |
           FS_SERVICE_COPYFROMDISKTOFS |
           FS_SERVICE_MOVEFROMDISKTOFS |
           FS_SERVICE_MOVEFROMFS |
           FS_SERVICE_COPYFROMFS |
           FS_SERVICE_DELETE |
           FS_SERVICE_VIEWFILE |
           FS_SERVICE_CREATEDIR |
           FS_SERVICE_ACCEPTSCHANGENOTIF |
           FS_SERVICE_QUICKRENAME |
           //FS_SERVICE_COMMANDLINE |
           //FS_SERVICE_SHOWINFO |
           //FS_SERVICE_GETFREESPACE |
           FS_SERVICE_GETFSICON |
           FS_SERVICE_GETNEXTDIRLINEHOTPATH |
           FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM |
           FS_SERVICE_GETPATHFORMAINWNDTITLE |
           FS_SERVICE_SHOWSECURITYINFO;
}

BOOL CPluginFSInterface::GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon)
{
    char txt[FTP_USERPART_SIZE + 50]; // userpart + misto na fs-name + misto na zdvojeni par ampersandu
    // text bude cesta na FS (v Salamanderovskem formatu)
    txt[0] = '\t';
    sprintf(txt + 1, "%s:", fsName);
    static char emptyBuff[] = "";
    MakeUserPart(txt + strlen(txt), FTP_USERPART_SIZE, emptyBuff);
    int l = (int)strlen(txt);
    if (l > 0 && txt[l - 1] == '/')
        txt[l - 1] = 0; // orizneme '/' z konce cesty (musi byt "relativni")
    // zdvojime pripadne '&', aby se tisk cesty provedl korektne
    SalamanderGeneral->DuplicateAmpersands(txt, FTP_USERPART_SIZE + 50);
    title = SalamanderGeneral->DupStr(txt);
    if (title == NULL)
        return FALSE; // low-memory, zadna polozka nebude

    icon = FTPIcon;
    destroyIcon = FALSE;
    return TRUE;
}

HICON
CPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
{
    destroyIcon = FALSE;
    return FTPIcon;
}

BOOL CPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    if (DirLineHotPathType == ftpsptEmpty && ControlConnection != NULL)
    { // na zaklade aktualni cesty ve FS zjistime typ cesty v Directory Line
        DirLineHotPathType = ControlConnection->GetFTPServerPathType(Path);
        DirLineHotPathUserLength = FTPGetUserLength(User);
    }

    const char* end = text + pathLen;
    const char* root = text; // ukazatel za root cestu
    while (*root != 0 && *root != ':')
        root++; // preskoceni fs-name
    if (*root == ':')
    {
        root = FTPFindPath(root + 1, DirLineHotPathUserLength);
        if (*root == '/' || *root == '\\')
            root++;                          // preskok prvniho '/' nebo '\\' z cesty
        if (DirLineHotPathType == ftpsptOS2) // u OS/2 cest preskocime jeste root disku ("C:/")
        {
            while (root < end && *root != '/' && *root != '\\')
                root++;
            if (*root == '/' || *root == '\\')
                root++; // preskok '/' nebo '\\' z rootu diskove cesty
        }
        if (DirLineHotPathType == ftpsptTandem) // u Tandem cest preskocime "system" ("\\SYSTEM"), kratsi cesta nema smysl
            while (root < end && *root != '.')
                root++;
    }
    const char* s = text + offset;
    if (s >= end)
    {
        DirLineHotPathType = ftpsptEmpty; // konec zjistovani hot-textu
        DirLineHotPathUserLength = 0;
        return FALSE;
    }
    if (s < root)
        offset = (int)(root - text);
    else
    {
        switch (DirLineHotPathType)
        {
        case ftpsptOpenVMS: // "PUB$DEVICE:[PUB.VMS.DIR2]"
        {                   // oddelovac je tecka, "^." je escape-sekvence pro '.'
            if (*s == '.')
                s++;
            BOOL vmsEsc = FALSE;
            while (s < end)
            {
                if (*s == '^')
                    vmsEsc = !vmsEsc;
                else
                {
                    if (vmsEsc)
                        vmsEsc = FALSE;
                    else
                    {
                        if (*s == '.')
                            break;
                    }
                }
                s++;
            }
            if (*s == '.' && *(s + 1) == ']')
                s = end;
            break;
        }

        case ftpsptIBMz_VM: // "ACADEM:ANONYMOU.PICS" (+root musi koncit teckou: "ACADEM:ANONYMOU.")
        case ftpsptMVS:     // "'VEA0016.MAIN.CLIST'"
        case ftpsptTandem:  // \SYSTEM.$VVVVV.SUBVOLUM.FILENAME
        {                   // oddelovac je tecka
            if (*s == '.')
                s++;
            while (s < end && *s != '.')
                s++;
            if (*s == '.')
            {
                if (DirLineHotPathType == ftpsptMVS && *(s + 1) == '\'')
                    s = end;
                else
                {
                    if (DirLineHotPathType == ftpsptIBMz_VM)
                    {
                        const char* z_VMRootDir = root; // pozice prvni tecky (root) u IBM_z/VM cest
                        while (z_VMRootDir < end && *z_VMRootDir != '.')
                            z_VMRootDir++;
                        if (s == z_VMRootDir)
                            s++; // root musi koncit teckou, ostatni cesty nejspis ne
                    }
                }
            }
            break;
        }

        case ftpsptNetware: // "/pub/altap/salamand" nebo "\pub\altap\salamand"
        case ftpsptWindows: // "/pub/altap/salamand" nebo "\pub\altap\salamand"
        case ftpsptOS2:     // napr. C:/DIR1/DIR2 nebo C:\DIR1\DIR2
        {                   // oddelovace jsou '/' a '\\'
            if (*s == '/' || *s == '\\')
                s++;
            while (s < end && *s != '/' && *s != '\\')
                s++;
            break;
        }

        default: // unix a dalsi: "/pub/altap/salamand" (ale i "/\dir-with-backslash")
        {        // oddelovac je '/'
            if (*s == '/')
                s++;
            while (s < end && *s != '/')
                s++;
            break;
        }
        }
        offset = (int)(s - text);
    }
    if (offset == pathLen)
    {
        DirLineHotPathType = ftpsptEmpty; // konec zjistovani hot-textu
        DirLineHotPathUserLength = 0;
    }
    return TRUE;
}

void CPluginFSInterface::CompleteDirectoryLineHotPath(char* path, int pathBufSize)
{
    if (ControlConnection != NULL)
    { // na zaklade aktualni cesty ve FS zjistime typ cesty v Directory Line
        CFTPServerPathType type = ControlConnection->GetFTPServerPathType(Path);
        if (type == ftpsptOpenVMS || type == ftpsptMVS) // pridat "zavorku" potrebuji jen OpenVMS a MVS
        {
            // ziskame ukazatel za root cestu
            const char* root = path;
            while (*root != 0 && *root != ':')
                root++; // preskoceni fs-name
            if (*root == ':')
            {
                root = FTPFindPath(root + 1, FTPGetUserLength(User));
                if (*root == '/' || *root == '\\')
                    root++; // preskok prvniho '/' nebo '\\' z cesty
            }
            int pathLen = (int)strlen(path);

            // zkontrolujeme "zavorku", pripadne pokud chybi, tak ji pridame
            if (type == ftpsptOpenVMS) // OpenVMS - zavorka je ']'
            {
                const char* s = root;
                BOOL vmsEsc = FALSE;
                while (*s != 0)
                {
                    if (*s == '^')
                        vmsEsc = !vmsEsc;
                    else
                    {
                        if (vmsEsc)
                            vmsEsc = FALSE;
                        else
                        {
                            if (*s == '[')
                                break;
                        }
                    }
                    s++;
                }
                if (*s == '[')
                {
                    if (path[pathLen - 1] != ']' || FTPIsVMSEscapeSequence(path, path + (pathLen - 1)))
                    {
                        if (pathLen + 1 < pathBufSize)
                        {
                            path[pathLen++] = ']';
                            path[pathLen] = 0;
                        }
                    }
                }
            }
            else // MVS - zavorka je '\''
            {
                if (strchr(root, '\'') != NULL && path[pathLen - 1] != '\'' && pathLen + 1 < pathBufSize)
                {
                    path[pathLen++] = '\'';
                    path[pathLen] = 0;
                }
            }
        }
    }
}

BOOL CPluginFSInterface::GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize)
{
    CFTPServerPathType pathType = ControlConnection != NULL ? ControlConnection->GetFTPServerPathType(Path) : ftpsptUnknown;
    BOOL needFullPath = FALSE;
    char root[2 * MAX_PATH];
    char path[FTP_MAX_PATH];
    if (mode == 1) // rezim "Directory Name Only"
    {
        lstrcpyn(path, Path, FTP_MAX_PATH);
        if (pathType == ftpsptUnknown)
            pathType = ftpsptUnix;
        if (FTPCutDirectory(pathType, path, FTP_MAX_PATH, buf, bufSize, NULL)) // oriznuti se povedlo
        {
            return TRUE;
        }
        else // je to root nebo nastala nejaka jina chyba (neznamy typ cesty)
        {
            needFullPath = TRUE;
        }
    }
    else
    {
        if (mode == 2) // rezim "Shortened Path"
        {
            char* trimStart = NULL;
            char* trimEnd = NULL;
            lstrcpyn(path, Path, FTP_MAX_PATH);
            switch (pathType)
            {
            case ftpsptIBMz_VM: // "ACADEM:ANONYMOU.PICS" (+root musi koncit teckou: "ACADEM:ANONYMOU.")
            {                   // oddelovac je tecka
                trimStart = strchr(path, '.');
                char* s = path + strlen(path) - 1; // posledni znak ignorujeme (je-li tecka stejne preskocime)
                while (--s > path && *s != '.')
                    ;
                if (s > path)
                    trimEnd = s + 1;
                if (trimStart + 1 == trimEnd)
                    trimEnd = NULL; // jen jedna tecka v ceste -> nedelame trim
                break;
            }

            case ftpsptOpenVMS: // "PUB$DEVICE:[PUB.VMS.DIR2]"
            {                   // oddelovac je tecka, "^." je escape-sekvence pro '.'
                char* s = path;
                BOOL vmsEsc = FALSE;
                while (*s != 0)
                {
                    if (*s == '^')
                        vmsEsc = !vmsEsc;
                    else
                    {
                        if (vmsEsc)
                            vmsEsc = FALSE;
                        else
                        {
                            if (*s == '[')
                                break;
                        }
                    }
                    s++;
                }
                if (*s == '[')
                    trimStart = s + 1;

                s = path + strlen(path) - 1;
                if (s > path && *(s - 1) == '.')
                    s--; // posledni znak musi byt ']', ten netestujeme a posledni '.' preskocime
                while (--s > path && (*s != '.' || FTPIsVMSEscapeSequence(path, s)))
                    ; // neescapovana '.'
                if (s > path)
                    trimEnd = s + 1;
                if (trimStart + 1 == trimEnd)
                    trimEnd = NULL; // napr. "[.pub]" -> nedelame trim
                break;
            }

            case ftpsptMVS: // "'VEA0016.MAIN.CLIST'"
            {               // oddelovac je tecka
                trimStart = strchr(path, '\'');
                if (trimStart != NULL)
                    trimStart++;
                char* s = path + strlen(path) - 1;
                if (s > path && *(s - 1) == '.')
                    s--; // posledni znak musi byt '\'', ten netestujeme a posledni '.' preskocime
                while (--s > path && *s != '.')
                    ;
                if (s > path)
                    trimEnd = s + 1;
                if (trimStart + 1 == trimEnd)
                    trimEnd = NULL; // napr. "'.pub'" -> nedelame trim
                break;
            }

            case ftpsptNetware: // "/pub/altap/salamand" nebo "\pub\altap\salamand"
            case ftpsptWindows: // "/pub/altap/salamand" nebo "\pub\altap\salamand"
            {                   // oddelovace jsou '/' a '\\'
                if (path[0] == '/' || path[0] == '\\')
                    trimStart = path + 1;
                else
                    trimStart = path;
                char* s = path + strlen(path) - 1; // '/' ani '\\' na konci retezce nebereme
                while (--s > path && *s != '/' && *s != '\\')
                    ;
                if (s > path)
                    trimEnd = s;
                break;
            }

            case ftpsptOS2: // napr. C:/DIR1/DIR2 nebo C:\DIR1\DIR2
            {
                if (path[0] != 0 && path[1] == ':')
                {
                    trimStart = path + 2;
                    if (*trimStart == '/' || *trimStart == '\\')
                        trimStart++;
                }
                else
                    trimStart = path;
                char* s = path + strlen(path) - 1; // '/' ani '\\' na konci retezce nebereme
                while (--s > path && *s != '/' && *s != '\\')
                    ;
                if (s > path)
                    trimEnd = s;
                break;
            }

            case ftpsptTandem: // \SYSTEM.$VVVVV.SUBVOLUM.FILENAME
            {
                if (path[0] == '/' || path[0] == '\\')
                    trimStart = path + 1;
                else
                    trimStart = path;
                char* s = path + strlen(path) - 1; // '.' na konci retezce nebereme
                while (--s > path && *s != '.')
                    ;
                if (s > path)
                    trimEnd = s + 1;
                break;
            }

            default: // unix a dalsi: "/pub/altap/salamand" (ale i "/\dir-with-backslash")
            {        // oddelovac je '/'
                if (path[0] == '/')
                    trimStart = path + 1;
                else
                    trimStart = path;
                char* s = path + strlen(path) - 1; // '/' na konci retezce nebereme
                while (--s > path && *s != '/')
                    ;
                if (s > path)
                    trimEnd = s;
                break;
            }
            }
            // orezeme cestu
            if (trimStart != NULL && trimEnd != NULL && trimEnd > trimStart)
            {
                memmove(trimStart + 3, trimEnd, strlen(trimEnd) + 1);
                memcpy(trimStart, "...", 3);
                sprintf(root, "%s:", fsName);
                int len = (int)strlen(root);
                if (MakeUserPart(root + len, 2 * MAX_PATH - len, path))
                {
                    lstrcpyn(buf, root, bufSize);
                    return TRUE;
                }
            }
            else
                needFullPath = TRUE;
        }
    }
    if (needFullPath)
    {
        sprintf(root, "%s:", fsName);
        int len = (int)strlen(root);
        if (MakeUserPart(root + len, 2 * MAX_PATH - len, Path))
        {
            lstrcpyn(buf, root, bufSize);
            return TRUE;
        }
    }
    return FALSE; // cesta staci vytvorit standardnim algoritmem
}

void CPluginFSInterface::AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs)
{
    // POZOR: v 'includingSubdirs' pro FTP cesty je naORovany 0x02 pokud jde o "soft refresh"

    BOOL isFTP = SalamanderGeneral->StrNICmp(path, AssignedFSName, AssignedFSNameLen) == 0 &&          // jde o nase fs-name pro FTP
                 path[AssignedFSNameLen] == ':';                                                       // nase fs-name neni jen prefix
    BOOL isFTPS = SalamanderGeneral->StrNICmp(path, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 && // jde o nase fs-name pro FTPS
                  path[AssignedFSNameLenFTPS] == ':';                                                  // nase fs-name neni jen prefix

    if (isFTP || isFTPS)
    {
        // otestujeme shodnost user-part cest nebo aspon jejich prefixu
        char path1[FTP_USERPART_SIZE];
        char path2[FTP_USERPART_SIZE];
        lstrcpyn(path1, path + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS) + 1, FTP_USERPART_SIZE);
        MakeUserPart(path2, FTP_USERPART_SIZE);
        int userLength = FTPGetUserLength(User);
        if (FTPHasTheSameRootPath(path1, path2, userLength)) // stejny root (pripojeni)
        {
            char* p1 = (char*)FTPFindPath(path1, userLength);
            char* p2 = (char*)FTPFindPath(path2, userLength);
            CFTPServerPathType type = GetFTPServerPathType(Path);
            if (FTPIsPrefixOfServerPath(type, p1, p2, !(includingSubdirs & 0x01))) // shoda prefixu nebo jen presna shoda
            {
                NextRefreshWontClearCache = TRUE;
                if (includingSubdirs & 0x02 /* soft refresh */) // provedeme refresh az se aktivuje panel
                {
                    if (GetForegroundWindow() != SalamanderGeneral->GetMainWindowHWND()) // neaktivni hl. okno Salamandera
                        RefreshPanelOnActivation = TRUE;                                 // refresh se provede az pri prijmu FSE_ACTIVATEREFRESH
                    else
                        SalamanderGeneral->PostRefreshPanelFS(this); // aktivni hl. okno Salamandera: pokud je FS v panelu, provedeme jeho refresh
                }
                else // provedeme refresh hned (nebudeme cekat az se aktivuje panel)
                {
                    SalamanderGeneral->AddPluginFSTimer(100, this, 0); // oddalujeme refresh panelu, aby se stihla vratit connectiona z dialogu operace (po dokonceni operace se posila notifikace o zmene na ceste + zavira dialog operace)
                }
            }
        }
    }
}

void CPluginFSInterface::AddBookmark(HWND parent)
{
    char name[BOOKMARKNAME_MAX_SIZE];
    lstrcpyn(name, Host, BOOKMARKNAME_MAX_SIZE);
    if (CRenameDlg(parent, name, FALSE, TRUE).Execute() == IDOK)
    {
        BOOL anonymous = strcmp(User, FTP_ANONYMOUS) == 0;
        if (Config.FTPServerList.AddServer(name,
                                           Host,
                                           Path,
                                           anonymous,
                                           anonymous ? NULL : User,
                                           NULL,
                                           0,
                                           FALSE,
                                           -2,
                                           NULL,
                                           NULL,
                                           0,
                                           Port))
        {
            // bookmarka pridana -> nechame usera novou bookmarku umistit v seznamu bookmark
            CConnectDlg dlg(parent, 2);
            if (dlg.IsGood())
                dlg.Execute();
        }
    }
}

void CPluginFSInterface::SendUserFTPCommand(HWND parent)
{
    CSendFTPCommandDlg dlg(parent);
    if (ControlConnection != NULL && // (always true)
        dlg.Execute() == IDOK)
    {
        // ulozime cestu, na ktere budeme hlasit zmenu po provedeni prikazu
        char changedPath[2 * MAX_PATH];
        const char* fsName = ControlConnection->GetEncryptControlConnection() == 1 ? AssignedFSNameFTPS : AssignedFSName;
        sprintf(changedPath, "%s:", fsName);
        int len = (int)strlen(changedPath);
        MakeUserPart(changedPath + len, 2 * MAX_PATH - len);

        char cmdBuf[FTPCOMMAND_MAX_SIZE + 2];
        char logBuf[FTPCOMMAND_MAX_SIZE + 2];
        char txtBuf[200 + FTPCOMMAND_MAX_SIZE];
        lstrcpyn(cmdBuf, dlg.Command, FTPCOMMAND_MAX_SIZE);
        strcpy(logBuf, Config.SendSecretCommand ? LoadStr(IDS_SECRETFTPCOMMAND) : cmdBuf);
        _snprintf_s(txtBuf, _TRUNCATE, LoadStr(IDS_LOGMSGSENDINGUSRCMD), logBuf);
        ControlConnection->LogMessage(txtBuf, -1, TRUE);
        strcat(cmdBuf, "\r\n");
        strcat(logBuf, "\r\n");

        TotalConnectAttemptNum = 1; // zahajeni uzivatelem pozadovane akce -> je-li treba znovu pripojit, jde o 1. pokus reconnect
        const char* retryMsgAux = NULL;
        BOOL canRetry = FALSE;
        char retryMsgBuf[300];

        BOOL commandSent = FALSE;
        BOOL reconnected = FALSE;
        int ftpReplyCode;
        char ftpReplyBuf[20480]; // bereme jen prvnich 20 KB z reply message (snad je dost predimenzovane)
        while (ReconnectIfNeeded(parent, &reconnected, TRUE,
                                 &TotalConnectAttemptNum, retryMsgAux)) // bude-li potreba, reconnectneme se
        {
            BOOL run = FALSE;
            BOOL ok = TRUE;
            char newPath[FTP_MAX_PATH];
            BOOL needChangeDir = reconnected; // po reconnectu zkusime opet nastavit pracovni adresar
            if (!reconnected)                 // jsme jiz dele pripojeni, zkontrolujeme jestli pracovni adresar odpovida 'Path'
            {
                // vyuzijeme cache, v normalnich pripadech by tam cesta mela byt
                ok = ControlConnection->GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE,
                                                              &canRetry, retryMsgBuf, 300);
                if (!ok && canRetry) // "retry" je povolen
                {
                    run = TRUE;
                    retryMsgAux = retryMsgBuf;
                }
                if (ok && strcmp(newPath, Path) != 0) // nesedi pracovni adresar na serveru - nutna zmena
                    needChangeDir = TRUE;             // (predpoklad: server vraci stale stejny retezec pracovni cesty)
            }
            if (ok && needChangeDir) // je-li potreba zmenit pracovni adresar
            {
                int panel;
                BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
                BOOL success;
                // v SendChangeWorkingPath() je pri vypadku spojeni ReconnectIfNeeded(), nastesti to
                // nevadi, protoze kod predchazejici tomuto volani se provadi jen pokud k reconnectu
                // nedojde - "if (!reconnected)" - pokud dojde k reconnectu, jsou oba kody stejne
                ok = ControlConnection->SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT, parent, Path,
                                                              User, USER_MAX_SIZE, &success,
                                                              ftpReplyBuf, 700, NULL,
                                                              &TotalConnectAttemptNum, NULL, TRUE, NULL);
                if (ok && !success && Path[0] != 0) // send se povedl, ale server hlasi nejakou chybu (+ignorujeme chybu pri prazdne ceste) -> useruv prikaz nelze
                {                                   // poslat (muze byt svazan s akt. cestou v panelu)
                    char errBuf[900 + FTP_MAX_PATH];
                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), Path, ftpReplyBuf);
                    SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    ok = FALSE;
                }
            }
            if (ok)
            {
                commandSent = TRUE;
                if (ControlConnection->SendFTPCommand(parent, cmdBuf, logBuf, NULL,
                                                      ControlConnection->GetWaitTime(WAITWND_COMOPER), NULL,
                                                      &ftpReplyCode, ftpReplyBuf, 20480, FALSE,
                                                      TRUE, TRUE, &canRetry, retryMsgBuf,
                                                      300, NULL)) // nenastal fatal error
                {
                    if (dlg.RefreshWorkingPath) // nejspis doslo ke zmene na Path, zneplatnime listing Path v cache
                        UploadListingCache.ReportUnknownChange(User, Host, Port, Path, GetFTPServerPathType(Path));

                    if (dlg.ChangePathInPanel)
                    {
                        int panel;
                        BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
                        ok = ControlConnection->GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, TRUE,
                                                                      &canRetry, retryMsgBuf, 300);
                        if (!ok && canRetry) // "retry" je povolen
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                        if (ok && !notInPanel && strcmp(newPath, Path) != 0)
                        { // doslo ke zmene cesty (+ jinak vse o.k. a jsme v panelu ("always true")) -> zmenime
                            // cestu v panelu (predpoklad: server vraci stale stejny retezec pracovni cesty)
                            char newUserPart[FTP_USERPART_SIZE];
                            MakeUserPart(newUserPart, FTP_USERPART_SIZE, newPath);
                            ChangePathOnlyGetCurPathTime = GetTickCount(); // pro optimalizaci ChangePath() volane tesne po ziskani pracovni cesty
                            SalamanderGeneral->ChangePanelPathToPluginFS(panel, fsName, newUserPart);
                        }
                    }

                    if (ok) // vse dopadlo dobre, muzeme userovi ohlasit vysledek prikazu
                    {
                        char* s = logBuf + strlen(logBuf);
                        while (s > logBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                            s--;
                        *s = 0;                                                      // orizneme CRLF z konce textu prikazu do logu
                        CWelcomeMsgDlg(parent, ftpReplyBuf, TRUE, logBuf).Execute(); // ukazeme odpoved, nic vic nedelame...
                        SalamanderGeneral->WaitForESCRelease();
                    }
                }
                else
                {
                    if (dlg.RefreshWorkingPath) // mozna doslo ke zmene na Path, zneplatnime listing Path v cache
                        UploadListingCache.ReportUnknownChange(User, Host, Port, Path, GetFTPServerPathType(Path));

                    if (canRetry) // "retry" je povolen
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                }
            }
            if (!run)
                break;
        }

        if (dlg.RefreshWorkingPath && commandSent)
            SalamanderGeneral->PostChangeOnPathNotification(changedPath, TRUE | 0x02 /* soft refresh */);
    }
}

void CPluginFSInterface::ShowRawListing(HWND parent)
{
    CWelcomeMsgDlg(parent, PathListing, FALSE, NULL, TRUE, PathListingLen).Execute();
}

int CPluginFSInterface::GetLogUID()
{
    return (ControlConnection != NULL ? ControlConnection->GetLogUID() : -1);
}

void CPluginFSInterface::ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                     int panel, int selectedFiles, int selectedDirs)
{
    if (type == fscmPathInPanel || type == fscmPanel || // menu pro akt. cestu + pro panel budou shodne
        type == fscmItemsInPanel)                       // menu pro polozky bude obsahovat prikazy pro polozky
    {
        HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_ACTPATHCONTEXTMENU));
        if (main != NULL)
        {
            HMENU subMenu;
            BOOL destroySubMenu = FALSE;
            if (type == fscmItemsInPanel)
            {
                subMenu = CreatePopupMenu();
                destroySubMenu = TRUE;
            }
            else
                subMenu = GetSubMenu(main, 0);
            if (subMenu != NULL)
            {
                if (type != fscmItemsInPanel)
                {
                    SetTransferModeCheckMarksInSubMenu(subMenu, 0);
                    SetListHiddenFilesCheckInMenu(subMenu);
                    SetShowCertStateInMenu(subMenu);
                }
                else
                {
                    MENUITEMINFO mi;
                    char nameBuf[200];

                    BOOL isfocusedDir = FALSE;
                    SalamanderGeneral->GetPanelFocusedItem(panel, &isfocusedDir);

                    // vlozeni prikazu Salamandera
                    int i = 0;
                    int index = 0;
                    int salCmd;
                    BOOL enabled;
                    int type2, lastType = sctyUnknown;
                    while (SalamanderGeneral->EnumSalamanderCommands(&index, &salCmd, nameBuf, 200, &enabled, &type2))
                    {
                        if (!enabled || salCmd == SALCMD_OPEN || // "open" pro soubory zatim neumime, tak ho nepridame (umime ho jen u adresaru a tam je nezajimavy)
                            type2 == sctyForFocusedFile && isfocusedDir ||
                            (type2 != sctyForFocusedFile && type2 != sctyForFocusedFileOrDirectory &&
                             type2 != sctyForSelectedFilesAndDirectories))
                        {
                            continue;
                        }

                        if (type2 != lastType && lastType != sctyUnknown) // vlozeni separatoru
                        {
                            memset(&mi, 0, sizeof(mi));
                            mi.cbSize = sizeof(mi);
                            mi.fMask = MIIM_TYPE;
                            mi.fType = MFT_SEPARATOR;
                            InsertMenuItem(subMenu, i++, TRUE, &mi);
                        }
                        lastType = type2;

                        // vlozeni prikazu Salamandera
                        memset(&mi, 0, sizeof(mi));
                        mi.cbSize = sizeof(mi);
                        mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
                        mi.fType = MFT_STRING;
                        mi.wID = salCmd + 5000; // vsechny prikazy Salamandera odsuneme o 5000, aby se daly rozlisit od FTP prikazu
                        mi.dwTypeData = nameBuf;
                        mi.cch = (UINT)strlen(nameBuf);
                        mi.fState = MFS_ENABLED;
                        InsertMenuItem(subMenu, i++, TRUE, &mi);
                    }
                }

                DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             menuX, menuY, parent, NULL);
                if (cmd >= 5000)
                    SalamanderGeneral->PostSalamanderCommand(cmd - 5000);
                else
                {
                    switch (cmd)
                    {
                    case CM_DISCONNECTSRV:
                    {
                        GlobalDisconnectPanel = panel;
                        SalamanderGeneral->PostMenuExtCommand(FTPCMD_DISCONNECT, TRUE); // spusti se az v "sal-idle"
                        break;
                    }

                    case CM_REFRESHPATH:
                    {
                        SalamanderGeneral->PostRefreshPanelFS(this, FALSE);
                        break;
                    }

                    case CM_ADDBOOKMARK:
                    {
                        AddBookmark(parent);
                        break;
                    }

                    case CM_SENDFTPCOMMAND:
                    {
                        SendUserFTPCommand(parent);
                        break;
                    }

                    case CM_SHOWRAWLISTING:
                    {
                        ShowRawListing(parent);
                        break;
                    }

                    case CM_LISTHIDDENFILES:
                    {
                        ToggleListHiddenFiles(parent);
                        SalamanderGeneral->PostRefreshPanelFS(this, FALSE);
                        break;
                    }

                    case CM_SHOWLOG:
                    {
                        GlobalShowLogUID = GetLogUID();
                        if (GlobalShowLogUID == -1 || !Logs.HasLogWithUID(GlobalShowLogUID))
                        {
                            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_FSHAVENOLOG),
                                                              LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                        }
                        else                                                              // otevreme okno Logs s vybranym logem GlobalShowLogUID
                            SalamanderGeneral->PostMenuExtCommand(FTPCMD_SHOWLOGS, TRUE); // spusti se az v "sal-idle"
                        break;
                    }

                    case CM_TRMODEAUTO:
                    case CM_TRMODEASCII:
                    case CM_TRMODEBINARY:
                        SetTransferModeByMenuCmd(cmd);
                        break;

                    case CM_SHOWCERT:
                    {
                        ShowSecurityInfo(parent);
                        break;
                    }
                    }
                }
                if (destroySubMenu)
                    DestroyMenu(subMenu);
            }
            DestroyMenu(main);
        }
    }
}

void CPluginFSInterface::SetTransferModeCheckMarksInSubMenu(HMENU menu, int submenuNumber)
{
    HMENU trModeMenu = NULL;
    int count = GetMenuItemCount(menu);
    int i;
    for (i = 0; i < count; i++)
    {
        trModeMenu = GetSubMenu(menu, i);
        if (trModeMenu != NULL && submenuNumber-- == 0)
            break;
    }
    if (trModeMenu != NULL)
    {
        CheckMenuItem(trModeMenu, CM_TRMODEBINARY, MF_BYCOMMAND | (TransferMode == trmBinary ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(trModeMenu, CM_TRMODEASCII, MF_BYCOMMAND | (TransferMode == trmASCII ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(trModeMenu, CM_TRMODEAUTO, MF_BYCOMMAND | (TransferMode != trmBinary && TransferMode != trmASCII ? MF_CHECKED : MF_UNCHECKED));
    }
}

void CPluginFSInterface::SetTransferModeByMenuCmd(int cmd)
{
    switch (cmd)
    {
    case CM_TRMODEBINARY:
        TransferMode = trmBinary;
        break;
    case CM_TRMODEASCII:
        TransferMode = trmASCII;
        break;
    default:
        TransferMode = trmAutodetect;
        break;
    }
}

void CPluginFSInterface::SetTransferModeByMenuCmd2(int id)
{
    switch (id)
    {
    case FTPCMD_TRMODEBINARY:
        TransferMode = trmBinary;
        break;
    case FTPCMD_TRMODEASCII:
        TransferMode = trmASCII;
        break;
    default:
        TransferMode = trmAutodetect;
        break;
    }
}

DWORD
CPluginFSInterface::GetTransferModeCmdState(int id)
{
    switch (id)
    {
    case FTPCMD_TRMODEBINARY:
        return MENU_ITEM_STATE_ENABLED |
               (TransferMode == trmBinary ? MENU_ITEM_STATE_CHECKED | MENU_ITEM_STATE_RADIO : 0);
    case FTPCMD_TRMODEASCII:
        return MENU_ITEM_STATE_ENABLED |
               (TransferMode == trmASCII ? MENU_ITEM_STATE_CHECKED | MENU_ITEM_STATE_RADIO : 0);
    default:
        return MENU_ITEM_STATE_ENABLED |
               (TransferMode == trmAutodetect ? MENU_ITEM_STATE_CHECKED | MENU_ITEM_STATE_RADIO : 0);
    }
}

void CPluginFSInterface::SetListHiddenFilesCheckInMenu(HMENU menu)
{
    if (ControlConnection != NULL) // "always true"
    {
        CheckMenuItem(menu, CM_LISTHIDDENFILES, MF_BYCOMMAND | (ControlConnection->IsListCommandLIST_a() ? MF_CHECKED : MF_UNCHECKED));
    }
}

void CPluginFSInterface::SetShowCertStateInMenu(HMENU menu)
{
    if (ControlConnection != NULL) // "always true"
    {
        EnableMenuItem(menu, CM_SHOWCERT, MF_BYCOMMAND | (ControlConnection->GetEncryptControlConnection() == 1 ? MF_ENABLED : MF_GRAYED));
    }
}

void CPluginFSInterface::ToggleListHiddenFiles(HWND parent)
{
    if (Config.HintListHiddenFiles)
    {
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = parent;
        params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_HINT;
        params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
        params.Text = LoadStr(IDS_HINTLISTHIDDENFILES);
        params.CheckBoxText = LoadStr(IDS_DONOTSHOWHINTLISTHF);
        int doNotShowHint = !Config.HintListHiddenFiles;
        params.CheckBoxValue = &doNotShowHint;
        SalamanderGeneral->SalMessageBoxEx(&params);
        Config.HintListHiddenFiles = !doNotShowHint;
    }
    if (ControlConnection != NULL) // "always true"
        ControlConnection->ToggleListCommandLIST_a();
}

BOOL CPluginFSInterface::IsListCommandLIST_a()
{
    if (ControlConnection != NULL) // "always true"
        return ControlConnection->IsListCommandLIST_a();
    return FALSE;
}

//
// ****************************************************************************
// CSimpleListPluginDataInterface
//

void CSimpleListPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    char* s = (char*)file.PluginData;
    if (s != NULL)
        free(s);
}

// callback volany ze Salamandera pro ziskani textu
void WINAPI GetRowText()
{
    char* s = (char*)((*TransferFileData)->PluginData);
    if (s != NULL)
    {
        *TransferLen = (int)strlen(s);
        memcpy(TransferBuffer, s, *TransferLen);
    }
    else
    {
        *TransferLen = 0;
    }
}

void CSimpleListPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view,
                                               const char* archivePath, const CFileData* upperDir)
{
    view->SetViewMode(VIEW_MODE_DETAILED, VALID_DATA_NONE); // prepneme do Detailed rezimu + zrusime ostatni sloupce, nechame jen sloupec Name
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen, TransferRowData,
                               TransferPluginDataIface, TransferActCustomData);

    // zmenime jmeno std. sloupce Name na Line
    view->SetColumnName(0, LoadStr(IDS_LINENUMCOLUMN), LoadStr(IDS_LINENUMCOLUMNDESC));

    CColumn column;
    lstrcpyn(column.Name, LoadStr(IDS_RAWLISTCOLUMN), COLUMN_NAME_MAX);
    lstrcpyn(column.Description, LoadStr(IDS_RAWLISTCOLUMNDESC), COLUMN_DESCRIPTION_MAX);
    column.GetText = GetRowText;
    column.SupportSorting = 0;
    column.LeftAlignment = 1;
    column.ID = COLUMN_ID_CUSTOM;
    column.Width = leftPanel ? LOWORD(ListingColumnWidth) : HIWORD(ListingColumnWidth);
    column.FixedWidth = leftPanel ? LOWORD(ListingColumnFixedWidth) : HIWORD(ListingColumnFixedWidth);
    view->InsertColumn(1, &column);
}

void CSimpleListPluginDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    if (leftPanel)
        ListingColumnFixedWidth = MAKELONG(newFixedWidth, HIWORD(ListingColumnFixedWidth));
    else
        ListingColumnFixedWidth = MAKELONG(LOWORD(ListingColumnFixedWidth), newFixedWidth);
    if (newFixedWidth)
        ColumnWidthWasChanged(leftPanel, column, column->Width);
}

void CSimpleListPluginDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    if (leftPanel)
        ListingColumnWidth = MAKELONG(newWidth, HIWORD(ListingColumnWidth));
    else
        ListingColumnWidth = MAKELONG(LOWORD(ListingColumnWidth), newWidth);
}

BOOL CSimpleListPluginDataInterface::GetInfoLineContent(int panel, const CFileData* file, BOOL isDir,
                                                        int selectedFiles, int selectedDirs,
                                                        BOOL displaySize, const CQuadWord& selectedSize,
                                                        char* buffer, DWORD* hotTexts, int& hotTextsCount)
{
    if (file != NULL)
    {
        const char* s = (char*)(file->PluginData);
        if (s == NULL)
            s = "";
        _snprintf_s(buffer, 1000, _TRUNCATE, "%s: %s", file->Name, s);
        if (*s != 0)
        {
            int off = (int)strlen(file->Name) + 2;
            hotTexts[0] = MAKELONG(off, strlen(buffer) - off);
            hotTextsCount = 1;
        }
        else
            hotTextsCount = 0;
        return TRUE;
    }
    else
        return FALSE; // pocty oznacenych polozek a info o prazdnem panelu at vypise Salamander
}

//
// ****************************************************************************
// CFTPListingPluginDataInterface
//

CFTPListingPluginDataInterface::CFTPListingPluginDataInterface(TIndirectArray<CSrvTypeColumn>* columns,
                                                               BOOL deleteColumns, DWORD validDataMask,
                                                               BOOL isVMS)
{
    Columns = columns;
    DeleteColumns = deleteColumns;
    ItemDataSize = 0;
    BlocksColumnOffset = -1;
    BytesColumnOffset = -1;
    DateColumnOffset = -1;
    TimeColumnOffset = -1;
    DataOffsets = new DWORD[Columns->Count];
    if (DataOffsets != NULL)
    {
        BOOL hasSize = FALSE;
        int i;
        for (i = 0; i < Columns->Count; i++)
        {
            switch (Columns->At(i)->Type)
            {
            case stctSize:
            {
                hasSize = TRUE;
                BlocksColumnOffset = -1; // CFileData::Size ma vyssi prioritu nez obecna velikost v blocich
                BytesColumnOffset = -1;  // CFileData::Size ma vyssi prioritu nez obecna velikost v bytech
                DataOffsets[i] = -1;     // ulozeni dat primo v CFileData
                break;
            }

            case stctDate:
            {
                if (DateColumnOffset == -1)
                {
                    int nameID = Columns->At(i)->NameID;
                    if (nameID == 3 /* date */) // nalezen sloupec s datumem posledniho zapisu (bereme prvni zleva)
                        DateColumnOffset = -2;  // ulozeni v CFileData::LastWrite
                }
                DataOffsets[i] = -1; // ulozeni dat primo v CFileData
                break;
            }

            case stctTime:
            {
                if (TimeColumnOffset == -1)
                {
                    int nameID = Columns->At(i)->NameID;
                    if (nameID == 4 /* time */) // nalezen sloupec s casem posledniho zapisu (bereme prvni zleva)
                        TimeColumnOffset = -2;  // ulozeni v CFileData::LastWrite
                }
                DataOffsets[i] = -1; // ulozeni dat primo v CFileData
                break;
            }

            case stctGeneralText:
            {
                DataOffsets[i] = ItemDataSize;
                ItemDataSize += sizeof(char*);
                break;
            }

            case stctGeneralDate:
            {
                if (DateColumnOffset == -1)
                {
                    int nameID = Columns->At(i)->NameID;
                    if (nameID == 3 /* date */) // nalezen sloupec s datumem posledniho zapisu (bereme prvni zleva)
                        DateColumnOffset = ItemDataSize;
                }
                DataOffsets[i] = ItemDataSize;
                ItemDataSize += sizeof(CFTPDate);
                break;
            }

            case stctGeneralTime:
            {
                if (TimeColumnOffset == -1)
                {
                    int nameID = Columns->At(i)->NameID;
                    if (nameID == 4 /* time */) // nalezen sloupec s casem posledniho zapisu (bereme prvni zleva)
                        TimeColumnOffset = ItemDataSize;
                }
                DataOffsets[i] = ItemDataSize;
                ItemDataSize += sizeof(CFTPTime);
                break;
            }

            case stctGeneralNumber:
            {
                if (!hasSize && BytesColumnOffset == -1)
                {
                    int nameID = Columns->At(i)->NameID;
                    if (nameID == 2 /* size */)
                    {                            // nalezen sloupec s velikostmi souboru v bytech (bereme prvni zleva)
                        BlocksColumnOffset = -1; // obecna velikost v bytech ma vetsi prioritu nez v blocich
                        BytesColumnOffset = ItemDataSize;
                    }
                    else
                    {
                        if ((nameID == 10 /* block size */ || nameID == 22 /* Physical Block Length */) &&
                            BlocksColumnOffset == -1)
                        { // nalezen sloupec s velikostmi souboru v blocich (bereme prvni zleva)
                            BlocksColumnOffset = ItemDataSize;
                        }
                    }
                }
                DataOffsets[i] = ItemDataSize;
                ItemDataSize += sizeof(__int64);
                break;
            }

            default:
                DataOffsets[i] = -1;
                break; // ulozeni dat primo v CFileData
            }
        }
    }
    IsVMS = isVMS;
    ValidDataMask = validDataMask | GetPLValidDataMask(); // musi se volat az po dohledani offsetu BytesColumnOffset, atd.
}

CFTPListingPluginDataInterface::~CFTPListingPluginDataInterface()
{
    if (DataOffsets != NULL)
        delete[] DataOffsets;
    if (DeleteColumns)
        delete Columns; // slo jen o kopii, musime ji dealokovat
}

void CFTPListingPluginDataInterface::StoreStringToColumn(CFileData& file, int column, char* str)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreStringToColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralText)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreStringToColumn(): Unexpected type of column!");
        return;
    }
#endif
    char** data = (char**)(((char*)file.PluginData) + DataOffsets[column]);
    if (*data != NULL)
        free(*data);
    *data = str;
}

void CFTPListingPluginDataInterface::StoreDayToColumn(CFileData& file, int column, BYTE day)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreDayToColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralDate)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreDayToColumn(): Unexpected type of column!");
        return;
    }
#endif
    CFTPDate* data = (CFTPDate*)(((char*)file.PluginData) + DataOffsets[column]);
    data->Day = day;
}

void CFTPListingPluginDataInterface::StoreMonthToColumn(CFileData& file, int column, BYTE month)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreMonthToColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralDate)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreMonthToColumn(): Unexpected type of column!");
        return;
    }
#endif
    CFTPDate* data = (CFTPDate*)(((char*)file.PluginData) + DataOffsets[column]);
    data->Month = month;
}

void CFTPListingPluginDataInterface::StoreYearToColumn(CFileData& file, int column, WORD year)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreYearToColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralDate)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreYearToColumn(): Unexpected type of column!");
        return;
    }
#endif
    CFTPDate* data = (CFTPDate*)(((char*)file.PluginData) + DataOffsets[column]);
    data->Year = year;
}

void CFTPListingPluginDataInterface::StoreDateToColumn(CFileData& file, int column, BYTE day,
                                                       BYTE month, WORD year)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreDateToColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralDate)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreDateToColumn(): Unexpected type of column!");
        return;
    }
#endif
    CFTPDate* data = (CFTPDate*)(((char*)file.PluginData) + DataOffsets[column]);
    data->Day = day;
    data->Month = month;
    data->Year = year;
}

void CFTPListingPluginDataInterface::StoreTimeToColumn(CFileData& file, int column, BYTE hour, BYTE minute,
                                                       BYTE second, WORD millisecond)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreTimeToColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralTime)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreTimeToColumn(): Unexpected type of column!");
        return;
    }
#endif
    CFTPTime* data = (CFTPTime*)(((char*)file.PluginData) + DataOffsets[column]);
    data->Hour = hour;
    data->Minute = minute;
    data->Second = second;
    data->Millisecond = millisecond;
}

void CFTPListingPluginDataInterface::StoreNumberToColumn(CFileData& file, int column, __int64 number)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreNumberToColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralNumber)
    {
        TRACE_E("CFTPListingPluginDataInterface::StoreNumberToColumn(): Unexpected type of column!");
        return;
    }
#endif
    __int64* data = (__int64*)(((char*)file.PluginData) + DataOffsets[column]);
    *data = number;
}

char* CFTPListingPluginDataInterface::GetStringFromColumn(const CFileData& file, int column)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetStringFromColumn(): Invalid column index!");
        return NULL;
    }
    if (Columns->At(column)->Type != stctGeneralText)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetStringFromColumn(): Unexpected type of column!");
        return NULL;
    }
#endif
    return *(char**)(((char*)file.PluginData) + DataOffsets[column]);
}

void CFTPListingPluginDataInterface::GetDateFromColumn(const CFileData& file, int column, SYSTEMTIME* stVal)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetDateFromColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralDate)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetDateFromColumn(): Unexpected type of column!");
        return;
    }
#endif
    CFTPDate* data = (CFTPDate*)(((char*)file.PluginData) + DataOffsets[column]);
    stVal->wDay = data->Day;
    stVal->wMonth = data->Month;
    stVal->wYear = data->Year;
}

void CFTPListingPluginDataInterface::GetDateFromColumn(const CFileData& file, int column, CFTPDate* date)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetDateFromColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralDate)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetDateFromColumn(): Unexpected type of column!");
        return;
    }
#endif
    *date = *(CFTPDate*)(((char*)file.PluginData) + DataOffsets[column]);
}

void CFTPListingPluginDataInterface::GetTimeFromColumn(const CFileData& file, int column, SYSTEMTIME* stVal)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetTimeFromColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralTime)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetTimeFromColumn(): Unexpected type of column!");
        return;
    }
#endif
    CFTPTime* data = (CFTPTime*)(((char*)file.PluginData) + DataOffsets[column]);
    stVal->wHour = data->Hour;
    stVal->wMinute = data->Minute;
    stVal->wSecond = data->Second;
    stVal->wMilliseconds = data->Millisecond;
}

void CFTPListingPluginDataInterface::GetTimeFromColumn(const CFileData& file, int column, CFTPTime* time)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetTimeFromColumn(): Invalid column index!");
        return;
    }
    if (Columns->At(column)->Type != stctGeneralTime)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetTimeFromColumn(): Unexpected type of column!");
        return;
    }
#endif
    *time = *(CFTPTime*)(((char*)file.PluginData) + DataOffsets[column]);
}

__int64
CFTPListingPluginDataInterface::GetNumberFromColumn(const CFileData& file, int column)
{
#ifdef _DEBUG
    if (column < 0 || column >= Columns->Count)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetNumberFromColumn(): Invalid column index!");
        return 0;
    }
    if (Columns->At(column)->Type != stctGeneralNumber)
    {
        TRACE_E("CFTPListingPluginDataInterface::GetNumberFromColumn(): Unexpected type of column!");
        return 0;
    }
#endif
    return *(__int64*)(((char*)file.PluginData) + DataOffsets[column]);
}

BOOL CFTPListingPluginDataInterface::AllocPluginData(CFileData& file)
{
    if (ItemDataSize == 0) // zadna data nejsou ukladana mimo CFileData
    {
        file.PluginData = 0;
        return TRUE;
    }

    char* base = (char*)(file.PluginData = (DWORD_PTR)malloc(ItemDataSize));
    if ((void*)(file.PluginData) == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    memset(base, 0, ItemDataSize); // retezce byt nulovane musi, zbytek je nulovany jen pro prehlednost
    return TRUE;
}

void CFTPListingPluginDataInterface::ClearPluginData(CFileData& file)
{
    if (file.PluginData != 0)
    {
        char* base = (char*)file.PluginData;
        int i;
        for (i = 0; i < Columns->Count; i++)
        {
            if (Columns->At(i)->Type == stctGeneralText)
            {
                char* data = *(char**)(base + DataOffsets[i]);
                if (data != NULL)
                    SalamanderGeneral->Free(data);
            }
            // stctGeneralDate, stctGeneralTime a stctGeneralNumber nejsou alokovane, uvolni se spolu s file.PluginData
        }
        memset(base, 0, ItemDataSize); // retezce byt nulovane musi, zbytek je nulovany jen pro prehlednost
    }
}

void CFTPListingPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    if (file.PluginData != 0)
    {
        char* base = (char*)file.PluginData;
        int i;
        for (i = 0; i < Columns->Count; i++)
        {
            if (Columns->At(i)->Type == stctGeneralText)
            {
                char* data = *(char**)(base + DataOffsets[i]);
                if (data != NULL)
                    SalamanderGeneral->Free(data);
            }
            // stctGeneralDate, stctGeneralTime a stctGeneralNumber nejsou alokovane, uvolni se spolu s file.PluginData
        }
        free(base);
    }
}

int CFTPListingPluginDataInterface::FindRightsColumn()
{
    int i;
    for (i = 0; i < Columns->Count; i++)
    {
        CSrvTypeColumn* col = Columns->At(i);
        if (col->NameID == 6 /* Rights */ && col->Type == stctGeneralText)
            return i;
    }
    return -1;
}

BOOL CFTPListingPluginDataInterface::GetSize(const CFileData& file, CQuadWord& size, BOOL& inBytes)
{
    if (ValidDataMask & VALID_DATA_SIZE)
    {
        size = file.Size;
        inBytes = TRUE;
        return TRUE;
    }
    else
    {
        if (BytesColumnOffset != -1)
        {
            size.SetUI64(*(__int64*)(((char*)(file.PluginData)) + BytesColumnOffset));
            if (size.Value == INT64_EMPTYNUMBER)
                size.Set(0, 0); // je-li ve sloupci prazdna hodnota, vracime nulovou velikost
            inBytes = TRUE;
            return TRUE;
        }
        else
        {
            if (BlocksColumnOffset != -1)
            {
                size.SetUI64(*(__int64*)(((char*)(file.PluginData)) + BlocksColumnOffset));
                if (size.Value == INT64_EMPTYNUMBER)
                    size.Set(0, 0); // je-li ve sloupci prazdna hodnota, vracime nulovou velikost
                inBytes = FALSE;
                return TRUE;
            }
            else
                return FALSE;
        }
    }
}

void CFTPListingPluginDataInterface::GetBasicName(const CFileData& file, char** name,
                                                  char** ext, char* buffer)
{
    if (IsVMS)
    {
        memcpy(buffer, file.Name, file.NameLen + 1);
        if (FTPVMSCutFileVersion(buffer, file.NameLen))
        {
            *name = buffer;
            if (file.NameLen > (DWORD)(file.Ext - file.Name))
                *ext = buffer + (file.Ext - file.Name);
            else
                *ext = buffer + strlen(buffer); // pro pripad, ze jmeno nema priponu (jinak je ';' vzdy za priponou)
            return;                             // uspech, koncime
        }
        else
            TRACE_E("Unexpected situation in CFTPListingPluginDataInterface::GetBasicName(): not "
                    "a VMS name? ("
                    << file.Name << ")");
    }
    *name = file.Name;
    *ext = file.Ext;
}

void CFTPListingPluginDataInterface::GetLastWriteDateAndTime(const CFileData& file, BOOL* dateAndTimeValid,
                                                             CFTPDate* date, CFTPTime* time)
{
    *dateAndTimeValid = (TimeColumnOffset != -1 || DateColumnOffset != -1);
    if (TimeColumnOffset == -2 || DateColumnOffset == -2)
    {
        DWORD mask = (TimeColumnOffset == -2 ? VALID_DATA_TIME : 0) | (DateColumnOffset == -2 ? VALID_DATA_DATE : 0);
        if ((ValidDataMask & mask) == mask) // jen pro sychr
        {
            FILETIME ft;
            SYSTEMTIME st;
            FileTimeToLocalFileTime(&file.LastWrite, &ft);
            FileTimeToSystemTime(&ft, &st);
            date->Year = st.wYear;
            date->Month = (BYTE)st.wMonth;
            date->Day = (BYTE)st.wDay;
            time->Hour = st.wHour;
            time->Minute = st.wMinute;
            time->Second = st.wSecond;
            time->Millisecond = st.wMilliseconds;
        }
        else
        {
            TRACE_E("Unexpected situation in CFTPListingPluginDataInterface::GetLastWriteDateAndTime()!");
            *dateAndTimeValid = FALSE;
        }
    }
    if (*dateAndTimeValid)
    {
        if (DateColumnOffset == -1)
            memset(date, 0, sizeof(CFTPDate)); // neznamy datum = pouzijeme "prazdnou hodnotu" (date->Day==0)
        else
        {
            if (DateColumnOffset != -2)
                *date = *(CFTPDate*)(((char*)(file.PluginData)) + DateColumnOffset);
        }
        if (TimeColumnOffset == -1 || date->Day == 0) // neznamy cas nebo neznamy ("prazdna hodnota") datum (nastavovat cas bez datumu je nesmysl -> cas je v tomto pripade proste povazovan tez za neznamy)
        {                                             // neznamy cas: mozna pouzijeme "prazdnou hodnotu" (viz dalsi upravy 'time')
            memset(time, 0, sizeof(CFTPTime));
            time->Hour = 24;
        }
        else
        {
            if (TimeColumnOffset != -2)
                *time = *(CFTPTime*)(((char*)(file.PluginData)) + TimeColumnOffset);
        }

        // je-li neznamy cas a znamy datum, pak cas upravime na 0:00:00 (nepouzijeme "prazdnou hodnotu")
        if (time->Hour == 24 && date->Day != 0)
            memset(time, 0, sizeof(CFTPTime));
    }
}

BOOL CFTPListingPluginDataInterface::GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size)
{
    if (BytesColumnOffset != -1)
    {
        size->SetUI64(*(__int64*)(((char*)(file->PluginData)) + BytesColumnOffset));
        return size->Value != INT64_EMPTYNUMBER; // TRUE vracime jen pokud je velikost znama
    }
    return FALSE;
}

BOOL CFTPListingPluginDataInterface::GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date)
{
    if (DateColumnOffset != -1 && DateColumnOffset != -2)
    {
        CFTPDate d = *(CFTPDate*)(((char*)(file->PluginData)) + DateColumnOffset);
        if (d.Day != 0) // pokud nejde o prazdnou hodnotu datumu
        {
            date->wYear = d.Year;
            date->wMonth = d.Month;
            date->wDay = d.Day;
            date->wDayOfWeek = 0;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CFTPListingPluginDataInterface::GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time)
{
    if (TimeColumnOffset != -1 && TimeColumnOffset != -2)
    {
        CFTPTime t = *(CFTPTime*)(((char*)(file->PluginData)) + TimeColumnOffset);
        if (t.Hour != 24) // pokud nejde o prazdnou hodnotu casu
        {
            time->wHour = t.Hour;
            time->wMinute = t.Minute;
            time->wSecond = t.Second;
            time->wMilliseconds = t.Millisecond;
            return TRUE;
        }
    }
    return FALSE;
}

DWORD
CFTPListingPluginDataInterface::GetPLValidDataMask()
{
    return (BytesColumnOffset != -1 ? VALID_DATA_PL_SIZE : 0) |
           (DateColumnOffset != -1 && DateColumnOffset != -2 ? VALID_DATA_PL_DATE : 0) |
           (TimeColumnOffset != -1 && TimeColumnOffset != -2 ? VALID_DATA_PL_TIME : 0);
}
