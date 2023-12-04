// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

BOOL InitViewer();
void ReleaseViewer();

extern HINSTANCE DLLInstance;
extern HINSTANCE HLanguage;
extern CSalamanderGeneralAbstract* SalamanderGeneral;

//
// ****************************************************************************
// CPluginInterface
//

class CPluginInterfaceForViewer : public CPluginInterfaceForViewerAbstract
{
public:
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex);
    virtual BOOL WINAPI CanViewFile(const char* name) { return TRUE; }
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent) {}

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) { return; }

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer();
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() { return NULL; }
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

//***********************************************************************************
//
// CImpIOleClientSite
//

class CSite;
class CIEWindow;

class CImpIOleClientSite : public IOleClientSite
{
protected:
    ULONG m_cRef;
    CSite* m_pSite;
    LPUNKNOWN m_pUnkOuter;

public:
    CImpIOleClientSite(CSite* pSite, IUnknown* pUnkOuter);
    ~CImpIOleClientSite();

    // IUnknown methods
    //
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG)
    AddRef();
    STDMETHODIMP_(ULONG)
    Release();

    // IOleClientSite methods
    //
    STDMETHODIMP SaveObject();
    STDMETHODIMP GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, LPMONIKER FAR* ppmk);
    STDMETHODIMP GetContainer(LPOLECONTAINER* ppContainer);
    STDMETHODIMP ShowObject();
    STDMETHODIMP OnShowWindow(BOOL fShow);
    STDMETHODIMP RequestNewObjectLayout();
};

//***********************************************************************************
//
// CImpIOleInPlaceSite
//

class CImpIOleInPlaceSite : public IOleInPlaceSite
{
protected:
    ULONG m_cRef;
    CSite* m_pSite;
    LPUNKNOWN m_pUnkOuter;

public:
    CImpIOleInPlaceSite(CSite* pSite, IUnknown* pUnkOuter);
    ~CImpIOleInPlaceSite();

    // IUnknown methods
    //
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG)
    AddRef();
    STDMETHODIMP_(ULONG)
    Release();

    // IOleWindow methods
    //
    STDMETHODIMP GetWindow(HWND* lphwnd);
    STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode);

    // IOleInPlaceSite methods
    //
    STDMETHODIMP CanInPlaceActivate();
    STDMETHODIMP OnInPlaceActivate();
    STDMETHODIMP OnUIActivate();
    STDMETHODIMP GetWindowContext(LPOLEINPLACEFRAME* lplpFrame,
                                  LPOLEINPLACEUIWINDOW* lplpDoc,
                                  LPRECT lprcPosRect,
                                  LPRECT lprcClipRect,
                                  LPOLEINPLACEFRAMEINFO lpFrameInfo);
    STDMETHODIMP Scroll(SIZE scrollExtent);
    STDMETHODIMP OnUIDeactivate(BOOL fUndoable);
    STDMETHODIMP OnInPlaceDeactivate();
    STDMETHODIMP DiscardUndoState();
    STDMETHODIMP DeactivateAndUndo();
    STDMETHODIMP OnPosRectChange(LPCRECT lprcPosRect);
};

//***********************************************************************************
//
// CImpIOleInPlaceFrame
//

class CImpIOleInPlaceFrame : public IOleInPlaceFrame
{
protected:
    ULONG m_cRef;
    CSite* m_pSite;
    LPUNKNOWN m_pUnkOuter;

public:
    CImpIOleInPlaceFrame(CSite* pSite, IUnknown* pUnkOuter);
    ~CImpIOleInPlaceFrame();

    // IUnknown methods
    //
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppvObj);
    STDMETHODIMP_(ULONG)
    AddRef();
    STDMETHODIMP_(ULONG)
    Release();

    // IOleWindow methods
    //
    STDMETHODIMP GetWindow(HWND* lphwnd);
    STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode);

    // IOleInPlaceUIWindow methods
    //
    STDMETHODIMP GetBorder(LPRECT lprectBorder);
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS lpborderwidths);
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS lpborderwidths);
    STDMETHODIMP SetActiveObject(LPOLEINPLACEACTIVEOBJECT lpActiveObject, LPCOLESTR lpszObjName);

    // IOleInPlaceFrame methods
    //
    STDMETHODIMP InsertMenus(HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths);
    STDMETHODIMP SetMenu(HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject);
    STDMETHODIMP RemoveMenus(HMENU hmenuShared);
    STDMETHODIMP SetStatusText(LPCOLESTR lpszStatusText);
    STDMETHODIMP EnableModeless(BOOL fEnable);
    STDMETHODIMP TranslateAccelerator(LPMSG lpmsg, WORD wID);
};

