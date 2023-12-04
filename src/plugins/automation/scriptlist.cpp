// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	scriptlist.cpp
	List of the scripts stored in the repositories.
*/

#include "precomp.h"
#include "scriptlist.h"
#include "scriptsite.h"
#include "aututils.h"
#include "engassoc.h"
#include "knownengines.h"
#include "automationplug.h"
#include "shim.h"
#include "abortpalette.h"
#include "abortmodal.h"
#include "lang\lang.rh"

extern HINSTANCE g_hInstance;
extern HINSTANCE g_hLangInst;
extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern CAutomationPluginInterface g_oAutomationPlugin;

CScriptInfo::CScriptInfo(
    PCTSTR pszFileName,
    CScriptContainer* pContainer)
{
    PCTSTR pszNameStart, pszNameEnd;

    StringCchCopy(m_szFileName, _countof(m_szFileName), pszFileName);

    pszNameStart = PathFindFileName(pszFileName);
    pszNameEnd = PathFindExtension(pszNameStart);
    StringCchCopyN(m_szDisplayName, _countof(m_szDisplayName), pszNameStart, pszNameEnd - pszNameStart);

    m_clsidEngine = CLSID_NULL;

    m_pScript = NULL;
    m_pSite = NULL;
    m_pHardError = NULL;

    m_cExecuted = 0;

    m_pExecInfo = NULL;

    m_bAbortPending = false;
    m_hAbortEvent = NULL;
    m_hwndAbortTarget = NULL;

    m_pNext = NULL;
    m_pNextHash = NULL;
    m_pContainer = pContainer;
    m_nId = 0;

    m_bDirty = false;

    m_pShim = NULL;
}

CScriptInfo::~CScriptInfo()
{
    if (m_pScript != NULL)
    {
        m_pScript->Release();
    }
}

PCTSTR CScriptInfo::GetFileExt() const
{
    return PathFindExtension(m_szFileName);
}

bool CScriptInfo::Execute(__inout EXECUTION_INFO& info)
{
    if (!EnsureEngineAssociation())
    {
        return false;
    }

    //return ExecuteInSeparateThread(&info);
    return ExecuteWorker(&info);
}

bool CScriptInfo::EnsureEngineAssociation()
{
    if (IsEqualGUID(m_clsidEngine, GUID_NULL))
    {
        if (!g_oScriptAssociations.FindEngineByExt(
                PathFindExtension(m_szFileName),
                &m_clsidEngine))
        {
            // TODO: report a message
            return false;
        }
    }

    return true;
}

bool CScriptInfo::CreateEngine(EXECUTION_INFO* info)
{
    HRESULT hr;
    IActiveScriptParse* pParse;

    if (m_pScript)
    {
        return true;
    }

    hr = CoCreateInstance(m_clsidEngine, NULL, CLSCTX_INPROC_SERVER,
                          __uuidof(m_pScript), (void**)&m_pScript);
    if (SUCCEEDED(hr))
    {
        _ASSERTE(m_pSite == NULL);
        m_pSite = new CScriptSite(this);
        _ASSERTE(m_pSite);

        m_pShim = CScriptEngineShim::Create(this);

        if (info->bEnableDebugger)
        {
            InitializeDebugger(&info->dbgInfo);
        }

        m_hAbortEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));

        // BUG 103: ActiveRuby calls OnEnterScript very early
        // during parsing.
        m_pSite->SetExecutionInfo(info);

        hr = m_pScript->SetScriptSite(m_pSite);

        if (SUCCEEDED(hr))
        {
            hr = m_pScript->QueryInterface(&pParse);
            if (SUCCEEDED(hr))
            {
                hr = pParse->InitNew();

                if (SUCCEEDED(hr))
                {
                    hr = m_pScript->AddNamedItem(
                        L"Salamander",
                        SCRIPTITEM_ISVISIBLE); // These are the flags cscript.exe uses.

                    if (SUCCEEDED(hr))
                    {
                        hr = LoadScript(pParse, info);
                    }
                }

                pParse->Release();
            }
        }
    }

    if (FAILED(hr))
    {
        delete m_pShim;

        if (m_pScript != NULL)
        {
            m_pScript->Release();
            m_pScript = NULL;
        }

        if (m_pSite != NULL)
        {
            m_pSite->Release();
            m_pSite = NULL;
        }

        HANDLES(CloseHandle(m_hAbortEvent));
        m_hAbortEvent = NULL;

        TRACE_E("CreateEngine failed with error " << std::hex << hr);

        return false;
    }

    return true;
}

