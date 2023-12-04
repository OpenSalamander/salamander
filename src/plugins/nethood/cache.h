// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...

	Network cache.
*/

#pragma once

#include "tree.h"

class CNethoodCache;
class CNethoodCacheEventConsumer;

struct CNethoodCacheListEntry
{
    CNethoodCacheListEntry* pNext;
    CNethoodCacheListEntry* pPrev;

    CNethoodCacheListEntry()
    {
        pNext = pPrev = this;
    }

    static void InsertAtHead(
        __in CNethoodCacheListEntry* pHead,
        __in CNethoodCacheListEntry* pEntry)
    {
        CNethoodCacheListEntry* pOldNext;
        CNethoodCacheListEntry* pOldHead;

        pOldHead = pHead;
        pOldNext = pOldHead->pNext;
        pEntry->pNext = pOldNext;
        pEntry->pPrev = pOldHead;
        pOldNext->pPrev = pEntry;
        pOldHead->pNext = pEntry;
    }

    static void InsertAtTail(
        __in CNethoodCacheListEntry* pHead,
        __in CNethoodCacheListEntry* pEntry)
    {
        CNethoodCacheListEntry* pOldPrev;
        CNethoodCacheListEntry* pOldHead;

        pOldHead = pHead;
        pOldPrev = pOldHead->pPrev;
        pEntry->pNext = pOldHead;
        pEntry->pPrev = pOldPrev;
        pOldPrev->pNext = pEntry;
        pOldHead->pPrev = pEntry;
    }

    static void Remove(
        __in CNethoodCacheListEntry* pEntry)
    {
        CNethoodCacheListEntry* pOldNext;
        CNethoodCacheListEntry* pOldPrev;

        pOldNext = pEntry->pNext;
        pOldPrev = pEntry->pPrev;
        pOldPrev->pNext = pOldNext;
        pOldNext->pPrev = pOldPrev;

        pEntry->pNext = pEntry->pPrev = pEntry;
    }

    static bool IsEmpty(__in CNethoodCacheListEntry* pHead)
    {
        return (pHead->pNext == pHead);
    }

    static bool Search(
        __in CNethoodCacheListEntry* pHead,
        __in CNethoodCacheListEntry* pEntry)
    {
        CNethoodCacheListEntry* pIterator = pHead->pNext;

        while (pIterator != pHead)
        {
            if (pIterator == pEntry)
            {
                return true;
            }

            pIterator = pIterator->pNext;
        }

        return false;
    }
};

/// Implements one node of the network cache tree.
class CNethoodCacheNode
{
public:
    /// Node status values.
    enum Status
    {
        /// Node not enumerated yet.
        StatusUnknown,

        /// Node currently being enumerated.
        StatusPending,

        /// Node enumerated and valid.
        StatusOk,

        /// Node and all possible children are invalid.
        StatusInvalid,

        /// Node valid, waiting for re-enumeration.
        StatusStandby,

        /// Node should not be visible.
        StatusInvisible,

        /// This node is being removed.
        StatusDead,
    };

    enum Hint
    {
        HintNone,

        HintRoot,

        HintServer,

        HintTSCVolume,

        HintTSCFolder,
    };

    enum Type
    {
        TypeGeneric = RESOURCEDISPLAYTYPE_GENERIC,
        TypeDomain = RESOURCEDISPLAYTYPE_DOMAIN,
        TypeServer = RESOURCEDISPLAYTYPE_SERVER,
        TypeShare = RESOURCEDISPLAYTYPE_SHARE,
        TypeFile = RESOURCEDISPLAYTYPE_FILE,
        TypeGroup = RESOURCEDISPLAYTYPE_GROUP,
        TypeNetwork = RESOURCEDISPLAYTYPE_NETWORK,
        TypeRoot = RESOURCEDISPLAYTYPE_ROOT,
        TypeShareAdmin = RESOURCEDISPLAYTYPE_SHAREADMIN,
        TypeDirectory = RESOURCEDISPLAYTYPE_DIRECTORY,
        TypeTree = RESOURCEDISPLAYTYPE_TREE,
        TypeNdsContainer = RESOURCEDISPLAYTYPE_NDSCONTAINER,

        /// Represents drive attached through the Terminal Services
        /// client.
        TypeTSCVolume = 0x10000,

        /// Virtual folder where the drives attached through the
        /// Terminal Services are located.
        TypeTSCFolder = 0x20000
    };

    enum RESOURCETYPE
    {
        RESOURCETYPE_SHORTCUT = 0x08000000
    };

protected:
    /// Status of this node.
    Status m_status;

    /// Previous status of this node.
    Status m_revertStatus;

    /// Error code of the last enumeration.
    DWORD m_dwLastEnumerationResult;

    /// Head of the consumer list.
    CNethoodCacheListEntry m_consumers;

    /// Entry for the stand-by list.
    CNethoodCacheListEntry m_standbyListEntry;

    /// Number of consumers interested in the node change events.
    unsigned m_cConsumers;

    /// Number of consumers used internally by the cache itself.
    unsigned m_cInternalConsumers;

    /// Node name.
    PTSTR m_pszName;

    /// Display name.
    PTSTR m_pszDisplayName;

    /// Node comment.
    PTSTR m_pszComment;

    /// Network provider.
    PTSTR m_pszProvider;

    /// Manually formed display name.
    PTSTR m_pszExplicitDisplayName;

