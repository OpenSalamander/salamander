// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "unmime.rh"
#include "unmime.rh2"
#include "lang\lang.rh"
#include "parser.h"
#include "decoder.h"
#include "unmime.h"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro archivator
CPluginInterfaceForArchiver InterfaceForArchiver;
// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;
// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
CSalamanderGUIAbstract* SalamanderGUI = NULL;
// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;
// PluginDataInterface
CPluginDataInterface PluginDataInterface;

// konfigurace
SGlobals G;

// jmena klicu v Registry
LPCTSTR KEY_LISTMAILHEADERS = _T("List Mail Headers");
LPCTSTR KEY_LISTMESSAGEBODIES = _T("List Message Bodies");
LPCTSTR KEY_APPENDCHARSET = _T("Append Charset");
LPCTSTR KEY_LISTATTACHMENTS = _T("List Attachments");
LPCTSTR KEY_DONTSHOWANYMORE = _T("DontShowAnymore");

// ConfigVersion: 0 - default (bez loadu),
//                1 - verze s konfiguraci v registry (uz byla pouzivana, jen bez cisla konfigurace)
//                2 - verze po zavedeni cisla konfigurace + yEnc
//                3 - Pridana podpora CNM (Mercury + Pegasus Mail (PMail))

int ConfigVersion = 0;
#define CURRENT_CONFIG_VERSION 3
const char* CONFIG_VERSION = "Version";

// ****************************************************************************

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DLLInstance = hinstDLL;
    return TRUE; // DLL can be loaded
}

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

//****************************************************************************

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

// ****************************************************************************

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (salamander->GetVersion() < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "UnMIME" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnMIME" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();
    SalamanderGUI = salamander->GetSalamanderGUI();

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnMIME" /* neprekladat! */, "eml;b64;uue;xxe;hqx;ntx;cnm");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

// ****************************************************************************
//
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT_TITLE), MB_OK | MB_ICONINFORMATION);
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    G.bListMailHeaders = FALSE; // default
    G.bListMessageBodies = TRUE;
    G.bAppendCharset = TRUE;
    G.bListAttachments = TRUE;
    G.nDontShowAnymore = 0;
    if (regKey != NULL) // load z registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
        {
            ConfigVersion = 1; // verze s konfiguraci v registry (uz byla pouzivana, jen bez cisla konfigurace)
        }
        registry->GetValue(regKey, KEY_LISTMAILHEADERS, REG_DWORD, &G.bListMailHeaders, sizeof(DWORD));
        registry->GetValue(regKey, KEY_LISTMESSAGEBODIES, REG_DWORD, &G.bListMessageBodies, sizeof(DWORD));
        registry->GetValue(regKey, KEY_APPENDCHARSET, REG_DWORD, &G.bAppendCharset, sizeof(DWORD));
        registry->GetValue(regKey, KEY_LISTATTACHMENTS, REG_DWORD, &G.bListAttachments, sizeof(DWORD));
        registry->GetValue(regKey, KEY_DONTSHOWANYMORE, REG_DWORD, &G.nDontShowAnymore, sizeof(DWORD));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");
    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    registry->SetValue(regKey, KEY_LISTMAILHEADERS, REG_DWORD, &G.bListMailHeaders, sizeof(DWORD));
    registry->SetValue(regKey, KEY_LISTMESSAGEBODIES, REG_DWORD, &G.bListMessageBodies, sizeof(DWORD));
    registry->SetValue(regKey, KEY_APPENDCHARSET, REG_DWORD, &G.bAppendCharset, sizeof(DWORD));
    registry->SetValue(regKey, KEY_LISTATTACHMENTS, REG_DWORD, &G.bListAttachments, sizeof(DWORD));
    registry->SetValue(regKey, KEY_DONTSHOWANYMORE, REG_DWORD, &G.nDontShowAnymore, sizeof(DWORD));
}

INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hWnd);
        CheckDlgButton(hWnd, IDC_CHECK_HEADERS, G.bListMailHeaders ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_BODIES, G.bListMessageBodies ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_APPEND, G.bAppendCharset ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_ATTACHMENTS, G.bListAttachments ? BST_CHECKED : BST_UNCHECKED);
        SendMessage(hWnd, WM_COMMAND, IDC_CHECK_BODIES, 0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            G.bListMailHeaders = IsDlgButtonChecked(hWnd, IDC_CHECK_HEADERS) == BST_CHECKED;
            G.bListMessageBodies = IsDlgButtonChecked(hWnd, IDC_CHECK_BODIES) == BST_CHECKED;
            G.bAppendCharset = IsDlgButtonChecked(hWnd, IDC_CHECK_APPEND) == BST_CHECKED;
            G.bListAttachments = IsDlgButtonChecked(hWnd, IDC_CHECK_ATTACHMENTS) == BST_CHECKED;
            // FALL THROUGH
        case IDCANCEL:
            EndDialog(hWnd, 0);
            return TRUE;

        case IDC_CHECK_BODIES:
            EnableWindow(GetDlgItem(hWnd, IDC_CHECK_APPEND), IsDlgButtonChecked(hWnd,
                                                                                IDC_CHECK_BODIES) == BST_CHECKED);
            return TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

void CPluginInterface::Configuration(HWND parent)
{
    DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CONFIG), parent, DlgProc, 0);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // zakladni cast:
    salamander->AddCustomUnpacker("UnMIME (Plugin)", "*.eml;*.b64;*.uue;*.xxe;*.hqx;*.ntx;*.cnm",
                                  ConfigVersion < 3);
    salamander->AddPanelArchiver("eml;b64;uue;xxe;hqx;ntx;cnm", FALSE, FALSE);

    // cast pro upgrady:
    if (ConfigVersion < 2) // verze po zavedeni cisla konfigurace + yEnc
    {
        salamander->AddPanelArchiver("ntx", FALSE, TRUE); // pridani pripony "ntx"
    }
    if (ConfigVersion < 3) // cnm: file format used by Mercury & PMail emailing system
    {
        salamander->AddPanelArchiver("cnm", FALSE, TRUE); // pridani pripony "cnm"
    }
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    return &InterfaceForArchiver;
}

// ****************************************************************************
//
// CPluginInterfaceForArchiver
//

