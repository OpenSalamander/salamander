// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef SHEXT_LOG_ENABLED
#define SHEXT_LOG_FILENAME "C:\\zumpa\\shext-log.txt"
// funkce pro zapis do log-file (pevna cesta, viz SHEXT_LOG_FILENAME)
void WriteToLog(const char* str);
#else                 // SHEXT_LOG_ENABLED
#define WriteToLog(a) // ignorujeme zpravy posilane do logu
#endif                // SHEXT_LOG_ENABLED

#define MyIsEqualIID(rguid1, rguid2) \
    (((PLONG)rguid1)[0] == ((PLONG)rguid2)[0] && \
     ((PLONG)rguid1)[1] == ((PLONG)rguid2)[1] && \
     ((PLONG)rguid1)[2] == ((PLONG)rguid2)[2] && \
     ((PLONG)rguid1)[3] == ((PLONG)rguid2)[3])

//****************************************************************************
//
// IShellExtClassFactory
//

// this class factory object creates context menu handlers for shell

#undef INTERFACE
#define INTERFACE IShellExtClassFactory

DECLARE_INTERFACE(IShellExtClassFactory)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)
    (THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef)
    (THIS) PURE;
    STDMETHOD_(ULONG, Release)
    (THIS) PURE;

    // IClassFactory methods
    STDMETHOD(CreateInstance)
    (THIS_
         LPUNKNOWN pUnkOuter,
     REFIID riid,
     LPVOID * ppvObj) PURE;

    STDMETHOD(LockServer)
    (THIS_
         BOOL fLock) PURE;
};

typedef struct
{
    IShellExtClassFactoryVtbl* lpVtbl;
    DWORD m_cRef;
} ShellExtClassFactory;

HRESULT CreateShellExtClassFactory(REFIID riid, LPVOID* ppvOut);
ShellExtClassFactory* SECF_Constructor();
void SECF_Destructor(ShellExtClassFactory* secf);
STDMETHODIMP SECF_QueryInterface(THIS_ REFIID riid, LPVOID* ppvObj);
STDMETHODIMP_(ULONG)
SECF_AddRef(THIS);
STDMETHODIMP_(ULONG)
SECF_Release(THIS);
STDMETHODIMP SECF_CreateInstance(THIS_
                                     LPUNKNOWN pUnkOuter,
                                 REFIID riid,
                                 LPVOID* ppvObj);
STDMETHODIMP SECF_LockServer(THIS_
                                 BOOL fLock);

//****************************************************************************
//
// ImpIShellExtInit
//

// this is the actual OLE Shell context menu handler

#undef INTERFACE
#define INTERFACE ImpIShellExtInit

DECLARE_INTERFACE(ImpIShellExtInit)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)
    (THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef)
    (THIS) PURE;
    STDMETHOD_(ULONG, Release)
    (THIS) PURE;

    // IShellExtInit methods
    STDMETHOD(Initialize)
    (THIS_
         LPCITEMIDLIST pIDFolder,
     LPDATAOBJECT pDataObj,
     HKEY hRegKey) PURE;
};

typedef struct ShellExt ShellExt;

typedef struct
{
    ImpIShellExtInitVtbl* lpVtbl;
    ShellExt* m_pObj;
} ShellExtInit;

STDMETHODIMP SEI_QueryInterface(THIS_ REFIID riid, LPVOID* ppvObj);
STDMETHODIMP_(ULONG)
SEI_AddRef(THIS);
STDMETHODIMP_(ULONG)
SEI_Release(THIS);

STDMETHODIMP SEI_Initialize(THIS_
                                LPCITEMIDLIST pIDFolder,
                            LPDATAOBJECT pDataObj,
                            HKEY hRegKey);

//****************************************************************************
//
// ImpICopyHook
//

#undef INTERFACE
#define INTERFACE ImpICopyHook

DECLARE_INTERFACE(ImpICopyHook)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface)
    (THIS_ REFIID riid, void** ppv) PURE;
    STDMETHOD_(ULONG, AddRef)
    (THIS) PURE;
    STDMETHOD_(ULONG, Release)
    (THIS) PURE;

    // *** ICopyHookA methods ***
    STDMETHOD_(UINT, CopyCallback)
    (THIS_ HWND hwnd, UINT wFunc, UINT wFlags,
     LPCSTR pszSrcFile, DWORD dwSrcAttribs,
     LPCSTR pszDestFile, DWORD dwDestAttribs) PURE;
};

typedef struct
{
    ImpICopyHookVtbl* lpVtbl;
    ShellExt* m_pObj;
} CopyHook;

STDMETHODIMP CH_QueryInterface(THIS_ REFIID riid, void** ppv);
STDMETHODIMP_(ULONG)
CH_AddRef(THIS);
STDMETHODIMP_(ULONG)
CH_Release(THIS);

STDMETHODIMP_(UINT)
CH_CopyCallback(THIS_ HWND hwnd, UINT wFunc, UINT wFlags,
                LPCSTR pszSrcFile, DWORD dwSrcAttribs,
                LPCSTR pszDestFile, DWORD dwDestAttribs);