    /// Hint for this node.
    Hint m_hint;

    /// Parent cache.
    CNethoodCache* m_pCache;

    /// The NETRESOURCE structure identifying this node.
    NETRESOURCE m_sNetResource;

    /// If true, the m_sNetResource member is valid.
    bool m_bNetResourceValid;

    /// Type of the node. Value from the CNethoodCacheNode::Type
    /// enumeration.
    Type m_type;

    /// Iterator for this node.
    void* m_myIterator;

    /// Timeout between starting the re-enumeration of the node.
    /// The value is in milliseconds.
    UINT m_uStandbyTimeout;

    /// Timestamp when the node was inserted in the stand-by list.
    /// If the node is not in the stand-by list then this value is zero.
    UINT m_uStandbyTimestamp;

    /// If true, the node will survive even if not included
    /// in the enumeration result.
    bool m_bImmortal;

    /// If true, the node should be made immortal after successful
    /// enumeration.
    bool m_bImmortalSticky;

    /// Number of external outstanding references to this node.
    LONG m_cRef;

    /// True if this is hidden/system share (C$ etc.).
    bool m_bHidden;

    /// True if this node is network shortcut indeed.
    bool m_bShortcut;

    /// Private copy constructor to prevent use.
    CNethoodCacheNode(const CNethoodCacheNode&);

    /// Private assignment operator to prevent use.
    CNethoodCacheNode& operator=(const CNethoodCacheNode&);

    friend class CNethoodCache;
    friend class CNethoodCacheManagementThread;

public:
    /// Constructor.
    CNethoodCacheNode();

    /// Destructor.
    ~CNethoodCacheNode();

    void Create(
        __in CNethoodCache* pCache,
        __in void* iterator);

    /// Adds a new consumer to the list.
    /// \return The return value is the total number or consumers
    ///         attached to this node after the addition.
    unsigned AddConsumer(__in CNethoodCacheEventConsumer* pConsumer);

    /// Removes the consumer from the list.
    /// \return The return value is the total number of consumers
    ///         attached to this node after the removal.
    unsigned RemoveConsumer(__in CNethoodCacheEventConsumer* pConsumer);

    void AddInternalConsumer(__in CNethoodCacheEventConsumer* pConsumer);

    void RemoveInternalConsumer(__in CNethoodCacheEventConsumer* pConsumer);

    void SetName(__in PCTSTR pszName, __in int nLen = -1);

    PCTSTR GetName() const
    {
        return m_pszName;
    }

    PCTSTR GetDisplayName() const
    {
        return m_pszDisplayName;
    }

    void SetNetResource(__in const NETRESOURCE* pNetResource);

    bool GetNetResource(__out NETRESOURCE& sNetResource)
    {
        sNetResource = m_sNetResource;
        return m_bNetResourceValid;
    }

    void InvalidateNetResource()
    {
        if (m_type != TypeTSCFolder)
        {
            m_bNetResourceValid = false;
        }
    }

    bool IsNetResourceValid() const
    {
        return m_bNetResourceValid;
    }

    bool IsContainer() const
    {
        return (m_sNetResource.dwUsage & RESOURCEUSAGE_CONTAINER) != 0;
    }

    void SetType(Type type)
    {
        m_type = type;
    }

    Type GetType() const
    {
        return m_type;
    }

    void SetHint(Hint hint)
    {
        m_hint = hint;
    }

    Hint GetHint() const
    {
        return m_hint;
    }

    void SetStatus(__in Status status);

    Status GetStatus() const
    {
        return m_status;
    }

    Status GetPreviousStatus() const
    {
        return m_revertStatus;
    }

    void RevertStatus();

    void SetComment(__in PCTSTR pszComment);

    PCTSTR GetComment() const
    {
        return (m_pszComment != m_pszDisplayName) ? m_pszComment : NULL;
    }

    bool IsReady() const
    {
        return (m_status == StatusOk) || (m_status == StatusStandby);
    }

    void NotifyNodeUpdated();

    void SetLastEnumerationResult(DWORD dwResult)
    {
        m_dwLastEnumerationResult = dwResult;
    }

    DWORD GetLastEnumerationResult() const
    {
        return m_dwLastEnumerationResult;
    }

    void SetProvider(__in PCTSTR pszProvider);

    PCTSTR GetProvider() const
    {
        return m_pszProvider;
    }

    bool IsRoot() const
    {
        return (m_hint == HintRoot);
    }

    void* GetIterator() const
    {
        return m_myIterator;
    }

    static PCTSTR GetDisplayNameFromNetResource(
        __in const NETRESOURCE* pNetResource);

    static PCTSTR GetDisplayName(
        __in PCTSTR pszName,
        __in PCTSTR pszComment);

    int Compare(__in const PCTSTR pszDisplayName) const
    {
        return _tcsicmp(m_pszDisplayName, pszDisplayName);
    }

    int Compare(__in const PCTSTR pszDisplayName, size_t cchLen) const
    {
        size_t nLen1 = (m_pszDisplayName == NULL) ? 0 : _tcslen(m_pszDisplayName);

        if (nLen1 == cchLen)
        {
            return _tcsnicmp(m_pszDisplayName, pszDisplayName, nLen1);
        }

        return (nLen1 > cchLen) ? 1 : -1;
    }

    int Compare(__in const NETRESOURCE* pNetResource) const
    {
        return Compare(GetDisplayNameFromNetResource(pNetResource));
    }

