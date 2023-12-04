// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#include "precomp.h"
#include "nethood.h"
#include "nethoodfs.h"
#include "cache.h"
#include "nethoodfs2.h"
#include "nethooddata.h"
#include "globals.h"
#include "icons.h"
#include "nethood.rh"
#include "nethood.rh2"
#include "lang\lang.rh"

/// Cache of network.
extern CNethoodCache g_oNethoodCache;

/// Icon cache.
extern CNethoodIcons g_oIcons;

bool CNethoodFSInterface::s_bHideServersInRoot;

CNethoodFSInterface::CNethoodFSInterface()
{
    // Initial path is root path ("net:\")
    GetRootPath(m_szCurrentPath);

    m_pathNode = NULL;
    m_pPathNodeEventConsumer = NULL;
    m_state = FSStateNormal;
    m_bIgnoreForceRefresh = false;
    m_szRedirectPath[0] = TEXT('\0');
    m_szAccessiblePath[0] = TEXT('\0');
    m_dwEnumerationResult = ERROR_SUCCESS;
    m_bManualEntry = false;

    m_bShowThrobber = false;
    m_iThrobberID = -1;
}

CNethoodFSInterface::~CNethoodFSInterface()
{
    if (m_pPathNodeEventConsumer != NULL)
    {
        assert(m_pathNode != NULL);
        g_oNethoodCache.UnregisterConsumer(m_pathNode, m_pPathNodeEventConsumer);
        g_oNethoodCache.ReleaseNode(m_pathNode);
        delete m_pPathNodeEventConsumer;
        m_pPathNodeEventConsumer = NULL;
    }
}

BOOL WINAPI
CNethoodFSInterface::GetCurrentPath(__out_ecount(MAX_PATH) char* userPart)
{
    if (m_state == FSStateImmediateRefresh)
    {
        // The panel is about to be refreshed immediately.
        // That means that the cache node has been updated
        // and the path could changed.
        g_oNethoodCache.LockCache();
        g_oNethoodCache.GetFullPath(m_pathNode, m_szCurrentPath,
                                    COUNTOF(m_szCurrentPath));
        g_oNethoodCache.UnlockCache();
    }

    StringCchCopy(userPart, MAX_PATH, m_szCurrentPath);

    return TRUE;
}

BOOL WINAPI
CNethoodFSInterface::GetFullName(
    __in CFileData& file,
    __in int isDir,
    __out_ecount(bufSize) char* buf,
    __in int bufSize)
{
    StringCchCopy(buf, bufSize, m_szCurrentPath);

    if (isDir == 2)
    {
        // up-dir

        if (buf[0] == TEXT('\\') && buf[1] == TEXT('\\'))
        {
            // UNC path, go to root
            GetRootPath(buf);
        }
        else
        {
            SalamanderGeneral->CutDirectory(buf, NULL);
        }
    }
    else
    {
        if (_tcscmp(m_szCurrentPath, TEXT("\\")) == 0)
        {
            if (GetNodeTypeFromFileData(file) == CNethoodCacheNode::TypeServer)
            {
                StringCchCopy(buf, bufSize, TEXT("\\\\"));
                SalamanderGeneral->SalPathAppend(buf + 2, file.Name, bufSize - 2);
            }
            else
            {
                SalamanderGeneral->SalPathAppend(buf + 1, file.Name, bufSize - 1);
            }
        }
        else
        {
            SalamanderGeneral->SalPathAppend(buf, file.Name, bufSize);
        }
    }

    return TRUE;
}

BOOL WINAPI
CNethoodFSInterface::GetFullFSPath(
    HWND parent,
    const char* fsName,
    char* path,
    int pathSize,
    BOOL& success)
{
    // FIXME
    return FALSE;
}

BOOL WINAPI
CNethoodFSInterface::GetRootPath(__out_ecount(MAX_PATH) char* userPart)
{
    userPart[0] = TEXT('\\');
    userPart[1] = TEXT('\0');
    return TRUE;
}

BOOL WINAPI
CNethoodFSInterface::IsCurrentPath(
    int currentFSNameIndex,
    int fsNameIndex,
    const char* userPart)
{
    return (currentFSNameIndex == fsNameIndex) &&
           SalamanderGeneral->IsTheSamePath(m_szCurrentPath, userPart);
}

BOOL WINAPI
CNethoodFSInterface::IsOurPath(
    int currentFSNameIndex,
    int fsNameIndex,
    const char* userPart)
{
    // It's always our path.
    return TRUE;
}