static void ShowBadBlockMessage(CStartMarker* m, BOOL bDontShow[][2])
{
    int e = (m->iEncoding == ENCODING_BINHEX) ? 0 : 1;
    if (!bDontShow[e][m->iBadBlock - 1])
    {
        char text[200];
        sprintf(text, LoadStr((m->iBadBlock == BADBLOCK_DAMAGED) ? IDS_BADBLOCK_DAMAGED : IDS_BADBLOCK_CRC),
                e ? "yEncode" : "BinHex");
        SalamanderGeneral->ShowMessageBox(text, LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        bDontShow[e][m->iBadBlock - 1] = TRUE;
    }
}

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);
    pluginData = &PluginDataInterface;

    // alokace ArchiveData - bude obsahovat data o obsahu "archivu"
    CArchiveData* ArchiveData = new CArchiveData;
    ArchiveData->RefCount = 0;

    // zjistim filetime "archivu" - stejny budou mit i soubory uvnitr
    HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        GetFileTime(hFile, NULL, NULL, &ArchiveData->ft);
        CloseHandle(hFile);
    }

    // analyza souboru
    if (!ParseMailFile(fileName, &ArchiveData->ParserOutput, G.bAppendCharset))
    {
        delete ArchiveData;
        if (iErrorStr != -1)
            SalamanderGeneral->ShowMessageBox(LoadStr(iErrorStr), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    CFileData fd;
    int ret = TRUE;
    BOOL bDontShow[2][2] = {{0, 0}, {0, 0}}, bDontShowAgainUnknown = FALSE;
    // vylistovani nalezenych souboru
    int i;
    for (i = 0; i < ArchiveData->ParserOutput.Markers.Count; i++)
    {
        if (ArchiveData->ParserOutput.Markers[i]->iMarkerType == MARKER_START)
        {
            CStartMarker* m = (CStartMarker*)ArchiveData->ParserOutput.Markers[i];
            if (m->bEmpty)
                continue;
            if (m->iBadBlock)
            {
                ShowBadBlockMessage(m, bDontShow);
                continue;
            }
            if (m->iEncoding != ENCODING_UNKNOWN)
            {
                if ((m->iBlockType == BLOCK_MAINHEADER && G.bListMailHeaders) ||
                    (m->iBlockType == BLOCK_BODY && !m->bAttachment && G.bListMessageBodies) ||
                    (m->iBlockType == BLOCK_BODY && m->bAttachment && G.bListAttachments))
                {
                    ZeroMemory(&fd, sizeof(fd));

                    // must allocate using SalamanderGeneral: see CFileData in spl-com.h
                    fd.Name = SalamanderGeneral->DupStr(m->cFileName);
                    if (fd.Name == NULL)
                    {
                        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                        ret = FALSE;
                        break;
                    }
                    fd.NameLen = strlen(fd.Name);
                    fd.Ext = strrchr(fd.Name, '.');
                    if (fd.Ext != NULL)
                        fd.Ext++; // ".cvspass" ve Windows je pripona
                    else
                        fd.Ext = fd.Name + fd.NameLen;
                    fd.Size = CQuadWord(m->iSize, 0);
                    fd.Attr = FILE_ATTRIBUTE_NORMAL;
                    fd.Hidden = 0;
                    fd.PluginData = (DWORD_PTR)ArchiveData;
                    fd.LastWrite = ArchiveData->ft;
                    fd.DosName = NULL;
                    fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
                    fd.IsOffline = 0;
                    if (!dir->AddFile("", fd, NULL))
                    {
                        SalamanderGeneral->Free(fd.Name);
                        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LIST), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                        ret = FALSE;
                        break;
                    }
                    ArchiveData->RefCount++;
                }
            }
            else if (!bDontShowAgainUnknown)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_UNKNOWN), LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
                bDontShowAgainUnknown = TRUE;
            }
        }
    }

    if (!ArchiveData->RefCount)
        delete ArchiveData;
    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                                const char* nameInArchive, const CFileData* fileData,
                                                const char* targetDir, const char* newFileName,
                                                BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackOneFile(, %s, , %s, , %s, ,)", fileName,
                        nameInArchive, targetDir);

    if (newFileName != NULL)
    {
        *renamingNotSupported = TRUE;
        return FALSE;
    }

    CArchiveData* pAD = (CArchiveData*)fileData->PluginData;
    pAD->ParserOutput.UnselectAll();
    pAD->ParserOutput.SelectBlock(nameInArchive);
    if (!DecodeSelectedBlocks(fileName, &pAD->ParserOutput, targetDir, &pAD->ft,
                              NULL, CQuadWord(0, 0), NULL, TRUE))
    {
        if (iErrorStr != -1)
            SalamanderGeneral->ShowMessageBox(LoadStr(iErrorStr), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* Salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    LPCTSTR nextName;
    BOOL isDir;
    CQuadWord size, totalSize(0, 0);
    const CFileData* fileData;
    CArchiveData* pAD = NULL;
    // oznacim pozadovane soubory
    while ((nextName = next(NULL, 0, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
    {
        if (pAD == NULL)
        {
            pAD = (CArchiveData*)fileData->PluginData;
            pAD->ParserOutput.UnselectAll();
        }
        pAD->ParserOutput.SelectBlock(nextName);
        totalSize += size;
    }
    // test volneho mista
    if (!SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(), targetDir, totalSize, LoadStr(IDS_PLUGINNAME)))
        return FALSE;
    // openprogressdialog
    char title[MAX_PATH + 32];
    sprintf(title, LoadStr(IDS_EXTRTITLE), SalamanderGeneral->SalPathFindFileName(fileName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    // vypakovani
    int ret = TRUE;
    BOOL bAborted;
    if (!DecodeSelectedBlocks(fileName, &pAD->ParserOutput, targetDir,
                              &pAD->ft, Salamander, totalSize, &bAborted) &&
        !bAborted)
    {
        if (iErrorStr != -1)
            SalamanderGeneral->ShowMessageBox(LoadStr(iErrorStr), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        ret = FALSE;
    }
    if (bAborted)
        ret = FALSE;

    Salamander->CloseProgressDialog();
    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* Salamander, LPCTSTR fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);
    CArchiveData ArchiveData;
    // zjistim filetime "archivu" - stejny budou mit i soubory uvnitr
    HANDLE hFile = CreateFile(fileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        GetFileTime(hFile, NULL, NULL, &ArchiveData.ft);
        CloseHandle(hFile);
    }
    // openprogressdialog
    char title[MAX_PATH + 32];
    sprintf(title, LoadStr(IDS_EXTRTITLE), SalamanderGeneral->SalPathFindFileName(fileName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PARSE), FALSE);
    // analyza souboru
    if (!ParseMailFile(fileName, &ArchiveData.ParserOutput, G.bAppendCharset))
    {
        if (iErrorStr != -1)
            SalamanderGeneral->ShowMessageBox(LoadStr(iErrorStr), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        Salamander->CloseProgressDialog();
        return FALSE;
    }
    if (delArchiveWhenDone)
        archiveVolumes->Add(fileName, -2);
    // oznacim soubory - stejnym zpusobem jako pri listovani
    ArchiveData.ParserOutput.UnselectAll();
    CQuadWord totalSize(0, 0);
    BOOL bDontShow[2][2] = {{0, 0}, {0, 0}};
    int i;
    for (i = 0; i < ArchiveData.ParserOutput.Markers.Count; i++)
    {
        CStartMarker* m = (CStartMarker*)ArchiveData.ParserOutput.Markers[i];
        if (m->iMarkerType == MARKER_START)
        {
            if (m->bEmpty)
                continue;
            if (m->iBadBlock)
            {
                ShowBadBlockMessage(m, bDontShow);
                continue;
            }
            if (m->iEncoding != ENCODING_UNKNOWN)
            {
                if ((m->iBlockType == BLOCK_MAINHEADER && G.bListMailHeaders) ||
                    (m->iBlockType == BLOCK_BODY && !m->bAttachment && G.bListMessageBodies) ||
                    (m->iBlockType == BLOCK_BODY && m->bAttachment && G.bListAttachments))
                {
                    m->bSelected = 1;
                    totalSize += CQuadWord(m->iSize, 0);
                }
            }
        }
    }
    // vypakovani
    int ret = TRUE;
    BOOL bAborted;
    if (!DecodeSelectedBlocks(fileName, &ArchiveData.ParserOutput, targetDir,
                              &ArchiveData.ft, Salamander, totalSize, &bAborted) &&
        !bAborted)
    {
        if (iErrorStr != -1)
            SalamanderGeneral->ShowMessageBox(LoadStr(iErrorStr), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        ret = FALSE;
    }
    if (bAborted)
        ret = FALSE;

    Salamander->CloseProgressDialog();
    return ret;
}

// ****************************************************************************
//
// CPluginDataInterface
//

void CPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    CALL_STACK_MESSAGE2("CPluginDataInterface::ReleasePluginData(, %d)", isDir);
    CArchiveData* pAD = (CArchiveData*)file.PluginData;
    pAD->RefCount--;
    if (!pAD->RefCount)
        delete pAD;
}

// ****************************************************************************

BOOL Error(int error, ...)
{
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE2("CPluginInterface::Error(%d, )", error);
    char buf[1024];
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        strcat(buf, " ");
        int l = (int)strlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);

    return FALSE;
}

void ShowOneTimeMessage(HWND HParent, int msg, BOOL* pChecked)
{
    MSGBOXEX_PARAMS params;

    memset(&params, 0, sizeof(params));
    params.HParent = HParent;
    params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT;
    params.Caption = LoadStr(IDS_PLUGINNAME);
    params.Text = LoadStr(msg);
    params.CheckBoxText = LoadStr(IDS_DONT_SHOW_AGAIN);
    params.CheckBoxValue = pChecked;
    SalamanderGeneral->SalMessageBoxEx(&params);
}

BOOL SafeWriteFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, char* fileName)
{
    while (!WriteFile(hFile, lpBuffer, nBytesToWrite, pnBytesWritten, NULL))
    {
        int lastErr = GetLastError();
        char error[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error, 1024, NULL);
        if (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, fileName, error,
                                           LoadStr(IDS_WRITEERROR)) != DIALOG_RETRY)
            return FALSE;
    }
    return TRUE;
}