    void Invalidate(__in bool bRecursive = true);

    void SetStandbyTimeout(__in UINT uTimeout);
    bool IsStandbyPeriodOver();

    bool IsImmortal() const
    {
        return m_bImmortal;
    }

    void SetImmortality()
    {
        m_bImmortal = true;
    }

    void ResetImmortality()
    {
        m_bImmortal = false;
    }

    void SetStickyImmortal()
    {
        m_bImmortalSticky = true;
    }

    bool TestAndResetStickyImmortal()
    {
        bool bWasSticky = m_bImmortalSticky;
        m_bImmortalSticky = false;
        return bWasSticky;
    }

    bool IsHidden() const
    {
        return m_bHidden;
    }

    void SetAttributes(bool bHidden)
    {
        m_bHidden = bHidden;
    }

    bool IsShortcut() const
    {
        return m_bShortcut;
    }

    void SetShortcut(bool bShortcut)
    {
        m_bShortcut = bShortcut;
    }

    void SetDisplayName(__in PCTSTR pszDisplayName);

    LONG AddRef()
    {
        assert(m_status != StatusDead);
        if (m_cRef > 2)
        {
            // 1st reference is raised if the node is displayed in
            //     the panel
            // 2nd reference may be temporarily bumped while
            //     filling the panel during refresh
            // (number of references may double if the path is
            // displayed in the second panel as well)
            TRACE_E("Suspicious increase of references to node " << (m_pszDisplayName ? m_pszDisplayName : "(null)") << " [" << m_myIterator << "], cRef=" << m_cRef);
        }
        return ++m_cRef;
    }

    LONG Release()
    {
        assert(m_cRef > 0);
        return --m_cRef;
    }

#if defined(_DEBUG) || defined(TRACE_ENABLE)
    bool CanKill() const;
#else
    bool CanKill() const
    {
        return (m_cRef == 0);
    }
#endif
};

/// UNC path parser.
class CUncPathParser
{
public:
    /// Token classification flags.
    enum TokenClassification
    {
        /// Token specifies name of a server.
        TokenServer = 2,

        /// Token specifies name of a share.
        TokenShare = 4,

        /// Token specifies address in IPv4 dotted format.
        TokenIPv4 = 8
    };

private:
    /// UNC path.
    PCTSTR m_pszPath;

    /// Token data.
    const TCHAR* m_pchData;

    /// Token length.
    size_t m_nLength;

    /// Points right after the last character (at the zero terminator)
    /// of the path being parsed.
    const TCHAR* m_pchMax;

    /// Classification of the token. This can be combination of one or
    /// more values from the TokenClassification enumeration.
    UINT m_uClassification;

    /// Last error code of the parsing.
    UINT m_uError;

public:
    /// Constructor.
    /// \param pszUncPath Path to be analyzed. This parameter is optional.
    CUncPathParser(__in_opt PCTSTR pszUncPath = NULL);

    /// Initializes the parser with the new path. This can be called to
    /// reuse the same instance of the parser.
    /// \param pszUncPath Path to be analyzed.
    void Initialize(__in PCTSTR pszUncPath);

    /// Moves to the next path token.
    /// \return If the method succeeds, the return value is true.
    ///         If the method fails, the return value is false.
    ///         The extended error code is available through the
    ///         GetLastError method.
    bool NextToken();

    /// Retrieves the text of the token.
    /// \return The return value is pointer to the token string data.
    /// \note The string is NOT explicitly terminated with the nul
    ///       character. Instead the length of the token should be
    ///       retrieved with the GetTokenLength method.
    const TCHAR* GetTokenData() const
    {
        assert(m_pchData != NULL);
        return m_pchData;
    }

    /// Returns length of the token.
    /// \return The return value is length of the token string,
    ///         in characters.
    size_t GetTokenLength() const
    {
        assert(m_nLength > 0);
        return m_nLength;
    }

    /// Retrieves classification of the token.
    /// \return The return value is classification of the token.
    ///         The classification is combination of one or more values
    ///         from the TokenClassification enumeration. If the
    ///         classification is not available, the return value is zero.
    UINT GetTokenClassification() const
    {
        return m_uClassification;
    }

    /// Returns parser's last error.
    /// \return The return value is the last error code.
    UINT GetLastError() const
    {
        return m_uError;
    }

    /// Resets the parser state.
    void Reset();

    /// Validates the path.
    /// \param pcTokens On return contains total count of tokens the path
    ///        consists of. This parameter is optional and may be NULL.
    /// \return If the path is syntactically valid, the return value
    ///         is NO_ERROR. If the path is a full UNC path, the return
    ///         value is ERROR_NETHOODCACHE_FULL_UNC_PATH. If the path
    ///         is not valid, the return value is ERROR_INVALID_NETNAME.
    /// \note After the validation, the parser is reset to the initial
    ///       state.
    UINT Validate(__out_opt int* pcTokens = NULL);

    /// Returns the path consisting of all the tokens previous the
    /// current one and including the current one.
    /// \param pszPath Pointer to the buffer.
    /// \param cchMax Size of the buffer, in characters.
    /// \return If the method succeeds, the return values is true.
    ///         Otherwise the return value is false.
    bool GetPathUpToCurrentToken(
        __out_ecount(cchMax) PTSTR pszPath,
        __in size_t cchMax) const;