BOOL WINAPI
CNethoodFSInterface::ChangePath(
    int currentFSNameIndex,
    char* fsName,
    int fsNameIndex,
    const char* userPart,
    char* cutFileName,
    BOOL* pathWasCut,
    BOOL forceRefresh,
    int mode)
{
    UINT uError;
    TCHAR szPath[MAX_PATH];
    TCHAR szCorrectedUserPart[MAX_PATH];
    const TCHAR* pSrc;
    TCHAR* pDst;

    //   1 (refresh path) - zkracuje cestu, je-li treba; nehlasit neexistenci cesty (bez hlaseni
    //                      zkratit), hlasit soubor misto cesty, nepristupnost cesty a dalsi chyby
    //   2 (volani ChangePanelPathToPluginFS, back/forward in history, etc.) - zkracuje cestu,
    //                      je-li treba; hlasit vsechny chyby cesty (soubor
    //                      misto cesty, neexistenci, nepristupnost a dalsi)
    //   3 (change-dir command) - zkracuje cestu jen jde-li o soubor nebo cestu nelze listovat
    //                      (ListCurrentPath pro ni vraci FALSE); nehlasit soubor misto cesty
    //                      (bez hlaseni zkratit a vratit jmeno souboru), hlasit vsechny ostatni
    //                      chyby cesty (neexistenci, nepristupnost a dalsi)
    // je-li 'mode' 1 nebo 2, vraci FALSE jen pokud na tomto FS zadna cesta neni pristupna
    // (napr. pri vypadku spojeni); je-li 'mode' 3, vraci FALSE pokud neni pristupna
    // pozadovana cesta nebo soubor (ke zkracovani cesty dojde jen v pripade, ze jde o soubor);
    // v pripade, ze je otevreni FS casove narocne (napr. pripojeni na FTP server) a 'mode'
    // je 3, je mozne upravit chovani jako u archivu - zkracovat cestu, je-li treba a vracet FALSE
    // jen pokud na FS neni zadna cesta pristupna, hlaseni chyb se nemeni

    CALL_STACK_MESSAGE4("CNethoodFSInterface::ChangePath(, , , %s, , , %d, %d)", userPart, forceRefresh, mode);

    // Replace forward slashes with backslashes.
    pSrc = userPart;
    pDst = szCorrectedUserPart;
    do
    {
        *pDst = (*pSrc == TEXT('/')) ? TEXT('\\') : *pSrc;
        ++pDst;
    } while (*pSrc++ != TEXT('\0'));
    userPart = szCorrectedUserPart;

    if (mode != 3 && (pathWasCut != NULL || cutFileName != NULL))
    {
        TRACE_E("Incorrect value of 'mode' in CPluginFSInterface::ChangePath().");
        mode = 3;
    }

    if (pathWasCut != NULL)
        *pathWasCut = FALSE;

    if (cutFileName != NULL)
        *cutFileName = TEXT('\0');

    szPath[0] = TEXT('\0');
    if (userPart[0] == TEXT('\0'))
    {
        GetRootPath(m_szCurrentPath);
        return TRUE;
    }
    else if (userPart[0] == TEXT('\\') && userPart[1] == TEXT('\\') && userPart[2] == TEXT('\0'))
    {
        GetRootPath(m_szCurrentPath);
        return TRUE;
    }
    else
    {
        if (userPart[0] != TEXT('\\'))
        {
            szPath[0] = TEXT('\\');
            szPath[1] = TEXT('\0');
        }
        StringCchCat(szPath, COUNTOF(szPath), userPart);
    }

    if (m_state == FSStateDisplayError)
    {
        assert(m_dwEnumerationResult != ERROR_SUCCESS);
        assert(m_szAccessiblePath[0] != TEXT('\0'));

        StringCchCopy(szPath, COUNTOF(szPath), m_szAccessiblePath);
        m_szAccessiblePath[0] = TEXT('\0');
        DisplayError(m_dwEnumerationResult);
        m_dwEnumerationResult = ERROR_SUCCESS;
        m_state = FSStateNormal;
    }
    else if (m_state == FSStateAsyncRedirect)
    {
        if (!PostRedirectPathToSalamander(m_szAccessiblePath))
        {
            // Asynchronous redirect to the accessible path found
            // from the enumeration thread failed. Try to shorten
            // the path. Note that we cannot use FindAccessiblePath()
            // because the cache node is NULL!

            StringCchCopy(szPath, COUNTOF(szPath), m_szAccessiblePath);
            TCHAR* pszLastSlash = _tcsrchr(szPath, TEXT('\\'));
            assert(pszLastSlash != NULL && pszLastSlash > szPath + 2);
            if (pszLastSlash != NULL)
            {
                *pszLastSlash = TEXT('\0');
            }

            m_state = FSStateNormal;
        }

        m_szAccessiblePath[0] = TEXT('\0');
    }
    else
    {
        uError = CUncPathParser::Validate(szPath);
        if (uError == ERROR_NETHOODCACHE_FULL_UNC_PATH)
        {
            if (!PostRedirectPathToSalamander(szPath))
            {
                return FALSE;
            }
        }
        else if (uError != NO_ERROR)
        {
            m_state = FSStateFindAccessible;
            if (mode == 2 || mode == 3)
            {
                DisplayError(uError);
                if (mode == 3)
                {
                    return FALSE;
                }
            }
        }
    }

    if (m_state == FSStateFindAccessible)
    {
        if (m_pathNode != NULL && !IsRootPath(m_szCurrentPath))
        {
            g_oNethoodCache.LockCache();
            g_oNethoodCache.FindAccessiblePath(m_pathNode, m_szCurrentPath, COUNTOF(m_szCurrentPath));
            g_oNethoodCache.UnlockCache();
        }
        else
        {
            GetRootPath(m_szCurrentPath);
        }

        if (pathWasCut != NULL)
        {
            *pathWasCut = TRUE;
        }
        if (cutFileName != NULL)
        {
            if (IsRootPath(m_szCurrentPath))
            {
                *cutFileName = TEXT('\0');
            }
            else
            {
                StringCchCopy(cutFileName, MAX_PATH, m_szCurrentPath);
            }
        }

        m_state = FSStateNormal;

        TRACE_I("Nethood: ChangePath: Found accessible path " << m_szCurrentPath);

        return TRUE;
    }

    if (!IsRootPath(szPath))
    {
        // Trim the backslash at the end of the path, but only
        // if it's not the root.
        SalamanderGeneral->SalPathRemoveBackslash(szPath);
    }

    StringCchCopy(m_szCurrentPath, COUNTOF(m_szCurrentPath), szPath);

    TRACE_I("Nethood: ChangePath: Path changed to " << m_szCurrentPath);

    if (mode != 1 && m_state == FSStateImmediateRefresh)
    {
        m_state = FSStateNormal;
    }

    if (m_state == FSStateSymLink)
    {
        m_state = FSStateNormal;
    }

    m_bManualEntry = (mode == 3);

    return TRUE;
}