//***********************************************************************************
//
// CImpIOleControlSite
//

class CImpIOleControlSite : public IOleControlSite
{
protected:
    ULONG m_cRef;
    CSite* m_pSite;
    LPUNKNOWN m_pUnkOuter;

public:
    CImpIOleControlSite(CSite* pSite, IUnknown* pUnkOuter);
    ~CImpIOleControlSite();

    // IUnknown methods
    //
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppvObj);
    STDMETHODIMP_(ULONG)
    AddRef();
    STDMETHODIMP_(ULONG)
    Release();

    // CImpIOleControlSite methods
    //
    STDMETHODIMP OnControlInfoChanged();
    STDMETHODIMP LockInPlaceActive(BOOL fLock);
    STDMETHODIMP GetExtendedControl(LPDISPATCH* ppDisp);
    STDMETHODIMP TransformCoords(POINTL* lpptlHimetric, POINTF* lpptfContainer,
                                 DWORD flags);
    STDMETHODIMP TranslateAccelerator(LPMSG lpMsg, DWORD grfModifiers);
    STDMETHODIMP OnFocus(BOOL fGotFocus);
    STDMETHODIMP ShowPropertyFrame();
};

//***********************************************************************************
//
// CImpIAdviseSink
//
/*
class CImpIAdviseSink: public IAdviseSink
{
  protected:
    ULONG               m_cRef;
    CSite              *m_pSite;
    LPUNKNOWN           m_pUnkOuter;

  public:
    CImpIAdviseSink(CSite *pSite, IUnknown *pUnkOuter);
    ~CImpIAdviseSink();

    // IUnknown methods
    //
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IAdviseSink methods
    //
    STDMETHODIMP_(void) OnDataChange (FORMATETC FAR* pFormatetc, STGMEDIUM FAR* pStgmed);
    STDMETHODIMP_(void) OnViewChange (DWORD dwAspect, LONG lindex);
    STDMETHODIMP_(void) OnRename (LPMONIKER pmk);
    STDMETHODIMP_(void) OnSave ();
    STDMETHODIMP_(void) OnClose ();
};
*/
//***********************************************************************************
//
// CImpIDispatch
//

class CImpIDispatch : public IDispatch
{
protected:
    ULONG m_cRef;
    CSite* m_pSite;
    LPUNKNOWN m_pUnkOuter;

public:
    CImpIDispatch(CSite* pSite, IUnknown* pUnkOuter);
    ~CImpIDispatch();

    // IUnknown methods
    //
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppvObj);
    STDMETHODIMP_(ULONG)
    AddRef();
    STDMETHODIMP_(ULONG)
    Release();

    // IDispatch methods
    //
    STDMETHODIMP GetTypeInfoCount(unsigned int* pctinfo);
    STDMETHODIMP GetTypeInfo(unsigned int itinfo, LCID lcid, ITypeInfo** pptinfo);
    STDMETHODIMP GetIDsOfNames(REFIID riid,
                               OLECHAR** rgszNames,
                               unsigned int cNames,
                               LCID lcid,
                               DISPID* rgdispid);
    STDMETHODIMP Invoke(DISPID dispID,
                        REFIID riid,
                        LCID lcid,
                        unsigned short wFlags,
                        DISPPARAMS* pDispParams,
                        VARIANT* pVarResult,
                        EXCEPINFO* pExcepInfo,
                        unsigned int* puArgErr);

protected:
    BOOL CanonizeURL(char* psz);
};

//***********************************************************************************
//
// The CSite class, a COM object with the interfaces
//   IOleClientSite,
//   IOleInPlaceSite
//   IOleInPlaceFrame
//

class CSite : public IUnknown
{
    friend CImpIOleClientSite;
    friend CImpIOleInPlaceSite;
    friend CImpIOleInPlaceFrame;
    friend CImpIOleControlSite;
    //  friend CImpIAdviseSink;
    friend CImpIDispatch;
    friend CIEWindow;
    friend void NavigateAux(CSite* m_pSite, const char* fileName, IStream* contentStream);

protected:
    ULONG m_cRef;
    HWND m_hParentWnd; //Parent window
    RECT m_rect;       //Object position

    BOOL m_fInitialized; //Something here?
    BOOL m_fCreated;

    BOOL m_fCanClose; //obrana pred padanim s XML soubory
    BOOL m_fOpening;  //nedovilime je zavrit behem nacitani