    /// This is the static method for validating the path.
    /// \param pszUncPath The path to be validated.
    /// \return For the list of the possible return values see description
    ///         of the non-static Validate method.
    static UINT Validate(__in PCTSTR pszUncPath);
};

/// Error codes are 32-bit values (bit 31 is the most significant bit).
/// Bit 29 is reserved for application-defined error codes; no system error
/// code has this bit set. If you are defining an error code for your
/// application, set this bit to indicate that the error code has been defined
/// by the application and to ensure that your error code does not conflict
/// with any system-defined error codes.
#define ERROR_NETHOODCACHE_BASE (1 << 29)

/// The specified path seems to be valid UNC share path that could not be
/// cached. Such path should be displayed by the Salamander directly, not
/// by the Nethood plugin.
#define ERROR_NETHOODCACHE_FULL_UNC_PATH (ERROR_NETHOODCACHE_BASE + 1)

/// The specified path is actually a symbolic link to other path.
#define ERROR_NETHOODCACHE_SYMLINK (ERROR_NETHOODCACHE_BASE + 2)

/// Implements network cache.
class CNethoodCache
{
private:
    /// Cache tree data structure.
    typedef TTree<CNethoodCacheNode> CNethoodCacheTree;

    /// Miscelaneous constants that affects cache behavior and performance.
    enum
    {
        /// Delay (in milliseconds) before the re-enumeration of the
        /// node starts.
        REENUMERATION_DELAY = 3000,
    };

    /// Parameters for the GetPathWorker method.
    enum PathAssemblyType
    {
        /// Retrieve the full path.
        PathFull,

        /// Retrieve the UNC path.
        PathUnc
    };

public:
    /// Cache node.
    typedef CNethoodCacheTree::ITERATOR Node;

    /// Mode for the SpreadMode method.
    enum SpreadMode
    {
        SpreadOnly = 0,
        SpreadRemoveChildren = 1,
        _SpreadRemoveSelf = 2,
        SpreadRemoveAll = _SpreadRemoveSelf | SpreadRemoveChildren
    };

    /// Mode of displaying system shares.
    enum SystemSharesDisplayMode
    {
        /// Do not display system shares at all.
        SysShareNone,

        /// Display system shares as normal items.
        SysShareDisplayNormal,

        /// Display system shares as hidden items (blended icon).
        SysShareDisplayHidden
    };

    /// Mode of displaying terminal service client volumes.
    enum TSCDisplayMode
    {
        /// Do not display volumes at all.
        TSCDisplayNone,

        /// Display volumes in subfolder.
        TSCDisplayFolder,

        /// Display volumes in root.
        TSCDisplayRoot
    };

private:
    /// Cache tree.
    CNethoodCacheTree m_oTree;

    /// This protects the entire cache.
    CRITICAL_SECTION m_csLock;

    /// Queue of the enumerating threads.
    CThreadQueue m_oThreadQueue;

    /// Handle to the manual event signalled to quit all the threads.
    HANDLE m_hQuitEvent;

    /// Handle to the cache management thread. Use only in calls to m_oThreadQueue (m_hMgmtThread may be already closed).
    HANDLE m_hMgmtThread;

    /// Handle to the auto event signalled to wake up the
    /// management thread.
    HANDLE m_hWakeMgmtEvent;

    /// Number of nodes on the stand-by list.
    LONG m_cStandbyNodes;

    /// Head of the list of nodes waiting for re-enumeration.
    CNethoodCacheListEntry m_standbyNodes;

    /// Should enumerate hidden shares?
    SystemSharesDisplayMode m_displaySystemShares;

    /// Should display network shortcuts?
    bool m_bNetworkShortcuts;

    /// Should enumerate Terminal Services volumes?
    TSCDisplayMode m_displayTscVolumes;

    /// Virtual folder for Terminal Services volumes (not NULL
    /// if m_displayTscVolumes == TSCDisplayFolder).
    Node m_nodeTscFolder;

    friend class CNethoodCacheEnumerationThread;
    friend class CNethoodCacheManagementThread;
    friend class CNethoodCacheNode;
    friend class CTsClientName;

    /// Searches for named node between siblings.
    /// \param startNode Position where to start the search.
    /// \param token
    /// \return Returns position of the found node or NULL if the node
    ///         was not found.
    Node FindNextSiblingNode(
        __in Node startNode,
        __in const CUncPathParser& token);

    Node FindNextSiblingNode(
        __in Node startNode,
        __in PCTSTR pszDisplayName);

    Node NewNode(
        __in Node parentNode,
        __in const CUncPathParser& token);

    int CompareNode(
        __in Node node,
        __in const CUncPathParser& token);

    int CompareNode(
        __in Node node,
        __in PCTSTR pszDisplayName);

    bool ScheduleEnumeration(
        __in Node node,
        __in Node nodeDependsOn,
        __in UINT uDelay);

    Node InsertNode(__in Node nodeParent, __in Node nodeInsertAfter);

    bool PutNodeOnStandbyList(__in Node node);
    bool RemoveNodeFromStandbyList(__in Node node);

    void UnregisterConsumerWorker(
        __in Node node,
        __in CNethoodCacheEventConsumer* pEventConsumer,
        __in bool bInternal);

    /// Finds out whether the failure is temporary.
    /// \param dwError Error code.
    /// \return The return value is true, whether the error is
    ///         temporary and the caller may recover from it.
    ///         If the error is permanent, the return value is false.
    static bool IsRecoverableError(__in DWORD dwError)
    {
        assert(dwError != -1);

        // For now, we always start re-enumeration.
        return true;
    }