bool CNethoodFSInterface::NethoodNodeToFileData(
    __in CNethoodCache::Node node,
    __out CFileData& file)
{
    const CNethoodCacheNode& nodeData = g_oNethoodCache.GetItemData(node);

    if (nodeData.GetType() == CNethoodCacheNode::TypeServer &&
        s_bHideServersInRoot &&
        IsRootPath(m_szCurrentPath))
    {
        return false;
    }

    memset(&file, 0, sizeof(CFileData));
    file.Name = SalamanderGeneral->DupStr(nodeData.GetDisplayName());
    file.NameLen = static_cast<unsigned>(_tcslen(file.Name));
    file.Ext = file.Name + file.NameLen;
    g_oNethoodCache.AddRefNode(node);
    file.PluginData = reinterpret_cast<DWORD_PTR>(node);
    assert(file.PluginData != 0);
    if (nodeData.IsContainer() && nodeData.GetType() != CNethoodCacheNode::TypeServer)
    {
        // We treat servers as "files" because of limited
        // Salamander's sorting options.
        file.Attr |= FILE_ATTRIBUTE_DIRECTORY;
    }

    if (nodeData.IsHidden())
    {
        file.Attr |= FILE_ATTRIBUTE_HIDDEN;
        file.Hidden = 1;
    }

#if 0
	if (nodeData.IsShortcut())
	{
		file.IsLink = 1;
	}
#endif

    return true;
}

CNethoodCache::Node CNethoodFSInterface::GetNodeFromFileData(
    __in const CFileData& file)
{
    return reinterpret_cast<CNethoodCache::Node>(static_cast<DWORD_PTR>(file.PluginData));
}

CNethoodCacheNode::Type CNethoodFSInterface::GetNodeTypeFromFileData(
    __in const CFileData& file)
{
    CNethoodCache::Node node;

    node = GetNodeFromFileData(file);
    if (node != NULL)
    {
        return g_oNethoodCache.GetItemData(node).GetType();
    }

    assert(0);
    return CNethoodCacheNode::TypeGeneric;
}

