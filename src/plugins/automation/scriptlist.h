// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	scriptlist.h
	List of the scripts stored in the repositories.
*/

#pragma once

class CScriptInfo
{
public:
    struct DEBUG_INFO
    {
        IProcessDebugManager* pProcDbgMgr;
        IDebugDocumentHelper* pDbgDocHelper;
        IDebugApplication* pDbgApp;
#ifdef _WIN64
        DWORDLONG dwSourceContext;
#else
        DWORD dwSourceContext;
#endif
        DWORD dwAppCookie;
    };

    struct EXECUTION_INFO
    {
        /// Deselect selection.
        bool bDeselect;

        /// Interface for displaying progress dialogs.
        CSalamanderForOperationsAbstract* pOperation;

        /// Enable script debugger.
        bool bEnableDebugger;

        // Internal members, do not touch.
        void* pInstance;
        bool bAsyncResult;
        DEBUG_INFO dbgInfo;

        class CScriptAbortPalette* pAbortPalette;

        /// Constructor.
        EXECUTION_INFO()
        {
            memset(this, 0, sizeof(*this));
        }
    };

private:
    TCHAR m_szFileName[MAX_PATH];
    TCHAR m_szDisplayName[64];
    CLSID m_clsidEngine;
    IActiveScript* m_pScript;
    class CScriptSite* m_pSite;
    EXECUTION_INFO* m_pExecInfo;
    bool m_bAbortPending; ///< AbortScript() was called.
    CScriptInfo* m_pNext;
    CScriptInfo* m_pNextHash;
    int m_nId;
    bool m_bDirty;
    class CScriptContainer* m_pContainer;
    IActiveScriptError* m_pHardError;
    bool m_bSiteErrorDisplayed; ///< Message box shown in site's OnScriptError.
    class CScriptEngineShim* m_pShim;
    HANDLE m_hAbortEvent; ///< Manually reset event signaled when the user requested abort.
    HWND m_hwndAbortTarget;

    // statistics stuff
    LONG m_cExecuted;

    bool EnsureEngineAssociation();

    bool CreateEngine(EXECUTION_INFO* info);

    HRESULT LoadScript(IActiveScriptParse* pParse, EXECUTION_INFO* info);

    static void FreeOleString(LPOLESTR s)
    {
        free(s);
    }

    static HRESULT LoadOleStringFromFile(PCTSTR pszFileName, __out LPOLESTR& s, __out_opt ULONG* cch);

    bool ExecuteWorker(__inout EXECUTION_INFO* info);
    bool ExecuteInSeparateThread(__inout EXECUTION_INFO* info);
    static DWORD WINAPI ExecuteEntryProc(void* arg);

    void InitializeDebugger(DEBUG_INFO* dbgInfo);
    void UninitializeDebugger(DEBUG_INFO* dbgInfo);

    void ScriptEnter();
    void ScriptLeave();

    friend class CScriptLookup;
    friend class CScriptEngineShim;

public:
    CScriptInfo(
        PCTSTR pszFileName,
        CScriptContainer* pContainer);

    ~CScriptInfo();

    PCTSTR GetFileName() const
    {
        return m_szFileName;
    }

    PCTSTR GetFileExt() const;

    PCTSTR GetDisplayName() const
    {
        return m_szDisplayName;
    }

    REFCLSID GetEngineCLSID() const
    {
        _ASSERTE(m_clsidEngine != CLSID_NULL);
        return m_clsidEngine;
    }

    class CScriptEngineShim* GetShim()
    {
        return m_pShim;
    }

    HRESULT GetScriptEngine(__out IActiveScript** pScript)
    {
        _ASSERTE(pScript != NULL);
        _ASSERTE(m_pScript != NULL);
        *pScript = m_pScript;
        m_pScript->AddRef();
        return S_OK;
    }

    bool Execute(__inout EXECUTION_INFO& info);

    DEBUG_INFO* GetDebugInfo()
    {
        _ASSERTE(m_pExecInfo);
        return &m_pExecInfo->dbgInfo;
    }

    /// Aborts script execution.
    HRESULT AbortScript();

    int GetId() const
    {
        return m_nId;
    }

    const CScriptInfo* Next() const
    {
        return m_pNext;
    }

    void SetDirty()
    {
        m_bDirty = true;
    }

    void ResetDirty()
    {
        m_bDirty = false;
    }

    bool IsDirty() const
    {
        return m_bDirty;
    }

    void SetHardError(IActiveScriptError* pError)
    {
        _ASSERTE(pError != NULL);
        if (m_pHardError != NULL)
        {
            m_pHardError->Release();
            m_pHardError = NULL;
        }
        m_pHardError = pError;
        m_pHardError->AddRef();
    }

    void SetSiteError(IActiveScriptError* pError)
    {
        _ASSERTE(pError != NULL);
        UNREFERENCED_PARAMETER(pError);
        m_bSiteErrorDisplayed = true;
    }

    HANDLE GetAbortEvent() const
    {
        _ASSERTE(m_hAbortEvent != NULL);
        return m_hAbortEvent;
    }

    bool IsAbortPending() const
    {
        return m_bAbortPending;
    }

    HWND SetAbortTargetHwnd(HWND hwndAbortTarget)
    {
        return (HWND)InterlockedExchangePointer((void**)&m_hwndAbortTarget, (void*)hwndAbortTarget);
    }
};