    /// Worker method for the GetFullPath and GetUncPath methods.
    bool GetPathWorker(
        __in Node node,
        __out_ecount(cchMax) PTSTR pszPath,
        __in size_t cchMax,
        __in PathAssemblyType type);

    bool CreateMgmtThread();

    typedef BOOL(WINAPI* PFN_WTSRegisterSessionNotification)(
        __in HWND hWnd,
        __in DWORD dwFlags);

    typedef BOOL(WINAPI* PFN_WTSUnRegisterSessionNotification)(
        __in HWND hWnd);

    typedef DWORD(WINAPI* PFN_WTSGetActiveConsoleSessionId)();

    /// Handle to the wtsapi32.dll module. NULL means that loading the
    /// module was not tried yet, INVALID_HANDLE_VALUE means that loading
    /// already tried but failed. Any other value is valid module handle.
    HMODULE m_hWtsApi32;

    PFN_WTSRegisterSessionNotification m_pfnWTSRegisterSessionNotification;
    PFN_WTSUnRegisterSessionNotification m_pfnWTSUnRegisterSessionNotification;
    PFN_WTSGetActiveConsoleSessionId m_pfnWTSGetActiveConsoleSessionId;

    bool EnsureWtsApiInitialized();

public:
    /// Constructor.
    CNethoodCache();

    /// Destructor.
    ~CNethoodCache();

    /// Cleans up the object and releases the associated resources.
    void Destroy();

    enum GetPathStatusFlags
    {
        GPSF_FORCE = 1,
        GPSF_MANUAL = 2,
        GPSF_DONT_RESOLVE_SYMLINK = 4,
    };

    /// Queries the cache status of the network path.
    /// \param pszPath Network path whose status is to be queried. The path
    ///        should start with '\' or '\\' characters.
    /// \param pEventConsumer Optional event consumer for the returned
    ///        node. If this parameter is non-NULL, the method will
    ///        atomically associate the consumer with the node to be
    ///        returned.
    /// \param node This parametr will receive the cache node that
    ///        identifies the network path in the cache.
    /// \param uFlags This parameter controls additional parameters that
    ///        may affect the query. This parameter can be combination of
    ///        one or more following values:
    ///        \c GPSF_FORCE
    ///        The node will be refreshed even in the case the node is
    ///        already cached.
    ///        \c GPSF_MANUAL
    ///        The path was manually entered by the user. If the node
    ///        will be successfully enumerated it will be made immortal.
    ///        \c GPSF_DONT_RESOLVE_SYMLINK
    ///        The symbolic link won't be resolved. The ERROR_NETHOODCACHE_SYMLINK
    ///        error code won't be returned.
    /// \param pszTargetPath If the return value is ERROR_NETHOODCACHE_FULL_UNC_PATH,
    ///        the buffer pointed to by this parameter will be filled with
    ///        the target UNC path. If the return value is ERROR_NETHOODCACHE_SYMLINK,
    ///        the buffer will contain the target path of the link. This
    ///        parameter is optional.
    /// \param cchMax Size of the \c pszTargetPath buffer, in characters.
    /// \return In the case of cache-hit the return value is NO_ERROR and
    ///       the enumeration of the node returned in the \c node
    ///       parameter can start immediately. In the case of the
    ///       cache-miss the return value is ERROR_IO_PENDING.
    ///       If the specified path is full UNC path that cannot be cached
    ///       the return value is ERROR_NETHOODCACHE_FULL_UNC_PATH.
    ///       If the \c pszPath is a symbolic link and \c uFlags
    ///       does not contain the GPSF_DONT_RESOLVE_SYMLINK value, the error
    ///       value is ERROR_NETHOODCACHE_SYMLINK and the \c pszTargetPath
    ///       (if specified) contains target path of the link.
    ///       If the method fails the return value is one of the system
    ///       error codes.
    /// \note The consumer is called while holding the cache lock.
    ///       Therefore the consumer should return as fast as possible
    ///       to prevent overall nethood cache performance degradation.
    UINT GetPathStatus(
        __in PCTSTR pszPath,
        __in_opt CNethoodCacheEventConsumer* pEventConsumer,
        __out_opt Node* node,
        __in unsigned uFlags = 0,
        __out_ecount_opt(cchMax) PTSTR pszTargetPath = NULL,
        __in_opt size_t cchMax = 0);

    /// Unregisters event consumer from the specific node.
    /// \param node Node, whose consumer is to be detached.
    /// \param pEventConsumer Pointer to the instance of the previously
    ///        attached CNethoodCacheEventConsumer interface.
    void UnregisterConsumer(
        __in Node node,
        __in CNethoodCacheEventConsumer* pEventConsumer)
    {
        UnregisterConsumerWorker(node, pEventConsumer, false);
    }

    /// Unregisters internal event consumer from the specific node.
    /// \param node Node, whose consumer is to be detached.
    /// \param pEventConsumer Pointer to the instance of the previously
    ///        attached CNethoodCacheEventConsumer interface.
    /// \note This method is reserved for internal use and should not be
    ///       used in the outside world. Internal event consumers are used
    ///       to maintain the private state of the cache.
    void UnregisterInternalConsumer(
        __in Node node,
        __in CNethoodCacheEventConsumer* pEventConsumer)
    {
        UnregisterConsumerWorker(node, pEventConsumer, true);
    }