BOOL WINAPI
CNethoodFSInterface::ListCurrentPath(
    __in CSalamanderDirectoryAbstract* dir,
    __out CPluginDataInterfaceAbstract*& pluginData,
    __out int& iconsType,
    __in BOOL forceRefresh)
{
    m_bShowThrobber = false;

    UINT uError = NO_ERROR;
    bool bAddUpDir = false;
    CFileData fileData;
    CNethoodPluginDataInterface* pDataInterface;
    TCHAR szTargetPath[MAX_PATH];

    TRACE_I("Nethood: ListCurrentPath (path=" << m_szCurrentPath << ", state=" << m_state << ")");

    pDataInterface = new CNethoodPluginDataInterface();
    TRACE_I("dataInterface = " << pDataInterface);
    pluginData = pDataInterface;
    iconsType = pitFromPlugin;

    // This will invalidate the "file" extension and remove the Ext column.
    dir->SetValidData(VALID_DATA_ATTRIBUTES | VALID_DATA_HIDDEN | VALID_DATA_ISLINK);

    g_oNethoodCache.LockCache();

    if (m_bIgnoreForceRefresh)
    {
        forceRefresh = FALSE;
        m_bIgnoreForceRefresh = false;
    }

    if (m_state == FSStateRedirect)
    {
        m_state = FSStateNormal;

        pDataInterface->SetRedirectPath(m_szRedirectPath);
        m_szRedirectPath[0] = TEXT('\0');

        g_oNethoodCache.UnlockCache();

        return TRUE;
    }
    else if (m_state == FSStateImmediateRefresh)
    {
        uError = g_oNethoodCache.GetItemData(m_pathNode).GetLastEnumerationResult();
        if (uError == -1 || g_oNethoodCache.GetItemData(m_pathNode).GetStatus() == CNethoodCacheNode::StatusPending)
        {
            uError = ERROR_IO_PENDING;
        }
        m_state = FSStateNormal;
        if (uError == NO_ERROR || uError == ERROR_IO_PENDING)
        {
            g_oNethoodCache.ReleaseNode(m_pathNode);
        }
    }
    else
    {
        if (m_pPathNodeEventConsumer != NULL)
        {
            assert(m_pathNode != NULL);
            g_oNethoodCache.UnregisterConsumer(m_pathNode, m_pPathNodeEventConsumer);
            g_oNethoodCache.ReleaseNode(m_pathNode);
            delete m_pPathNodeEventConsumer;
            m_pPathNodeEventConsumer = NULL;
        }

        unsigned uFlags = 0;

        if (forceRefresh)
        {
            uFlags |= CNethoodCache::GPSF_FORCE;
        }

        if (m_bManualEntry)
        {
            uFlags |= CNethoodCache::GPSF_MANUAL;
        }

        m_pPathNodeEventConsumer = new CNethoodFSCacheConsumer(this);
        uError = g_oNethoodCache.GetPathStatus(
            m_szCurrentPath,
            m_pPathNodeEventConsumer,
            &m_pathNode,
            uFlags,
            szTargetPath,
            COUNTOF(szTargetPath));

        if (uError == ERROR_NETHOODCACHE_FULL_UNC_PATH)
        {
            delete m_pPathNodeEventConsumer;
            m_pPathNodeEventConsumer = NULL;

            if (!PostRedirectPathToSalamander(szTargetPath))
            {
                m_state = FSStateFindAccessible;
                g_oNethoodCache.UnlockCache();
                return FALSE;
            }

            if (m_state == FSStateRedirect)
            {
                m_state = FSStateNormal;

                pDataInterface->SetRedirectPath(szTargetPath);
                m_szRedirectPath[0] = TEXT('\0');
            }

            g_oNethoodCache.UnlockCache();

            return TRUE;
        }
        else if (uError == ERROR_NETHOODCACHE_SYMLINK)
        {
            delete m_pPathNodeEventConsumer;
            m_pPathNodeEventConsumer = NULL;

            m_state = FSStateSymLink;

            StringCchCopy(m_szCurrentPath, COUNTOF(m_szCurrentPath),
                          szTargetPath);

            g_oNethoodCache.UnlockCache();

            SalamanderGeneral->PostMenuExtCommand(
                MENUCMD_FOCUS_SHARE_BASE + g_iFocusSharePanel - PANEL_LEFT,
                TRUE);
            g_iFocusSharePanel = 0;

            delete pDataInterface;
            pluginData = NULL;

            return FALSE;
        }
    }

    if (uError == NO_ERROR || uError == ERROR_IO_PENDING)
    {
        CNethoodCache::Node nodeItem;

        g_oNethoodCache.AddRefNode(m_pathNode);

        g_oNethoodCache.GetFullPath(m_pathNode, m_szCurrentPath,
                                    COUNTOF(m_szCurrentPath));

        assert(m_pathNode != NULL);
        nodeItem = g_oNethoodCache.GetFirstItem(m_pathNode);
        while (nodeItem != NULL)
        {
            if (NethoodNodeToFileData(nodeItem, fileData))
            {
                if ((fileData.Attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    dir->AddDir(NULL, fileData, NULL);
                }
                else
                {
                    dir->AddFile(NULL, fileData, NULL);
                }
            }

            nodeItem = g_oNethoodCache.GetNextItem(nodeItem);
        }

        m_state = FSStateNormal;
        bAddUpDir = true;

        if (uError == ERROR_IO_PENDING ||
            g_oNethoodCache.GetItemData(m_pathNode).GetStatus() == CNethoodCacheNode::StatusPending)
        {
            StartThrobber();
        }
    }
    else if (uError == ERROR_NO_MORE_ITEMS)
    {
        bAddUpDir = true;
    }
    else
    {
        if (m_pathNode != NULL)
        {
            g_oNethoodCache.UnregisterConsumer(m_pathNode, m_pPathNodeEventConsumer);
        }
        delete m_pPathNodeEventConsumer;
        m_pPathNodeEventConsumer = NULL;
        m_state = FSStateFindAccessible;

        g_oNethoodCache.UnlockCache();

        DisplayError(uError);

        delete pDataInterface;
        pluginData = NULL;

        return FALSE;
    }

    g_oNethoodCache.UnlockCache();

    if (bAddUpDir && !IsRootPath(m_szCurrentPath))
    {
        memset(&fileData, 0, sizeof(CFileData));
        fileData.Name = SalamanderGeneral->DupStr("..");
        fileData.NameLen = 2;
        fileData.Ext = fileData.Name + fileData.NameLen;
        fileData.Attr |= FILE_ATTRIBUTE_DIRECTORY;
        dir->AddDir(NULL, fileData, NULL);
    }

    return TRUE;
}

BOOL WINAPI
CNethoodFSInterface::TryCloseOrDetach(
    BOOL forceClose,
    BOOL canDetach,
    BOOL& detach,
    int reason)
{
    detach = FALSE;
    return TRUE;
}

void WINAPI
CNethoodFSInterface::Event(
    __in int nEvent,
    __in DWORD dwParam)
{
    if (nEvent == FSE_PATHCHANGED)
    {
        if (m_bShowThrobber)
        {
            m_iThrobberID = SalamanderGeneral->StartThrobber(
                dwParam,
                SalamanderGeneral->LoadStr(
                    GetLangInstance(), IDS_REFRESHING),
                THROBBER_GRACE_PERIOD_IMMEDIATE);
        }
    }
}

void WINAPI
CNethoodFSInterface::ReleaseObject(
    __in HWND parent)
{
    UNREFERENCED_PARAMETER(parent);
}

DWORD WINAPI
CNethoodFSInterface::GetSupportedServices()
{
    return FS_SERVICE_CONTEXTMENU |
           FS_SERVICE_GETFSICON |
           FS_SERVICE_GETNEXTDIRLINEHOTPATH |
           FS_SERVICE_GETPATHFORMAINWNDTITLE;
}

BOOL WINAPI
CNethoodFSInterface::GetChangeDriveOrDisconnectItem(
    __in const char* fsName,
    __out_opt char*& title,
    __out_opt HICON& icon,
    __out BOOL& destroyIcon)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(title);
    UNREFERENCED_PARAMETER(icon);
    UNREFERENCED_PARAMETER(destroyIcon);

    return FALSE;
}