HRESULT CScriptInfo::LoadScript(IActiveScriptParse* pParse, EXECUTION_INFO* info)
{
    HRESULT hr;
    LPOLESTR pszCode;
    EXCEPINFO ei;
    ULONG cch;
    TCHAR szExpanded[MAX_PATH];

    if (!g_oAutomationPlugin.ExpandPath(m_szFileName, szExpanded,
                                        _countof(szExpanded)))
    {
        return HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND);
    }

    hr = LoadOleStringFromFile(szExpanded, pszCode, &cch);
    if (FAILED(hr))
    {
        TCHAR szMessage[256];
        TCHAR szError[192];

        FormatErrorText(hr, szError, _countof(szError));
        StringCchPrintf(szMessage, _countof(szMessage),
                        SalamanderGeneral->LoadStr(g_hLangInst, IDS_LOADERRFMT),
                        PathFindFileName(m_szFileName), szError);

        SalamanderGeneral->ShowMessageBox(
            szMessage,
            SalamanderGeneral->LoadStr(g_hLangInst, IDS_PLUGINNAME),
            MSGBOX_ERROR);

        return hr;
    }

    if (SUCCEEDED(hr) && info->dbgInfo.pDbgDocHelper != NULL)
    {
        HRESULT hrdbg;
        IDebugDocumentHelper* pddh = info->dbgInfo.pDbgDocHelper;

        IDebugDocumentHost* phost;
        hrdbg = m_pSite->QueryInterface(__uuidof(phost), (void**)&phost);
        if (SUCCEEDED(hrdbg))
        {
            hrdbg = pddh->SetDebugDocumentHost(phost);
            phost->Release();
        }

        hrdbg = pddh->AddUnicodeText(pszCode);
        if (SUCCEEDED(hrdbg))
        {
            hrdbg = pddh->DefineScriptBlock(0, cch, m_pScript, FALSE, &info->dbgInfo.dwSourceContext);
        }
    }

    hr = pParse->ParseScriptText(pszCode, NULL, NULL, NULL, 0, 0, SCRIPTTEXT_HOSTMANAGESSOURCE, NULL, &ei);

    FreeOleString(pszCode);

    if (FAILED(hr) && hr != SCRIPT_E_REPORTED)
    {
        DisplayException(ei);
    }

    return hr;
}

HRESULT CScriptInfo::LoadOleStringFromFile(PCTSTR pszFileName, __out LPOLESTR& s, __out_opt ULONG* cch)
{
    HANDLE hFile, hMapping;
    DWORD cbSize;
    char* pszCodeA;
    HRESULT hr;
    int cchRequired, cchConverted;

    hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    cbSize = GetFileSize(hFile, NULL);
    if (cbSize == 0)
    {
        CloseHandle(hFile);
        s = (LPOLESTR)malloc(sizeof(WCHAR));
        if (s == NULL)
        {
            return E_OUTOFMEMORY;
        }

        *s = L'\0';
        return S_OK;
    }

    hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    hr = HRESULT_FROM_WIN32(GetLastError());
    CloseHandle(hFile);
    if (hMapping == NULL)
    {
        return hr;
    }

    pszCodeA = (char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    hr = HRESULT_FROM_WIN32(GetLastError());
    CloseHandle(hMapping);
    if (pszCodeA == NULL)
    {
        return hr;
    }

    cchRequired = MultiByteToWideChar(CP_ACP, 0, pszCodeA, cbSize, NULL, 0);
    if (cchRequired <= 0)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        UnmapViewOfFile(pszCodeA);
        return hr;
    }

    s = (LPOLESTR)malloc((cchRequired + 1) * sizeof(WCHAR));
    if (s == NULL)
    {
        UnmapViewOfFile(pszCodeA);
        return E_OUTOFMEMORY;
    }

    cchConverted = MultiByteToWideChar(CP_ACP, 0, pszCodeA, cbSize, s, cchRequired);
    hr = HRESULT_FROM_WIN32(GetLastError());
    UnmapViewOfFile(pszCodeA);
    if (cchConverted <= 0)
    {
        free(s);
        return hr;
    }

    // nul terminate
    s[cchConverted] = L'\0';

    if (cch != NULL)
    {
        *cch = cchConverted;
    }

    return S_OK;
}