    /// Locks the entire cache for exclusive access for the calling thread.
    void LockCache()
    {
        EnterCriticalSection(&m_csLock);
    }

    /// Unlocks the cache previously locked with LockCache.
    void UnlockCache()
    {
        LeaveCriticalSection(&m_csLock);
    }

#ifdef _DEBUG
    bool IsLocked() const
    {
        return (DWORD)(DWORD_PTR)((RTL_CRITICAL_SECTION*)&m_csLock)->OwningThread == GetCurrentThreadId();
    }
#endif

    Node ValidItem(__in Node node)
    {
        while (node != NULL)
        {
            CNethoodCacheNode::Status status = m_oTree.GetAt(node).GetStatus();

            if (status != CNethoodCacheNode::StatusDead &&
                status != CNethoodCacheNode::StatusInvisible)
            {
                break;
            }

            node = m_oTree.GetNextSibling(node);
        }

        return node;
    }

    Node GetFirstItem(__in Node node)
    {
        assert(m_oTree.GetAt(node).GetStatus() != CNethoodCacheNode::StatusDead);
        return ValidItem(m_oTree.GetFirstChild(node));
    }

    Node GetNextItem(__in Node node)
    {
        assert(m_oTree.GetAt(node).GetStatus() != CNethoodCacheNode::StatusDead);
        return ValidItem(m_oTree.GetNextSibling(node));
    }

    const CNethoodCacheNode& GetItemData(__in Node node)
    {
        if (m_oTree.GetAt(node).GetStatus() == CNethoodCacheNode::StatusDead)
        {
            TRACE_I("Accessing dead node " << DbgGetNodeName(node) << "[" << node << "]");
        }
        return m_oTree.GetAt(node);
    }

    bool BeginEnumerationThread(__in Node node);

    void InvalidateNetResource(__in Node node)
    {
        m_oTree.GetAt(node).InvalidateNetResource();
    }

    void RemoveItem(__in Node node);

    void SpreadError(
        __in Node node,
        __in UINT uError,
        __in SpreadMode mode);

    /// For the specified node returns full path.
    /// \param node Cache node.
    /// \param pszPath Pointer to the buffer that will receive the path.
    /// \param cchMax Specifies the maximum number of characters to copy
    ///        to the buffer, including the NULL character.
    /// \return If the method succeeds, the return value is true.
    bool GetFullPath(
        __in Node node,
        __out_ecount(cchMax) PTSTR pszPath,
        __in size_t cchMax)
    {
        return GetPathWorker(node, pszPath, cchMax, PathFull);
    }

    /// For the specified node returns the UNC path.
    /// \param node Cache node. The node must be a share.
    /// \param pszPath Pointer to the buffer that will receive the path.
    /// \param cchMax Specifies the maximum number of characters to copy
    ///        to the buffer, including the NULL character.
    /// \return If the method succeeds, the return value is true.
    bool GetUncPath(
        __in Node node,
        __out_ecount(cchMax) PTSTR pszPath,
        __in size_t cchMax)
    {
        return GetPathWorker(node, pszPath, cchMax, PathUnc);
    }

    /// Given a node finds valid higher level node.
    /// \param node Invalid node.
    /// \param pEventConsumer Optional event consumer for the returned
    ///        node. If this parameter is non-NULL, the method will
    ///        atomically associate the consumer with the node to be
    ///        returned.
    /// \return Returns first valid parent or grand-parent or
    ///         grand-grand-parent...
    /// \note The consumer is called while holding the cache lock.
    ///       Therefore the consumer should return as fast as possible
    ///       to prevent overall nethood cache performance degradation.
    Node FindValidItem(
        __in Node node,
        __in_opt CNethoodCacheEventConsumer* pEventConsumer);

    bool FindAccessiblePath(
        __in Node node,
        __out_ecount(cchMax) PTSTR pszPath,
        __in size_t cchMax);

    /// Creates the path if it does not exist yet.
    /// \param pszPath Path to create if needed. The path has to have the
    ///        UNC format and at least the server path must be specified.
    /// \return If the method succeeds, the return value is \c NO_ERROR.
    ///         Otherwise the return value is one of the Windows system
    ///         error code.
    UINT EnsurePathExists(__in PCTSTR pszPath);

    void AddRefNode(__in Node node);
    void ReleaseNode(__in Node node);

    SystemSharesDisplayMode GetDisplaySystemShares() const
    {
        return m_displaySystemShares;
    }

    void SetDisplaySystemShares(SystemSharesDisplayMode mode)
    {
        m_displaySystemShares = mode;
    }

    bool GetDisplayNetworkShortcuts() const
    {
        return m_bNetworkShortcuts;
    }

    void SetDisplayNetworkShortcuts(bool bDisplay)
    {
        m_bNetworkShortcuts = bDisplay;
    }

    TSCDisplayMode GetDisplayTSClientVolumes() const
    {
        return m_displayTscVolumes;
    }

    void SetDisplayTSClientVolumes(TSCDisplayMode mode);

    void WTSSessionChange(DWORD dwSessionId, int nStatus);