HICON WINAPI
CNethoodFSInterface::GetFSIcon(__out BOOL& destroyIcon)
{
    destroyIcon = TRUE;
    return g_oIcons.GetIcon(SALICONSIZE_16, CNethoodIcons::IconMain);
}

void WINAPI
CNethoodFSInterface::GetDropEffect(
    __in const char* srcFSPath,
    __in const char* tgtFSPath,
    __in DWORD allowedEffects,
    __in DWORD keyState,
    __out DWORD* dropEffect)
{
    UNREFERENCED_PARAMETER(srcFSPath);
    UNREFERENCED_PARAMETER(tgtFSPath);
    UNREFERENCED_PARAMETER(allowedEffects);
    UNREFERENCED_PARAMETER(keyState);
    UNREFERENCED_PARAMETER(dropEffect);
}

void WINAPI
CNethoodFSInterface::GetFSFreeSpace(
    __out CQuadWord* retValue)
{
    UNREFERENCED_PARAMETER(retValue);
}

BOOL WINAPI
CNethoodFSInterface::GetNextDirectoryLineHotPath(
    __in const char* text,
    __in int pathLen,
    __inout int& offset)
{
    PCTSTR psz;
    PCTSTR pszEnd;
    PCTSTR pszRoot;

    pszRoot = text;

    while (*pszRoot != TEXT('\0') && *pszRoot != TEXT(':'))
    {
        ++pszRoot;
    }

    if (*pszRoot == TEXT(':'))
    {
        ++pszRoot;

        // Skip root backslashes (net:\ or net:\\)
        while (*pszRoot == TEXT('\\'))
        {
            ++pszRoot;
        }
    }

    psz = text + offset;
    pszEnd = text + pathLen;

    if (psz >= pszEnd)
    {
        return FALSE;
    }

    if (psz < pszRoot)
    {
        offset = (int)(INT_PTR)(pszRoot - text);
    }
    else
    {
        if (*psz == TEXT('\\'))
        {
            ++psz;
        }

        while (psz < pszEnd && *psz != TEXT('\\'))
        {
            ++psz;
        }

        offset = (int)(INT_PTR)(psz - text);
    }

    return psz < pszEnd;
}

void WINAPI
CNethoodFSInterface::CompleteDirectoryLineHotPath(
    __inout_ecount(pathBufSize) char* path,
    __in int pathBufSize)
{
    // FIXME
}

BOOL WINAPI
CNethoodFSInterface::GetPathForMainWindowTitle(
    __in const char* fsName,
    __in int mode,
    __out_ecount(bufSize) char* buf,
    __in int bufSize)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(mode);
    UNREFERENCED_PARAMETER(buf);
    UNREFERENCED_PARAMETER(bufSize);

    // We return FALSE and Salamander will create the title for the main
    // window automatically through the GetNextDirectoryLineHotPath method.
    return FALSE;
}

void WINAPI
CNethoodFSInterface::ShowInfoDialog(
    __in const char* fsName,
    __in HWND parent)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(parent);
}

BOOL WINAPI
CNethoodFSInterface::ExecuteCommandLine(
    __in HWND parent,
    __inout_ecount(SALCMDLINE_MAXLEN + 1) char* command,
    __out int& selFrom,
    __out int& selTo)
{
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(command);
    UNREFERENCED_PARAMETER(selFrom);
    UNREFERENCED_PARAMETER(selTo);

    return FALSE;
}

BOOL WINAPI
CNethoodFSInterface::QuickRename(
    __in const char* fsName,
    __in int mode,
    __in HWND parent,
    __in CFileData& file,
    __in BOOL isDir,
    __out_ecount(MAX_PATH) char* newName,
    __out BOOL& cancel)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(mode);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(file);
    UNREFERENCED_PARAMETER(isDir);
    UNREFERENCED_PARAMETER(newName);
    UNREFERENCED_PARAMETER(cancel);

    return FALSE;
}

void WINAPI
CNethoodFSInterface::AcceptChangeOnPathNotification(
    __in const char* fsName,
    __in const char* path,
    __in BOOL includingSubdirs)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(path);
    UNREFERENCED_PARAMETER(includingSubdirs);
}

BOOL WINAPI
CNethoodFSInterface::CreateDir(
    __in const char* fsName,
    __in int mode,
    __in HWND parent,
    __inout_ecount(2 * MAX_PATH) char* newName,
    __out BOOL& cancel)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(mode);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(newName);
    UNREFERENCED_PARAMETER(cancel);

    return FALSE;
}

void WINAPI
CNethoodFSInterface::ViewFile(
    __in const char* fsName,
    __in HWND parent,
    __in CSalamanderForViewFileOnFSAbstract* salamander,
    __in CFileData& file)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(salamander);
    UNREFERENCED_PARAMETER(file);
}

BOOL WINAPI
CNethoodFSInterface::Delete(
    __in const char* fsName,
    __in int mode,
    __in HWND parent,
    __in int panel,
    __in int selectedFiles,
    __in int selectedDirs,
    __out BOOL& cancelOrError)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(mode);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(panel);
    UNREFERENCED_PARAMETER(selectedFiles);
    UNREFERENCED_PARAMETER(selectedDirs);
    UNREFERENCED_PARAMETER(cancelOrError);

    return FALSE;
}