//****************************************************************************
//
// ImpICopyHookW
//

#undef INTERFACE
#define INTERFACE ImpICopyHookW

DECLARE_INTERFACE(ImpICopyHookW)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface)
    (THIS_ REFIID riid, void** ppv) PURE;
    STDMETHOD_(ULONG, AddRef)
    (THIS) PURE;
    STDMETHOD_(ULONG, Release)
    (THIS) PURE;

    // *** ICopyHookW methods ***
    STDMETHOD_(UINT, CopyCallback)
    (THIS_ HWND hwnd, UINT wFunc, UINT wFlags, LPCWSTR pszSrcFile, DWORD dwSrcAttribs,
     LPCWSTR pszDestFile, DWORD dwDestAttribs) PURE;
};

typedef struct
{
    ImpICopyHookWVtbl* lpVtbl;
    ShellExt* m_pObj;
} CopyHookW;

STDMETHODIMP CHW_QueryInterface(THIS_ REFIID riid, void** ppv);
STDMETHODIMP_(ULONG)
CHW_AddRef(THIS);
STDMETHODIMP_(ULONG)
CHW_Release(THIS);

STDMETHODIMP_(UINT)
CHW_CopyCallback(THIS_ HWND hwnd, UINT wFunc, UINT wFlags,
                 LPCWSTR pszSrcFile, DWORD dwSrcAttribs,
                 LPCWSTR pszDestFile, DWORD dwDestAttribs);

//****************************************************************************
//
// ImpIContextMenu
//

#undef INTERFACE
#define INTERFACE IShellExt

DECLARE_INTERFACE(IShellExt)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)
    (THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef)
    (THIS) PURE;
    STDMETHOD_(ULONG, Release)
    (THIS) PURE;

#ifdef ENABLE_SH_MENU_EXT

    // IContextMenu methods
    STDMETHOD(QueryContextMenu)
    (THIS_
         HMENU hMenu,
     UINT indexMenu,
     UINT idCmdFirst,
     UINT idCmdLast,
     UINT uFlags) PURE;

    STDMETHOD(InvokeCommand)
    (THIS_
         LPCMINVOKECOMMANDINFO lpici) PURE;

    STDMETHOD(GetCommandString)
    (THIS_
         UINT idCmd,
     UINT uType,
     UINT * pwReserved,
     LPSTR pszName,
     UINT cchMax) PURE;

#endif // ENABLE_SH_MENU_EXT
};

STDMETHODIMP SE_QueryInterface(THIS_ REFIID riid, LPVOID* ppvObj);
STDMETHODIMP_(ULONG)
SE_AddRef(THIS);
STDMETHODIMP_(ULONG)
SE_Release(THIS);

#ifdef ENABLE_SH_MENU_EXT

STDMETHODIMP SE_QueryContextMenu(THIS_
                                     HMENU hMenu,
                                 UINT indexMenu,
                                 UINT idCmdFirst,
                                 UINT idCmdLast,
                                 UINT uFlags);
STDMETHODIMP SE_InvokeCommand(THIS_
                                  LPCMINVOKECOMMANDINFO lpici);

STDMETHODIMP SE_GetCommandString(THIS_
                                     UINT idCmd,
                                 UINT uType,
                                 UINT* pwReserved,
                                 LPSTR pszName,
                                 UINT cchMax);

#endif // ENABLE_SH_MENU_EXT

// **************************************************************************************

struct ShellExt
{
    IShellExtVtbl* lpVtbl;
    DWORD m_cRef;

    // zakladni rozhrani objektu je IContextMenu, dalsi rozhrani jsou zde:
    ShellExtInit* m_pSEI; // IShellExtInit
    CopyHook* m_pCH;      // ICopyHookA
    CopyHookW* m_pCHW;    // ICopyHookW

    // obdrzim pri IShellExtInit::Initialize
    //  LPITEMIDLIST  m_pIDFolder; // specifies the folder that contains the selected file objects
    LPDATAOBJECT m_pDataObj; // identifies the selected file objects
    //  HKEY          m_hRegKey;   // specifies the file class of the file object that has the focus
};

HRESULT CreateShellExt(REFIID riid, LPVOID* ppvOut);
ShellExt* SE_Constructor();
void SE_Destructor(ShellExt* se);
/*
// Shell Utils

//=========================================================================== 
// ITEMIDLIST 
//===========================================================================  
// unsafe macros from shesimp.h & idlist.c 
#define _ILSkip(pidl, cb)       ((LPITEMIDLIST)(((BYTE*)(pidl))+cb)) 
#define _ILNext(pidl)           _ILSkip(pidl, (pidl)->mkid.cb) 

//#ifdef _DEBUG 
// Dugging aids for making sure we dont use free pidls 
//#define VALIDATE_PIDL(pidl) Assert((pidl)->mkid.cb != 0xC5C5) 
//#else 
#define VALIDATE_PIDL(pidl) 
//#endif  

UINT ILGetSize(LPCITEMIDLIST pidl);
LPITEMIDLIST ILClone(LPCITEMIDLIST pidl);
void ILFree(LPITEMIDLIST pidl);

*/