    /// Queries whether the Terminal Services are available
    /// on this system.
    bool AreTSAvailable();

#if defined(_DEBUG) || defined(TRACE_ENABLE)
    PCTSTR DbgGetNodeName(__in Node node)
    {
        PCTSTR pszName = m_oTree.GetAt(node).GetDisplayName();
        if (pszName == NULL)
            return TEXT("\\");
        return pszName;
    }
#else
    /*PCTSTR DbgGetNodeName(__in Node)
	{	
		return NULL;
	}*/
#endif
};

/// Terminal Services session name holder.
class CTsClientName
{
private:
    PTSTR m_pszClientName;
    int m_cchClientName;
    CNethoodCache* m_pCache;

public:
    CTsClientName(CNethoodCache* pCache);
    ~CTsClientName();

    operator PCTSTR() const
    {
        return m_pszClientName;
    }

    int Length() const
    {
        return m_cchClientName;
    }
};

/// Helper class for formatting Terminal Services volume display names.
class CTsNameFormatter
{
private:
    PCTSTR m_pszFormat;

public:
    CTsNameFormatter();

    static bool NeedsExtraFormatting(
        __in CNethoodCache::TSCDisplayMode displayMode)
    {
        return (displayMode == CNethoodCache::TSCDisplayRoot);
    }

    void Format(
        __in CNethoodCache::TSCDisplayMode displayMode,
        __in PCTSTR pszVolumeName,
        __in_opt const CTsClientName* poClientName,
        __out_ecount(cchMax) PTSTR pszDisplayName,
        __in size_t cchMax);
};

/// Interface for receiving cache events.
class CNethoodCacheEventConsumer : public CNethoodCacheListEntry
{
public:
    virtual void OnCacheNodeUpdated(__in CNethoodCache::Node node) = 0;

#ifdef _DEGUG
    virtual ~CNethoodCacheEventConsumer()
    {
    }
#endif
};

class CNethoodCacheBeginDependentEnumerationConsumer : public CNethoodCacheEventConsumer
{
protected:
    CNethoodCache* m_pCache;
    CNethoodCache::Node m_node;
    CNethoodCacheEventConsumer* m_pHitmanConsumer;

public:
    CNethoodCacheBeginDependentEnumerationConsumer(
        __in CNethoodCache* pCache,
        __in CNethoodCache::Node node)
    {
        assert(pCache != NULL);
        assert(node != NULL);

        m_pCache = pCache;
        m_node = node;
        m_pHitmanConsumer = NULL;
    }

    virtual void OnCacheNodeUpdated(__in CNethoodCache::Node node);

    void SetHitman(CNethoodCacheEventConsumer* pHitmanConsumer)
    {
        m_pHitmanConsumer = pHitmanConsumer;
    }
};

class CNethoodCacheHitmanConsumer : public CNethoodCacheEventConsumer
{
protected:
    CNethoodCache* m_pCache;
    CNethoodCache::Node m_nodeToKill;
    CNethoodCacheBeginDependentEnumerationConsumer* m_pConsumerToKill;

public:
    CNethoodCacheHitmanConsumer(
        __in CNethoodCache* pCache,
        __in CNethoodCache::Node nodeToKill,
        __in CNethoodCacheBeginDependentEnumerationConsumer* pConsumerToKill)
    {
        assert(pCache != NULL);
        assert(nodeToKill != NULL);
        assert(pConsumerToKill != NULL);

        m_pCache = pCache;
        m_nodeToKill = nodeToKill;
        m_pConsumerToKill = pConsumerToKill;
    }

    virtual void OnCacheNodeUpdated(__in CNethoodCache::Node node);
};

class CNethoodCacheEnumerationThread : public CThread
{
private:
    /// Defines base class of this class.
    typedef CThread _baseClass;

    enum
    {
        /// Size (in bytes) of the enumeration buffer (for the
        /// EnumResource method).
        ENUM_BUFFER_SIZE = 4 * 1024
    };

    /// Flags for the ProcessEnumeration method.
    enum ProcessEnumerationFlags
    {
        /// The elements in the array are system shares and should
        /// be displayed with the hidden attribute.
        ProcessHidden = 1,

        /// The elements in the array are network shortcuts.
        ProcessShortcut = 2,

        /// The elements in the array are volumes attached
        /// through a Terminal Services client session.
        ProcessTSCVolume = 4,
    };

    enum EnumerationPhase
    {
        PhaseNetwork,
        PhaseShortcuts,
        PhaseTerminalServices
    };

    /// Points to the cache object this enumeration thread belongs to.
    CNethoodCache* m_pCache;
    CNethoodCache::Node m_node;

    void BeginEnumeration(__in EnumerationPhase phase);
    void EndEnumeration(__in DWORD dwResult);

    void ProcessEnumeration(
        __in CNethoodCache::Node nodeParent,
        __in const NETRESOURCE* pNetResource,
        __in DWORD cElements,
        __in UINT uFlags,
        __in_opt const CTsClientName* poClientName);

    /// Updates cache with the array of the NETRESOURCE structures.
    /// \param pNetResource Pointer to the first element in the array
    ///        of the NETRESOURCE structures.
    /// \param cElements Number of elements in the array.
    /// \param uFlags Additional flags that control the way how the
    ///        elements are processed. This can be combination of one
    ///        or more values from the ProcessEnumerationFlags enumeration.
    /// \param pszClientName If uFlags contains the ProcessTSCVolume
    ///        flag then this parameter should specify the client's name.
    ///        Otherwise this parameter is ignored.
    void ProcessEnumeration(
        __in const NETRESOURCE* pNetResource,
        __in DWORD cElements,
        __in UINT uFlags,
        __in_opt const CTsClientName* poClientName = NULL)
    {
        ProcessEnumeration(m_node, pNetResource, cElements, uFlags,
                           poClientName);
    }