    DWORD m_dwCookie; //connection point identifier for m_pConnectionPoint

    char MarkdownFilename[MAX_PATH];

    //Object interfaces
    IUnknown* m_pIUnknown;
    IWebBrowser* m_pIWebBrowser;
    IWebBrowser2* m_pIWebBrowser2; // pokud neni podporovano, bude rovno NULL
    IOleObject* m_pIOleObject;
    IOleInPlaceObject* m_pIOleInPlaceObject;
    IOleInPlaceActiveObject* m_pIOleInPlaceActiveObject;
    IViewObject* m_pIViewObject;
    IConnectionPoint* m_pConnectionPoint;

    //Our interfaces
    CImpIOleClientSite* m_pImpIOleClientSite;
    CImpIOleInPlaceSite* m_pImpIOleInPlaceSite;
    CImpIOleInPlaceFrame* m_pImpIOleInPlaceFrame;
    CImpIOleControlSite* m_pImpIOleControlSite;
    //    CImpIAdviseSink         *m_pImpIAdviseSink;
    CImpIDispatch* m_pImpIDispatch;

    //Our methods
protected:
    BOOL ObjectInitialize();

    void ConnectEvents();
    void DisconnectEvents();

public:
    CSite();
    ~CSite();

    // IUnknown methods for delegation
    //
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG)
    AddRef();
    STDMETHODIMP_(ULONG)
    Release();

    BOOL Create(HWND hParentWnd);
    void Close();
    HRESULT DoVerb(LONG iVerb);
};

//***********************************************************************************
//
// CIEWindow
//
// slouzi k zabaleni Interner Exoloreru
//

class CIEWindow
{
public:
    HWND HWindow;

protected:
    CSite m_Site;

public:
    CIEWindow() { HWindow = NULL; }

    // vytvori a pripoji se na IE
    BOOL CreateSite(HWND hParent);
    // provede uklid IE !! je treba volat pred destrukci okna
    void CloseSite();
    // hupne na pozadovane URL
    void Navigate(LPCTSTR lpszURL, IStream* contentStream);
    // tak tohle musime bohuzel volat z message-loopy
    HRESULT TranslateAccelerator(LPMSG lpmsg);
    BOOL CanClose();
};

//
// ****************************************************************************
// CIEMainWindow
//

struct CIEMainWindowQueueItem
{
    HWND HWindow;
    CIEMainWindowQueueItem* Next;

    CIEMainWindowQueueItem(HWND hWindow)
    {
        HWindow = hWindow;
        Next = NULL;
    }
};

class CIEMainWindowQueue
{
protected:
    CIEMainWindowQueueItem* Head;

    struct CCS // pristup z vice threadu -> nutna synchronizace
    {
        CRITICAL_SECTION cs;

        CCS() { InitializeCriticalSection(&cs); }
        ~CCS() { DeleteCriticalSection(&cs); }

        void Enter() { EnterCriticalSection(&cs); }
        void Leave() { LeaveCriticalSection(&cs); }
    } CS;

public:
    CIEMainWindowQueue() { Head = NULL; }
    ~CIEMainWindowQueue();

    BOOL Add(CIEMainWindowQueueItem* item); // prida polozku do fronty, vraci uspech
    void Remove(HWND hWindow);              // odstrani polozku z fronty
    BOOL Empty();                           // vraci TRUE pokud je fronta prazdna

    // broadcastne WM_CLOSE, pak ceka na prazdnou frontu (max. cas dle 'force' bud 'forceWaitTime'
    // nebo 'waitTime'); vraci TRUE pokud je fronta prazdna (vsechna okna se zavrela)
    // nebo je 'force' TRUE; cas INFINITE = neomezene dlouhe cekani
    // Poznamka: pri 'force' TRUE vraci vzdy TRUE, nema smysl na nic cekat, proto forceWaitTime = 0
    BOOL CloseAllWindows(BOOL force, int waitTime = 1000, int forceWaitTime = 0);
};

class CIEMainWindow
{
public:
    HWND HWindow;                                // handle okna viewru
    HANDLE Lock;                                 // 'lock' objekt nebo NULL (do signaled stavu az zavreme soubor)
    static CIEMainWindowQueue ViewerWindowQueue; // seznam vsech oken viewru
    static CThreadQueue ThreadQueue;             // seznam vsech threadu oken

    CIEWindow m_IEViewer;

public:
    CIEMainWindow();

    HANDLE GetLock();

    static LRESULT CALLBACK CIEMainWindowProc(HWND hwnd, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