bool CScriptInfo::ExecuteWorker(EXECUTION_INFO* info)
{
    HRESULT hr = S_OK;

    CALL_STACK_MESSAGE2("CScriptInfo::ExecuteWorker() (file name = \"%s\")", m_szFileName);

    info->bDeselect = false;

    m_pExecInfo = info;
    m_bAbortPending = false;

    _ASSERTE(m_pHardError == NULL);

    m_bSiteErrorDisplayed = false;
    if (!CreateEngine(info))
    {
        // If the error occured during parsing and was already displayed
        // through the site's OnScriptError event, don't display it here.
        // Otherwise display generic error message here.
        if (!m_bSiteErrorDisplayed)
        {
            SalamanderGeneral->SalMessageBox(
                SalamanderGeneral->GetMsgBoxParent(),
                SalamanderGeneral->LoadStr(g_hLangInst, IDS_ENGINECREATEFAIL),
                SalamanderGeneral->LoadStr(g_hLangInst, IDS_PLUGINNAME),
                MB_OK | MB_ICONERROR);
        }

        return false;
    }

    // workaround for WshShell.SendKeys to work properly (by john):
    // Salamander is not responding properly on SendKeys (http://msdn.microsoft.com/en-us/library/8c6yea83%28VS.85%29.aspx)
    // when Ctrl/Shift/Alt is still pressed (for example when script was started using Ctrl+Shift+Z hot key).
    // Miranda global hot key (Ctrl+Shift+A) is triggered on (salamander ->) Ctrl+Shift+Z -> (script started ->) SendKeys("a").
    // Attempt to release pressed Ctrl/Shift/Alt using SetKeyboardState() doesn't work.
    // Note: it seems that SendKeys is using API ::SendInput() beacause it doesn't work too (Miranda is activated).
    // Fortunately, following hack works pretty well.
    ResetKeyboardState();

    m_pShim->BeginExecution();

    // Run the script.
    // We used to have only SetScriptState(SCRIPTSTATE_CONNECTED) here, but tracing
    // the cscript.exe revealed, that it calls SCRIPTSTATE_INITIALIZED immediately
    // followed by SCRIPTSTATE_STARTED. We changed the logic here to match the one
    // of cscript.exe more closely. This was done primarily because of RScript22
    // won't execute a script without SCRIPTSTATE_STARTED. Hopefully, this won't
    // break other engines (JScript and VBScript seems fine).
    hr = m_pScript->SetScriptState(SCRIPTSTATE_INITIALIZED);
    if (SUCCEEDED(hr))
    {
        hr = m_pScript->SetScriptState(SCRIPTSTATE_STARTED);
    }

    // ActivePython returns SCRIPT_E_REPORTED if there was parse error.
    // If debugging is enabled, the SCRIPT_E_PROPAGATE may be returned
    // if debugger is detached while an exception is being debugged.
    _ASSERTE(SUCCEEDED(hr) || hr == SCRIPT_E_REPORTED || hr == SCRIPT_E_PROPAGATE || m_pHardError);

    m_pSite->SetExecutionInfo(NULL);
    UninitializeDebugger(&info->dbgInfo);
    m_pShim->EndExecution();

    // cscript.exe doesn't call SetScriptState at all at the end of the execution. But it can
    // afford not calling it because the process ends afterwards. We must deal with buggy
    // engines and do need to uninitialize it.
    hr = m_pScript->SetScriptState(SCRIPTSTATE_INITIALIZED);
    _ASSERTE(SUCCEEDED(hr));

    if (m_pHardError)
    {
        DisplayException(m_pHardError, m_pShim);
        m_pHardError->Release();
        m_pHardError = NULL;
    }

    hr = m_pShim->ReleaseEngine(m_pScript);
    _ASSERTE(SUCCEEDED(hr));

    m_pScript = NULL;

    m_pSite->Release();
    m_pSite = NULL;

    m_pExecInfo = NULL;

    HANDLES(CloseHandle(m_hAbortEvent));
    m_hAbortEvent = NULL;

    delete m_pShim;
    m_pShim = NULL;

    return SUCCEEDED(hr);
}