    void ProcessShareInfo(
        __in PCTSTR pszServerName,
        __in const SHARE_INFO_1& sShareInfo);

    void Invalidate();

    CNethoodCache::Node FindNode(
        __in CNethoodCache::Node nodeParent,
        __in const NETRESOURCE* pNetResource);

    void InvalidateNetResources(
        __in CNethoodCache::Node nodeParent,
        __in EnumerationPhase phase);

    int RemoveInvalidNetResources(__in CNethoodCache::Node nodeParent);

    void InvalidateNetResources(__in EnumerationPhase phase)
    {
        InvalidateNetResources(m_node, phase);
    }

    int RemoveInvalidNetResources()
    {
        return RemoveInvalidNetResources(m_node);
    }

    /// Starts an enumeration of network resources.
    /// This method wraps the WNetOpenEnum API. For description
    /// of parameters and possible return values see MSDN.
    static DWORD OpenEnum(
        __in DWORD dwScope,
        __in DWORD dwType,
        __in DWORD dwUsage,
        __in NETRESOURCE* pNetResource,
        __out HANDLE* phEnum);

    static DWORD CloseEnum(__in HANDLE hEnum)
    {
        assert(hEnum != NULL);
        return WNetCloseEnum(hEnum);
    }

    static DWORD EnumResource(
        __in HANDLE hEnum,
        __out DWORD& cEntries,
        __inout NETRESOURCE* pBuffer,
        __in DWORD cbBuffer);

    /// Enumerates hidden system shares (C$ etc).
    /// \param pszServerName UNC server name.
    /// \return If succeeded, returns NO_ERROR. Otherwise returns one of
    ///         the system defined error code.
    DWORD EnumHiddenShares(__in PCTSTR pszServerName);

    /// Workhorse for the EnumHiddenShares method on the NT platform.
    /// \see EnumHiddenShares
    DWORD EnumHiddenSharesNt(__in PCTSTR pszServerName);

    /// Enumerates network shortcuts.
    /// \return If succeeded, returns NO_ERROR. Otherwise returns one of
    ///         the system defined error code.
    DWORD EnumNetworkShortcuts();

    /// Enumerates volumes attached trough Terminal Service client.
    DWORD EnumTSClientVolumes();

    /// Reads shortcut target and inserts it as a child of the node
    /// being enumerated.
    /// \param pszName Display name of the shortcut.
    /// \param pszDir Path to the directory where the shortcut is stored.
    BOOL AddNetworkShortcut(
        __in PCTSTR pszName,
        __in PTSTR pszDir);

    /// Finds out whether the caller may recover from the error
    /// by providing user credentials.
    /// \param dwError Error code.
    /// \return The return value is true if there is an chance
    ///         to recover from the error by providing user
    ///         credentials. Otherwise the return value is false.
    static bool IsLogonFailure(__in DWORD dwError);

    /// Extends the error code.
    /// \param dwError WNet Error code.
    /// \return If the value is ERROR_EXTENDED_ERROR the return value
    ///         is the retrieved extended error code. Otherwise the
    ///         return value is the value of dwError parameter.
    static DWORD ExtendWNetError(__in DWORD dwError);

    /// Returns path to the user's network places folder.
    /// \return If the method succeeds, the return value is nonzero.
    ///         Otherwise the return value is zero.
    static BOOL GetShortcutsDir(__out PTSTR pszPath);

    ///
    static BOOL ResolveNetShortcut(__inout_ecount(MAX_PATH) PTSTR path);

public:
    CNethoodCacheEnumerationThread(
        __in CNethoodCache* pCache,
        __in CNethoodCache::Node node);

    virtual ~CNethoodCacheEnumerationThread();

    virtual unsigned Body();
};

/// Network cache thread that manages the stand-by list.
class CNethoodCacheManagementThread : public CThread
{
private:
    typedef CThread _baseClass;

    CNethoodCache* m_pCache;

    /// Handle to the window receiving the WM_WTSSESSION_CHANGE
    /// notification message.
    HWND m_hwndWtsNotify;

    ATOM m_wndWtsNotifyCls;

    DWORD WalkStandbyList();

    void DrainMsgQueue(bool bQuitting = false);
    void CreateWtsNotifyWindow();

    static LRESULT WINAPI WtsNotifyWndProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

    /// Helper routine to supress compiler warnings.
    static inline void SetInstanceToHwnd(
        HWND hwnd,
        CNethoodCacheManagementThread* pInstance)
    {
#ifndef _WIN64
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(pInstance));
#ifndef _WIN64
#pragma warning(pop)
#endif
    }

    static inline CNethoodCacheManagementThread* GetInstanceFromHwnd(
        HWND hwnd)
    {
#ifndef _WIN64
#pragma warning(push)
#pragma warning(disable : 4312)
#endif
        return reinterpret_cast<CNethoodCacheManagementThread*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
#ifndef _WIN64
#pragma warning(pop)
#endif
    }

public:
    CNethoodCacheManagementThread(__in CNethoodCache* pCache);

    virtual ~CNethoodCacheManagementThread();

    virtual unsigned Body();
};