BOOL WINAPI
CNethoodFSInterface::CopyOrMoveFromFS(
    __in BOOL copy,
    __in int mode,
    __in const char* fsName,
    __in HWND parent,
    __in int panel,
    __in int selectedFiles,
    __in int selectedDirs,
    __inout_ecount(2 * MAX_PATH) char* targetPath,
    __out BOOL& operationMask,
    __out BOOL& cancelOrHandlePath,
    __in HWND dropTarget)
{
    UNREFERENCED_PARAMETER(copy);
    UNREFERENCED_PARAMETER(mode);
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(panel);
    UNREFERENCED_PARAMETER(selectedFiles);
    UNREFERENCED_PARAMETER(selectedDirs);
    UNREFERENCED_PARAMETER(targetPath);
    UNREFERENCED_PARAMETER(operationMask);
    UNREFERENCED_PARAMETER(cancelOrHandlePath);
    UNREFERENCED_PARAMETER(dropTarget);

    return FALSE;
}

BOOL WINAPI
CNethoodFSInterface::CopyOrMoveFromDiskToFS(
    __in BOOL copy,
    __in int mode,
    __in const char* fsName,
    __in HWND parent,
    __in const char* sourcePath,
    __in SalEnumSelection2 next,
    __in void* nextParam,
    __in int sourceFiles,
    __in int sourceDirs,
    __inout_ecount(2 * MAX_PATH) char* targetPath,
    __out_opt BOOL* invalidPathOrCancel)
{
    UNREFERENCED_PARAMETER(copy);
    UNREFERENCED_PARAMETER(mode);
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(sourcePath);
    UNREFERENCED_PARAMETER(next);
    UNREFERENCED_PARAMETER(nextParam);
    UNREFERENCED_PARAMETER(sourceFiles);
    UNREFERENCED_PARAMETER(sourceDirs);
    UNREFERENCED_PARAMETER(targetPath);
    UNREFERENCED_PARAMETER(invalidPathOrCancel);

    return FALSE;
}

BOOL WINAPI
CNethoodFSInterface::ChangeAttributes(
    __in const char* fsName,
    __in HWND parent,
    __in int panel,
    __in int selectedFiles,
    __in int selectedDirs)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(panel);
    UNREFERENCED_PARAMETER(selectedFiles);
    UNREFERENCED_PARAMETER(selectedDirs);

    return FALSE;
}

void WINAPI
CNethoodFSInterface::ShowProperties(
    __in const char* fsName,
    __in HWND parent,
    __in int panel,
    __in int selectedFiles,
    __in int selectedDirs)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(parent);
    UNREFERENCED_PARAMETER(panel);
    UNREFERENCED_PARAMETER(selectedFiles);
    UNREFERENCED_PARAMETER(selectedDirs);
}

void WINAPI
CNethoodFSInterface::ContextMenu(
    __in const char* fsName,
    __in HWND parent,
    __in int menuX,
    __in int menuY,
    __in int type,
    __in int panel,
    __in int selectedFiles,
    __in int selectedDirs)
{
    TCHAR szPath[MAX_PATH];
    TCHAR szMappedRoot[4] = TEXT("\0:\\");
    bool bOk = false;

    if (type == fscmItemsInPanel)
    {
        // Open context menu for focused item or selected items in panel.

        if (!AreItemsSuitableForContextMenu(selectedFiles + selectedDirs > 0))
        {
            return;
        }

        if (IsRootForContextMenu())
        {
            SalamanderGeneral->OpenNetworkContextMenu(
                parent, panel, TRUE,
                menuX, menuY, TEXT("\\\\"),
                &szMappedRoot[0]);
        }
        else
        {
            g_oNethoodCache.LockCache();
            bOk = (m_pathNode != NULL) && g_oNethoodCache.GetUncPath(
                                              m_pathNode, szPath, COUNTOF(szPath));
            g_oNethoodCache.UnlockCache();
            if (bOk)
            {
                SalamanderGeneral->OpenNetworkContextMenu(
                    parent, panel, TRUE,
                    menuX, menuY, szPath,
                    &szMappedRoot[0]);
            }
        }
    }
    else
    {
        // Open context menu for incomplete UNC path in panel.

        assert(type == fscmPathInPanel || type == fscmPanel);

        if (IsRootPath(m_szCurrentPath))
        {
            SalamanderGeneral->OpenNetworkContextMenu(
                parent, panel, FALSE,
                menuX, menuY, TEXT("\\\\"),
                &szMappedRoot[0]);
        }
        else
        {
            g_oNethoodCache.LockCache();
            if (m_pathNode != NULL)
            {
                CNethoodCacheNode::Type nodeType;

                nodeType = g_oNethoodCache.GetItemData(m_pathNode).GetType();
                if (nodeType == CNethoodCacheNode::TypeServer ||
                    nodeType == CNethoodCacheNode::TypeShare ||
                    nodeType == CNethoodCacheNode::TypeTSCVolume)
                {
                    bOk = g_oNethoodCache.GetUncPath(
                        m_pathNode, szPath,
                        COUNTOF(szPath));
                }
            }
            g_oNethoodCache.UnlockCache();

            if (bOk)
            {
                SalamanderGeneral->OpenNetworkContextMenu(
                    parent, panel, FALSE,
                    menuX, menuY, szPath,
                    &szMappedRoot[0]);
            }
        }
    }

    if (szMappedRoot[0] != TEXT('\0'))
    {
        // New drive was mapped using Map Network Drive command,
        // let's open it in panel.

        // Store the path to redirect...
        PostRedirectPathToSalamander(szMappedRoot);
        // ...and refresh the panel that will perform the
        // actual redirection.
        SalamanderGeneral->PostRefreshPanelFS2(this);
    }
}