bool CScriptInfo::ExecuteInSeparateThread(EXECUTION_INFO* info)
{
    HANDLE hThread;

    info->pInstance = this;
    info->bAsyncResult = false;

    hThread = CreateThread(NULL, 0, ExecuteEntryProc, info, 0, NULL);
    if (hThread != NULL)
    {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    return info->bAsyncResult;
}

DWORD WINAPI CScriptInfo::ExecuteEntryProc(void* arg)
{
    HRESULT hr;
    CScriptInfo* that;
    EXECUTION_INFO* info;

    info = reinterpret_cast<EXECUTION_INFO*>(arg);
    that = reinterpret_cast<CScriptInfo*>(info->pInstance);

    hr = CoInitializeEx(NULL, /*COINIT_MULTITHREADED*/ COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        info->bAsyncResult = that->ExecuteWorker(info);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        CoUninitialize();
    }

    return 0;
}

void CScriptInfo::InitializeDebugger(DEBUG_INFO* dbgInfo)
{
    HRESULT hr;

    hr = CoCreateInstance(
        CLSID_ProcessDebugManager,
        NULL,
        CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        __uuidof(IProcessDebugManager),
        (void**)&dbgInfo->pProcDbgMgr);
    if (FAILED(hr))
        return;

    hr = dbgInfo->pProcDbgMgr->GetDefaultApplication(&dbgInfo->pDbgApp);
    if (FAILED(hr))
        return;

    hr = dbgInfo->pProcDbgMgr->CreateDebugDocumentHelper(NULL, &dbgInfo->pDbgDocHelper);
    if (FAILED(hr))
        return;

    OLECHAR szUrl[2 * MAX_PATH];
    DWORD cchUrl = _countof(szUrl);
    A2OLE sFileNameW(m_szFileName);
    if (FAILED(UrlCreateFromPathW(A2OLE(sFileNameW), szUrl, &cchUrl, 0)))
    {
        StringCchCopyW(szUrl, _countof(szUrl), sFileNameW);
    }
    hr = dbgInfo->pDbgDocHelper->Init(dbgInfo->pDbgApp,
                                      A2OLE(GetDisplayName()), szUrl, TEXT_DOC_ATTR_READONLY);
    if (FAILED(hr))
        return;

    hr = dbgInfo->pDbgDocHelper->Attach(NULL);
}

void CScriptInfo::UninitializeDebugger(DEBUG_INFO* dbgInfo)
{
    if (dbgInfo->pDbgDocHelper)
    {
        dbgInfo->pDbgDocHelper->Detach();
        dbgInfo->pDbgDocHelper->Release();
    }

    if (dbgInfo->pDbgApp)
    {
        dbgInfo->pDbgApp->Release();
    }

    if (dbgInfo->pProcDbgMgr)
    {
        dbgInfo->pProcDbgMgr->Release();
    }

    memset(dbgInfo, 0, sizeof(DEBUG_INFO));
}

HRESULT CScriptInfo::AbortScript()
{
    HRESULT hr;

    _ASSERTE(m_pScript);
    _ASSERTE(m_pShim);

    hr = m_pShim->InterruptScript(m_pScript);

    if (!m_bAbortPending)
    {
        m_bAbortPending = true;

        // Cooperatively abort native execution (e.g. quit modal
        // message boxes, exit GUI loops, cancel sleeps, break
        // enumerators etc.).

        // Set the abort event, so the kernel waits can exit (e.g.
        // Salamander.Sleep()).
        _ASSERTE(m_hAbortEvent != NULL);
        SetEvent(m_hAbortEvent);

        HWND hwndAbortTarget = *(volatile HWND*)&m_hwndAbortTarget;
        if (hwndAbortTarget != NULL)
        {
            PostMessage(hwndAbortTarget, WM_SAL_ABORTMODAL, 0, 0);
        }
    }

    return hr;
}

void CScriptInfo::ScriptEnter()
{
    HWND hwndPalette;

    _ASSERTE(m_pExecInfo);
    _ASSERTE(m_pExecInfo->pAbortPalette == NULL);

    if (m_pExecInfo->pAbortPalette == NULL)
    {
        m_pExecInfo->pAbortPalette = new CScriptAbortPalette(this);
        hwndPalette = m_pExecInfo->pAbortPalette->GetHwnd();
        _ASSERTE(IsWindow(hwndPalette));

        // Make the main window inaccessible, since the script may display
        // modeless window and the user can unload the whole plugin from
        // the main window in the mean time.
        // NOTE: 'hwndPalette' mechanism is not used because WS_EX_TOPMOST with process tree
        // checking using WindowBelongsToProcessID() look like better solution for now.
        // For example unpack script is starting several command prompt windows
        // so WS_EX_TOPMOST toolbar is better accessible.
        SalamanderGeneral->LockMainWindow(TRUE, NULL, SalamanderGeneral->LoadStr(g_hLangInst, IDS_MAINWINDOWLOCKED));
    }
}

void CScriptInfo::ScriptLeave()
{
    if (m_pExecInfo != NULL)
    {
        // Enable the main window again.
        SalamanderGeneral->LockMainWindow(FALSE, NULL, NULL);

        delete m_pExecInfo->pAbortPalette;
        m_pExecInfo->pAbortPalette = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

CScriptContainer::CScriptContainer()
{
    m_pParent = NULL;
    m_pSibling = NULL;
    m_pChild = NULL;
    m_pScripts = NULL;
    m_szPath[0] = _T('\0');
    m_pszName = NULL;
}

CScriptContainer::CScriptContainer(
    CScriptContainer* pParent,
    PCTSTR pszPath,
    bool bFullPath)
{
    m_pParent = pParent;
    m_pSibling = NULL;
    m_pChild = NULL;
    m_pScripts = NULL;

    if (bFullPath)
    {
        StringCchCopy(m_szPath, _countof(m_szPath), pszPath);
    }
    else
    {
        _ASSERTE(pParent);
        StringCchCopy(m_szPath, _countof(m_szPath), pParent->m_szPath);
        SalamanderGeneral->SalPathAppend(m_szPath, pszPath, _countof(m_szPath));
    }

    SalamanderGeneral->SalPathRemoveBackslash(m_szPath);
    m_pszName = PathFindFileName(m_szPath);
}

CScriptContainer::~CScriptContainer()
{
}

CScriptContainer* CScriptContainer::FirstChild(
    PCTSTR pszPath,
    bool bFullPath)
{
    CScriptContainer* pIter;
    TCHAR szFullPath[MAX_PATH];

    if (m_pChild == NULL)
    {
        return NULL;
    }

    pIter = m_pChild;

    if (bFullPath)
    {
        StringCchCopy(szFullPath, _countof(szFullPath), pszPath);
    }
    else
    {
        StringCchCopy(szFullPath, _countof(szFullPath), m_szPath);
        SalamanderGeneral->SalPathAppend(szFullPath, pszPath, _countof(szFullPath));
    }

    while (pIter)
    {
        if (_tcsicmp(pIter->m_szPath, szFullPath) == 0)
        {
            return pIter;
        }

        pIter = pIter->m_pSibling;
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

CScriptLookup::CScriptLookup()
{
    m_pRootContainer = NULL;
    memset(m_apHashBins, 0, sizeof(m_apHashBins));
    m_cScriptsTotal = 0;
    m_bModified = false;
    m_dwLastRefreshTime = 0;
}

CScriptLookup::~CScriptLookup()
{
    if (m_pRootContainer)
    {
        CascadeDeleteContainer(m_pRootContainer);
    }

    CScriptInfo *pIter, *pTmp;
    for (int iBin = 0; iBin < _countof(m_apHashBins); iBin++)
    {
        pIter = m_apHashBins[iBin];
        while (pIter)
        {
            pTmp = pIter->m_pNextHash;
            delete pIter;
            pIter = pTmp;
        }
    }
}

bool CScriptLookup::Load(HKEY hKey, CSalamanderRegistryAbstract* registry)
{
    int cDirs;
    int iDir;

    if (m_pRootContainer == NULL)
    {
        m_pRootContainer = new CScriptContainer();
    }

    if (hKey != INVALID_HANDLE_VALUE)
    {
        // not a refresh
        m_bModified = false;
    }

    cDirs = g_oAutomationPlugin.GetScriptDirectoryCount();
    for (iDir = 0; iDir < cDirs; iDir++)
    {
        CScriptContainer* pContainer;
        bool bExisting;

        pContainer = m_pRootContainer->FirstChild(
            g_oAutomationPlugin.GetScriptDirectoryRaw(iDir), true);
        bExisting = (pContainer != NULL);

        if (!bExisting)
        {
            pContainer = new CScriptContainer(m_pRootContainer,
                                              g_oAutomationPlugin.GetScriptDirectoryRaw(iDir), true);
        }

        if (FillContainer(pContainer, hKey, registry) > 0)
        {
            if (!bExisting)
            {
                LinkContainer(pContainer, m_pRootContainer);
            }
        }
        else if (!bExisting)
        {
            delete pContainer;
        }
    }

    m_dwLastRefreshTime = GetTickCount();

    return true;
}

bool CScriptLookup::Save(HKEY hKey, CSalamanderRegistryAbstract* registry)
{
    _ASSERTE(hKey != NULL);
    _ASSERTE(registry != NULL);

    if (!registry->ClearKey(hKey))
    {
        return false;
    }

    for (int iBin = 0; iBin < _countof(m_apHashBins); iBin++)
    {
        if (m_apHashBins[iBin] != NULL)
        {
            SaveBin(m_apHashBins[iBin], hKey, registry);
        }
    }

    m_bModified = false;

    return true;
}

bool CScriptLookup::SaveBin(
    CScriptInfo* pFirst,
    HKEY hKey,
    CSalamanderRegistryAbstract* registry)
{
    TCHAR szName[8];
    HKEY hkSub = NULL;
    UINT nPrevHash = 0;
    UINT nHash;
    CScriptInfo* pIter;

    for (pIter = pFirst; pIter; pIter = pIter->m_pNextHash)
    {
        nHash = HashFromId(pIter->m_nId);
        if (nHash != nPrevHash)
        {
            if (hkSub != NULL)
            {
                registry->CloseKey(hkSub);
                hkSub = NULL;
            }

            StringCchPrintf(szName, _countof(szName), "%06X", nHash);
            if (!registry->CreateKey(hKey, szName, hkSub))
            {
                return false;
            }

            nPrevHash = nHash;
        }

        StringCchPrintf(szName, _countof(szName), "%02X", UniquierFromId(pIter->m_nId));
        registry->SetValue(hkSub, szName, REG_SZ, pIter->GetFileName(), -1);
    }

    if (hkSub != NULL)
    {
        registry->CloseKey(hkSub);
    }

    return true;
}

CScriptInfo* CScriptLookup::LookupScript(int nId)
{
    int iBin;
    CScriptInfo* pScript;

    iBin = HashFromId(nId) % _countof(m_apHashBins);
    pScript = m_apHashBins[iBin];
    while (pScript)
    {
        if (pScript->m_nId == nId)
        {
            return pScript;
        }
        else if (pScript->m_nId > nId)
        {
            break;
        }

        pScript = pScript->m_pNextHash;
    }

    return NULL;
}

int CScriptLookup::FillContainer(
    CScriptContainer* pContainer,
    HKEY hKey,
    CSalamanderRegistryAbstract* registry)
{
    HANDLE hFind;
    TCHAR szPattern[MAX_PATH];
    WIN32_FIND_DATA fd;
    int cScripts = 0;

    g_oAutomationPlugin.ExpandPath(pContainer->GetPath(), szPattern, _countof(szPattern));
    SalamanderGeneral->SalPathAppend(szPattern, _T("*"), _countof(szPattern));

    hFind = FindFirstFile(szPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
        {
            continue;
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (fd.cFileName[0] != _T('.')) // exclude . and .. as well as unix style hidden dirs
            {
                int cSubScripts = 0;
                CScriptContainer* pSubContainer;
                bool bExisting;

                pSubContainer = pContainer->FirstChild(fd.cFileName, false);
                bExisting = (pSubContainer != NULL);
                if (!bExisting)
                {
                    pSubContainer = new CScriptContainer(pContainer, fd.cFileName, false);
                }

                cSubScripts = FillContainer(pSubContainer, hKey, registry);
                if (cSubScripts > 0)
                {
                    if (!bExisting)
                    {
                        LinkContainer(pSubContainer, pContainer);
                    }
                    cScripts += cSubScripts;
                }
                else if (!bExisting)
                {
                    delete pSubContainer;
                }
            }
        }
        else
        {
            PTSTR pszExt = PathFindExtension(fd.cFileName);
            if (pszExt && *pszExt && g_oScriptAssociations.FindEngineByExt(pszExt))
            {
                TCHAR szFullPath[MAX_PATH];
                StringCchCopy(szFullPath, _countof(szFullPath), pContainer->GetPath());
                SalamanderGeneral->SalPathAppend(szFullPath, fd.cFileName, _countof(szFullPath));
                if (AddScriptFromFile(pContainer, szFullPath, hKey, registry))
                {
                    ++cScripts;
                    ++m_cScriptsTotal;
                }
            }
        }
    } while (FindNextFile(hFind, &fd));

    FindClose(hFind);

    return (cScripts > 0);
}

void CScriptLookup::LinkContainer(CScriptContainer* pContainer, CScriptContainer* pParent)
{
    _ASSERTE(pParent != NULL);

    if (pParent->m_pChild == NULL)
    {
        pParent->m_pChild = pContainer;
    }
    else
    {
        CScriptContainer* pIter = pParent->m_pChild;
        CScriptContainer* pPrev = NULL;

        while (pIter && _tcsicmp(pContainer->m_pszName, pIter->m_pszName) >= 0)
        {
            pPrev = pIter;
            pIter = pIter->m_pSibling;
        }

        if (pPrev)
        {
            pContainer->m_pSibling = pPrev->m_pSibling;
            pPrev->m_pSibling = pContainer;
        }
        else
        {
            pContainer->m_pSibling = pParent->m_pChild;
            pParent->m_pChild = pContainer;
        }
    }
}

void CScriptLookup::UnlinkContainer(CScriptContainer* pContainer)
{
    _ASSERTE(pContainer != NULL);
    _ASSERTE(pContainer->m_pChild == NULL);
    _ASSERTE(pContainer->m_pScripts == NULL);

    CScriptContainer* pParent = pContainer->m_pParent;
    CScriptContainer *pIter, *pPrev = NULL;

    pIter = pParent->m_pChild;
    while (pIter != pContainer)
    {
        pPrev = pIter;
        pIter = pIter->m_pSibling;
    }

    if (pPrev == NULL)
    {
        pParent->m_pChild = pContainer->m_pSibling;
    }
    else
    {
        pPrev->m_pSibling = pContainer->m_pSibling;
    }

    pContainer->m_pSibling = NULL;
    pContainer->m_pParent = NULL;
}

void CScriptLookup::LinkScript(
    CScriptInfo* pScript)
{
    CScriptContainer* pParent = pScript->m_pContainer;

    if (pParent->m_pScripts == NULL)
    {
        pParent->m_pScripts = pScript;
    }
    else
    {
        CScriptInfo* pIter = pParent->m_pScripts;
        CScriptInfo* pPrev = NULL;

        while (pIter && _tcsicmp(pScript->GetDisplayName(), pIter->GetDisplayName()) >= 0)
        {
            pPrev = pIter;
            pIter = pIter->m_pNext;
        }

        if (pPrev)
        {
            pScript->m_pNext = pPrev->m_pNext;
            pPrev->m_pNext = pScript;
        }
        else
        {
            pScript->m_pNext = pParent->m_pScripts;
            pParent->m_pScripts = pScript;
        }
    }
}

void CScriptLookup::UnlinkScript(
    CScriptInfo* pScript)
{
    CScriptContainer* pParent = pScript->m_pContainer;
    CScriptInfo *pIter, *pPrev = NULL;

    pIter = pParent->m_pScripts;
    while (pIter != pScript)
    {
        pPrev = pIter;
        pIter = pIter->m_pNext;
    }

    if (pPrev == NULL)
    {
        pParent->m_pScripts = pScript->m_pNext;
    }
    else
    {
        pPrev->m_pNext = pScript->m_pNext;
    }

    pScript->m_pNext = NULL;
    pScript->m_pContainer = NULL;
}

void CScriptLookup::LinkScriptHash(CScriptInfo* pScript)
{
    int iBin;

    iBin = HashFromId(pScript->m_nId) % _countof(m_apHashBins);
    if (m_apHashBins[iBin] == NULL)
    {
        m_apHashBins[iBin] = pScript;
    }
    else
    {
        CScriptInfo* pIter = m_apHashBins[iBin];
        CScriptInfo* pPrev = NULL;

        while (pIter && pIter->m_nId <= pScript->m_nId)
        {
            pPrev = pIter;
            pIter = pIter->m_pNextHash;
        }

        if (pPrev)
        {
            pScript->m_pNextHash = pPrev->m_pNextHash;
            pPrev->m_pNextHash = pScript;
        }
        else
        {
            pScript->m_pNextHash = m_apHashBins[iBin];
            m_apHashBins[iBin] = pScript;
        }
    }
}

CScriptInfo* CScriptLookup::AddScriptFromFile(
    CScriptContainer* pContainer,
    PCTSTR pszFullPath,
    HKEY hKey,
    CSalamanderRegistryAbstract* registry)
{
    UINT nHash;
    int nUniquier;
    CScriptInfo* pScript;

    nHash = HashPath(pszFullPath);
    if (nHash == 0)
    {
        _ASSERTE(0);
        return NULL;
    }

    pScript = LookupScriptByPath(nHash, pszFullPath);
    if (pScript)
    {
        // script already in the hash map,
        // clear the dirty flag
        pScript->ResetDirty();
        return pScript;
    }

    nUniquier = GetUniquier(nHash, pszFullPath, hKey, registry);
    if (nUniquier < 0)
    {
        return NULL;
    }

    pScript = new CScriptInfo(pszFullPath, pContainer);
    pScript->m_nId = MakeId(nHash, nUniquier);
    LinkScript(pScript);
    LinkScriptHash(pScript);

    return pScript;
}

int CScriptLookup::GetUniquier(
    UINT nHash,
    __in_z PCTSTR pszPath,
    HKEY hKey,
    CSalamanderRegistryAbstract* registry)
{
    int iBin;
    CScriptInfo* pScript;
    int nUniquier;
    CUniquierBitmap bitmap;

    if (hKey != NULL && hKey != INVALID_HANDLE_VALUE)
    {
        HKEY hkSub;
        TCHAR szName[8];
        StringCchPrintf(szName, _countof(szName), "%06X", HashFromId(nHash));
        if (!registry->OpenKey(hKey, szName, hkSub))
        {
            // the hash key does not even exist in the registry,
            // return 1st available uniquier
            m_bModified = true;
            return 0;
        }

        LONG res = NO_ERROR;
        DWORD dwIndex = 0;
        DWORD cchName;
        DWORD dwType;
        TCHAR szPathRead[MAX_PATH];
        DWORD cbData;

        for (; res == NO_ERROR; dwIndex++)
        {
            cchName = _countof(szName);
            cbData = sizeof(szPathRead);
            res = RegEnumValue(hkSub, dwIndex, szName, &cchName,
                               NULL, &dwType, (LPBYTE)szPathRead, &cbData);
            if (res == NO_ERROR && dwType == REG_SZ)
            {
                nUniquier = _tcstol(szName, NULL, 16);

                // mark this uniquier as used in the free bitmap
                bitmap.MarkBusy(nUniquier);

                if (_tcsicmp(pszPath, szPathRead) == 0)
                {
                    // we found exact uniquier for this script
                    registry->CloseKey(hkSub);
                    return nUniquier;
                }
            }
        }

        registry->CloseKey(hkSub);

        // the script path was not found in the registry,
        // look if we can allocate a new uniquier for this path
        m_bModified = true;
        return bitmap.Alloc();
    }

    m_bModified = true;
    iBin = nHash % _countof(m_apHashBins);
    pScript = m_apHashBins[iBin];
    CScriptInfo* pFirstScript = NULL;
    while (pScript)
    {
        if (HashFromId(pScript->m_nId) == HashFromId(nHash))
        {
            pFirstScript = pScript;
            break;
        }
        else if (HashFromId(pScript->m_nId) > HashFromId(nHash))
        {
            break;
        }

        pScript = pScript->m_pNextHash;
    }

    if (pFirstScript != NULL)
    {
        while (pFirstScript && HashFromId(pFirstScript->m_nId) == HashFromId(nHash))
        {
            bitmap.MarkBusy(UniquierFromId(pFirstScript->m_nId));
            pFirstScript = pFirstScript->m_pNextHash;
        }

        return bitmap.Alloc();
    }

    // no uniquier for this hash yet, start counting at zero
    return 0;
}

UINT CScriptLookup::HashPath(__in_z PCTSTR pszPath)
{
    UINT nHash;
    TCHAR szCanonicalPath[MAX_PATH];

    StringCchCopy(szCanonicalPath, _countof(szCanonicalPath), pszPath);
    CharLower(szCanonicalPath);
    nHash = HashString(szCanonicalPath);
    if (nHash == 0)
    {
        nHash = ~0u;
    }

    nHash <<= HASH_SHIFT;
    nHash &= HASH_MASK;

    return nHash;
}

void CScriptLookup::CascadeDeleteContainer(
    CScriptContainer* pContainer)
{
    if (pContainer)
    {
        CascadeDeleteContainer(pContainer->m_pChild);

        while (pContainer->m_pSibling)
        {
            CScriptContainer* pTmp;
            pTmp = pContainer->m_pSibling->m_pSibling;
            CascadeDeleteContainer(pContainer->m_pSibling->m_pChild);
            delete pContainer->m_pSibling;
            pContainer->m_pSibling = pTmp;
        }

        delete pContainer;
    }
}

bool CScriptLookup::Refresh(bool bForce)
{
    bool res;

    if (GetTickCount() - m_dwLastRefreshTime < 5000 && !bForce)
    {
        // ignore refresh request if it comes too early
        // since the last one (this prevents excessive
        // disk activity when user repeatedly runs a script)
        return true;
    }

    // assume all existing scripts dirty
    MarkAllScriptsDirty();

    res = Load((HKEY)INVALID_HANDLE_VALUE, NULL);

    // remove scripts that remained dirty after the refresh
    RemoveDirtyScripts();

    // remove containers that remained empty
    RemoveEmptyContainers(m_pRootContainer);

    return res;
}

void CScriptLookup::MarkAllScriptsDirty()
{
    for (int iBin = 0; iBin < _countof(m_apHashBins); iBin++)
    {
        for (CScriptInfo* pIter = m_apHashBins[iBin];
             pIter != NULL;
             pIter = pIter->m_pNextHash)
        {
            pIter->SetDirty();
        }
    }
}

void CScriptLookup::RemoveDirtyScripts()
{
    for (int iBin = 0; iBin < _countof(m_apHashBins); iBin++)
    {
    restart:
        CScriptInfo* pIter = m_apHashBins[iBin];
        CScriptInfo* pPrev = NULL;

        while (pIter != NULL)
        {
            CScriptInfo* pNext = pIter->m_pNextHash;

            if (pIter->IsDirty())
            {
                // unlink from hash map
                if (pPrev)
                {
                    pPrev->m_pNextHash = pNext;
                }
                else
                {
                    m_apHashBins[iBin] = pNext;
                }

                // unlink from container
                UnlinkScript(pIter);
                m_bModified = true;

                delete pIter;

                goto restart;
            }

            pPrev = pIter;
            pIter = pNext;
        }
    }
}

void CScriptLookup::RemoveEmptyContainers(CScriptContainer* pContainer)
{
    _ASSERTE(pContainer != NULL);

    if (pContainer->m_pChild != NULL)
    {
        RemoveEmptyContainers(pContainer->m_pChild);
    }

    while (pContainer != NULL)
    {
        CScriptContainer* pNext = pContainer->m_pSibling;

        if (pContainer->m_pChild == NULL && pContainer->m_pScripts == NULL)
        {
            if (pContainer != m_pRootContainer)
            {
                UnlinkContainer(pContainer);
                delete pContainer;
            }
        }

        pContainer = pNext;
    }
}

CScriptInfo* CScriptLookup::LookupScriptByPath(
    UINT nHash,
    PCTSTR pszFullPath)
{
    _ASSERTE(nHash != 0);

    int iBin = HashFromId(nHash) % _countof(m_apHashBins);
    for (CScriptInfo* pIter = m_apHashBins[iBin];
         pIter != NULL;
         pIter = pIter->m_pNextHash)
    {
        if (_tcsicmp(pIter->GetFileName(), pszFullPath) == 0)
        {
            return pIter;
        }
    }

    return NULL;
}