class CScriptContainer
{
private:
    CScriptContainer* m_pSibling;
    CScriptContainer* m_pChild;
    CScriptContainer* m_pParent;
    CScriptInfo* m_pScripts;
    TCHAR m_szPath[MAX_PATH];
    PTSTR m_pszName;

    friend class CScriptLookup;

public:
    CScriptContainer();
    CScriptContainer(CScriptContainer* pParent, PCTSTR pszPath, bool bFullPath);
    ~CScriptContainer();

    void Clear();
    bool Fill(__in_z PCTSTR pszPath);

    PCTSTR GetPath() const
    {
        return m_szPath;
    }

    PCTSTR GetName() const
    {
        return m_pszName;
    }

    const CScriptContainer* FirstChild() const
    {
        return m_pChild;
    }

    const CScriptContainer* NextSibling() const
    {
        return m_pSibling;
    }

    const CScriptInfo* FirstScript() const
    {
        return m_pScripts;
    }

    CScriptContainer* FirstChild(PCTSTR pszPath, bool bFullPath);
};

class CScriptLookup
{
private:
    CScriptContainer* m_pRootContainer;
    CScriptInfo* m_apHashBins[37];
    int m_cScriptsTotal;
    bool m_bModified;
    DWORD m_dwLastRefreshTime;

    enum
    {
        HASH_SHIFT = 8,
        HASH_MASK = 0x7FFFFF00,
        UNIQUIER_MASK = 0xFF,
        UNIQUIER_MAX = 255,
    };

    UINT HashPath(__in_z PCTSTR pszPath);

    int FillContainer(
        CScriptContainer* pContainer,
        HKEY hKey,
        CSalamanderRegistryAbstract* registry);

    /// Inserts the subcontainer to the container.
    void LinkContainer(CScriptContainer* pContainer, CScriptContainer* pParent);

    void UnlinkContainer(CScriptContainer* pContainer);

    /// Inserts the script to the container.
    void LinkScript(CScriptInfo* pScript);

    /// Removed the script from the container.
    void UnlinkScript(CScriptInfo* pScript);

    /// Adds the script into the hash map.
    void LinkScriptHash(CScriptInfo* pScript);

    CScriptInfo* AddScriptFromFile(
        CScriptContainer* pContainer,
        PCTSTR pszFullPath,
        HKEY hKey,
        CSalamanderRegistryAbstract* registry);

    int GetUniquier(
        UINT nHash,
        __in_z PCTSTR pszPath,
        HKEY hKey,
        CSalamanderRegistryAbstract* registry);

    int MakeId(UINT nHash, int nUniquier)
    {
        _ASSERTE((nHash & ~HASH_MASK) == 0);
        _ASSERTE((nUniquier & ~UNIQUIER_MASK) == 0);
        return nHash + nUniquier;
    }

    UINT HashFromId(int nId)
    {
        return (((UINT)nId) & HASH_MASK) >> HASH_SHIFT;
    }

    int UniquierFromId(int nId)
    {
        return (nId & UNIQUIER_MASK);
    }

    bool SaveBin(
        CScriptInfo* pFirst,
        HKEY hKey,
        CSalamanderRegistryAbstract* registry);

    void CascadeDeleteContainer(CScriptContainer* pContainer);

    void MarkAllScriptsDirty();
    void RemoveDirtyScripts();
    void RemoveEmptyContainers(CScriptContainer* pContainer);

    CScriptInfo* LookupScriptByPath(UINT nHash, PCTSTR pszFullPath);

    /// This data structure holds bitmap of free uniquiers for
    /// single hash bucket.
    class CUniquierBitmap
    {
    private:
        enum
        {
            BITS_PER_ULONG = sizeof(ULONG) * 8,
        };

        /// This is the actual bitmap. If the bit is set, the
        /// uniquier is free.
        ULONG m_bitmap[(UNIQUIER_MAX + 1) / BITS_PER_ULONG];

    public:
        CUniquierBitmap()
        {
            memset(m_bitmap, 0xFF, sizeof(m_bitmap));
        }

        /// Marks the uniquier as allocated.
        void MarkBusy(int nUniquier)
        {
            _ASSERTE(nUniquier >= 0 || nUniquier <= UNIQUIER_MAX);
            m_bitmap[nUniquier / BITS_PER_ULONG] &= ~(1 << (nUniquier % BITS_PER_ULONG));
        }

        /// Finds first free uniquier.
        int Alloc()
        {
            ULONG ibit;
            for (int iword = 0; iword < _countof(m_bitmap); iword++)
            {
                if (BitScanForward(&ibit, m_bitmap[iword]))
                {
                    return (iword * BITS_PER_ULONG) + ibit;
                }
            }

            // uniquiers exhausted
            return -1;
        }
    };

public:
    CScriptLookup();
    ~CScriptLookup();

    bool Load(HKEY hKey, CSalamanderRegistryAbstract* registry);
    bool Save(HKEY hKey, CSalamanderRegistryAbstract* registry);

    bool Refresh(bool bForce = false);

    CScriptInfo* LookupScript(int nId);

    int GetCount() const
    {
        return m_cScriptsTotal;
    }

    const CScriptContainer* GetRootContainer() const
    {
        return m_pRootContainer;
    }

    bool IsModified() const
    {
        return m_bModified;
    }
};