BOOL WINAPI
CNethoodFSInterface::OpenFindDialog(
    __in const char* fsName,
    __in int panel)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(panel);

    return FALSE;
}

void WINAPI
CNethoodFSInterface::OpenActiveFolder(
    __in const char* fsName,
    __in HWND parent)
{
    UNREFERENCED_PARAMETER(fsName);
    UNREFERENCED_PARAMETER(parent);
}

void WINAPI
CNethoodFSInterface::GetAllowedDropEffects(
    __in int mode,
    __in const char* tgtFSPath,
    __out DWORD* allowedEffects)
{
    UNREFERENCED_PARAMETER(mode);
    UNREFERENCED_PARAMETER(tgtFSPath);
    UNREFERENCED_PARAMETER(allowedEffects);
}

BOOL WINAPI
CNethoodFSInterface::HandleMenuMsg(
    __in UINT uMsg,
    __in WPARAM wParam,
    __in LPARAM lParam,
    __out LRESULT* plResult)
{
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    UNREFERENCED_PARAMETER(plResult);

    return FALSE;
}

BOOL WINAPI
CNethoodFSInterface::GetNoItemsInPanelText(
    __out char* textBuf,
    __in int textBufSize)
{
    bool bPending = false;

    g_oNethoodCache.LockCache();

    if (m_pathNode != NULL)
    {
        bPending = (g_oNethoodCache.GetItemData(m_pathNode).GetStatus() ==
                    CNethoodCacheNode::StatusPending);
    }

    g_oNethoodCache.UnlockCache();

    if (bPending)
    {
        // Enumeration pending...
        LoadString(GetLangInstance(), IDS_REFRESHING, textBuf, textBufSize);
        return TRUE;
    }
#if 0
	else
	{
		// No items in the panel.
		LoadString(GetLangInstance(), IDS_NETWORK_UNAVAILABLE, textBuf, textBufSize);
	}

	return TRUE;
#endif

    return FALSE;
}

void CNethoodFSInterface::NotifyPathUpdated(__in CNethoodCache::Node node)
{
    bool bRefresh = false;
    bool bFindAccessible = false;
    DWORD dwEnumerationResult = ERROR_SUCCESS;

    const CNethoodCacheNode& nodeData = g_oNethoodCache.GetItemData(node);

    TRACE_I("Node " << node << " updated");

    if (node == m_pathNode)
    {
        if (nodeData.GetStatus() == CNethoodCacheNode::StatusInvalid)
        {
            dwEnumerationResult = nodeData.GetLastEnumerationResult();

            if (dwEnumerationResult == ERROR_NETHOODCACHE_FULL_UNC_PATH)
            {
                g_oNethoodCache.GetUncPath(node, m_szAccessiblePath, COUNTOF(m_szAccessiblePath));
                m_state = FSStateAsyncRedirect;

                g_oNethoodCache.UnregisterConsumer(node, m_pPathNodeEventConsumer);
                g_oNethoodCache.ReleaseNode(node);
                delete m_pPathNodeEventConsumer;
                m_pPathNodeEventConsumer = NULL;
                m_pathNode = NULL;
            }
            else
            {
                bFindAccessible = true;
            }
        }
        else if (nodeData.GetStatus() == CNethoodCacheNode::StatusDead)
        {
            bFindAccessible = true;
            dwEnumerationResult = ERROR_BAD_NETPATH;
        }
        else if (nodeData.GetStatus() == CNethoodCacheNode::StatusOk)
        {
            m_state = FSStateImmediateRefresh;
            TRACE_I("Path " << m_szCurrentPath << " is valid. Will do immediate refresh.");
        }
        else if (nodeData.GetStatus() == CNethoodCacheNode::StatusPending)
        {
            if (nodeData.GetPreviousStatus() == CNethoodCacheNode::StatusStandby)
            {
                PostStartThrobber(node);
                return;
            }
            else
            {
                // Partial listing retrieved.
                m_state = FSStateImmediateRefresh;
            }
        }
        else
        {
            return;
        }

        if (bFindAccessible)
        {
            g_oNethoodCache.UnregisterConsumer(node, m_pPathNodeEventConsumer);
            g_oNethoodCache.ReleaseNode(node);
            delete m_pPathNodeEventConsumer;
            m_pPathNodeEventConsumer = NULL;
            g_oNethoodCache.FindAccessiblePath(m_pathNode, m_szAccessiblePath, COUNTOF(m_szAccessiblePath));
            m_pathNode = NULL;
            m_state = FSStateDisplayError;
            m_dwEnumerationResult = dwEnumerationResult;

            TRACE_I("Path was invalid, new accessible path is " << m_szAccessiblePath << ". Will do full refresh.");
        }

        bRefresh = true;
    }

    if (bRefresh)
    {
        m_bIgnoreForceRefresh = true;

        if (!SalamanderGeneral->PostRefreshPanelFS2(this))
        {
            // Petr: Give some time to main thread to attach this FS to panel.
            // FIXME: Milan: Not a good idea to sleep while holding the cache lock!
            Sleep(200);
            SalamanderGeneral->PostRefreshPanelFS2(this);
        }
    }
}

bool CNethoodFSInterface::IsRootPath(__in PCTSTR pszPath)
{
    return (pszPath[0] == TEXT('\\')) && (pszPath[1] == TEXT('\0'));
}

void CNethoodFSInterface::DisplayError(DWORD dwError)
{
    TCHAR szErrorMessage[512];
    void* pAllocatedErrorDescription;

    if (m_bShowThrobber)
    {
        SalamanderGeneral->StopThrobber(m_iThrobberID);
        m_bShowThrobber = false;
    }

    if (dwError == ERROR_CANCELLED)
    {
        // User cancelled something (the logon dialog most probably).
        // Do dot inform her/him about something she is already aware of.
        return;
    }

    if (FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dwError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<PTSTR>(&pAllocatedErrorDescription),
            0, NULL) > 0)
    {
        StringCchPrintf(szErrorMessage, COUNTOF(szErrorMessage),
                        TEXT("(%lu) "), dwError);
        StringCchCat(szErrorMessage, COUNTOF(szErrorMessage),
                     static_cast<PTSTR>(pAllocatedErrorDescription));
        LocalFree(pAllocatedErrorDescription);
    }
    else
    {
        StringCchPrintf(szErrorMessage, COUNTOF(szErrorMessage),
                        TEXT("(%lu)"), dwError);
    }

    SalamanderGeneral->ShowMessageBox(szErrorMessage, NULL, MSGBOX_WARNING);
}

bool CNethoodFSInterface::PostRedirectPathToSalamander(
    __in PCTSTR pszPath)
{
    assert(m_state != FSStateRedirect);
    assert(pszPath != m_szRedirectPath); // We REALLY want to compare the pointers!

    if (!SalamanderGeneral->SalCheckAndRestorePath(
            SalamanderGeneral->GetMainWindowHWND(),
            pszPath,
            TRUE))
    {
        return false;
    }

    // TODO: Exclude current path from the history.

    StringCchCopy(m_szRedirectPath, COUNTOF(m_szRedirectPath), pszPath);
    m_state = FSStateRedirect;

    return true;
}

void CNethoodFSInterface::StartThrobber()
{
    TRACE_I("Starting throbber.");

    m_bShowThrobber = true;
}

void CNethoodFSInterface::StopThrobber()
{
    TRACE_I("Stopping throbber.");

    m_bShowThrobber = false;
}

void CNethoodFSInterface::PostStartThrobber(__in CNethoodCache::Node node)
{
    g_oNethoodCache.AddRefNode(node);
    g_adwPostedThrobberQueue[g_iPostedThrobberQueue] = reinterpret_cast<DWORD_PTR>(node);
    if (++g_iPostedThrobberQueue >= POSTED_THROBBER_QUEUE_LEN)
    {
        g_iPostedThrobberQueue = 0;
    }
    SalamanderGeneral->PostMenuExtCommand(MENUCMD_START_THROBBER, FALSE);
}

bool CNethoodFSInterface::IsRootForContextMenu()
{
    bool bRoot;

    bRoot = IsRootPath(m_szCurrentPath);
    if (!bRoot)
    {
        g_oNethoodCache.LockCache();
        if (m_pathNode != NULL)
        {
            CNethoodCacheNode::Type nodeType;
            nodeType = g_oNethoodCache.GetItemData(m_pathNode).GetType();
            bRoot = (nodeType == CNethoodCacheNode::TypeDomain) ||
                    (nodeType == CNethoodCacheNode::TypeGroup);
        }
        g_oNethoodCache.UnlockCache();
    }

    return bRoot;
}

bool CNethoodFSInterface::AreItemsSuitableForContextMenu(
    __in bool bSelected)
{
    const CFileData* file;
    CNethoodCacheNode::Type nodeType;
    bool bOk = false;

    g_oNethoodCache.LockCache();
    if (bSelected)
    {
        int iItem = 0;

        while ((file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &iItem, NULL)) != NULL)
        {
            nodeType = GetNodeTypeFromFileData(*file);
            bOk = (nodeType == CNethoodCacheNode::TypeServer ||
                   nodeType == CNethoodCacheNode::TypeShare ||
                   nodeType == CNethoodCacheNode::TypeTSCVolume);
            if (!bOk)
            {
                break;
            }
        }
    }
    else
    {
        file = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, NULL);
        if (file != NULL)
        {
            nodeType = GetNodeTypeFromFileData(*file);
            bOk = (nodeType == CNethoodCacheNode::TypeServer ||
                   nodeType == CNethoodCacheNode::TypeShare ||
                   nodeType == CNethoodCacheNode::TypeTSCVolume);
        }
    }
    g_oNethoodCache.UnlockCache();

    return bOk;
}

void CNethoodFSInterface::ConfigurationChanged()
{
    SalamanderGeneral->PostRefreshPanelFS2(this);
}

//------------------------------------------------------------------------------
//

void CNethoodFSCacheConsumer::OnCacheNodeUpdated(__in CNethoodCache::Node node)
{
#if defined(_DEBUG) && defined(SONIC_DEBUGGING)
    MessageBeep(MB_ICONINFORMATION);
#endif
    m_pChief->NotifyPathUpdated(node);
}
