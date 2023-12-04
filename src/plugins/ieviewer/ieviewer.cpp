// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <comutil.h>

#include "ieviewer.h"
#include "ieviewer.rh"
#include "ieviewer.rh2"
#include "lang\lang.rh"
#include "dbg.h"

#include "markdown.h"

#pragma comment(lib, "comsuppw.lib")

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro viewer
CPluginInterfaceForViewer InterfaceForViewer;

const char* WINDOW_CLASSNAME = "IE Viewer Class";
ATOM AtomObject = 0;                                         // "property" okna s ukazatelem na objekt
CIEMainWindowQueue CIEMainWindow::ViewerWindowQueue;         // seznam vsech oken viewru
CThreadQueue CIEMainWindow::ThreadQueue("IEViewer Viewers"); // seznam vsech threadu oken

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

int ConfigVersion = 0;           // 0 - default, 1 - SS 1.6 beta 3, 2 - SS 1.6 beta 4, 3 - SS 2.5 beta 1, 4 - AS 3.1 beta 1
#define CURRENT_CONFIG_VERSION 4 // AS 3.1 beta 1
const char* CONFIG_VERSION = "Version";

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

char GetStringFromIIDBuffer[MAX_PATH];
char* GetStringFromIID(REFIID riid)
{
    CALL_STACK_MESSAGE1("GetStringFromIID()");
    LPOLESTR lplpsz;
    if (StringFromIID(riid, &lplpsz) == E_OUTOFMEMORY)
    {
        TRACE_E("Low memory");
        *GetStringFromIIDBuffer = 0;
    }
    else
    {
        WideCharToMultiByte(CP_ACP,
                            0,
                            lplpsz,
                            -1,
                            GetStringFromIIDBuffer,
                            MAX_PATH,
                            NULL,
                            NULL);
        GetStringFromIIDBuffer[MAX_PATH - 1] = 0;

        //free the string
        LPMALLOC pMalloc;
        CoGetMalloc(1, &pMalloc);
        if (pMalloc != NULL)
        {
            pMalloc->Free(lplpsz);
            pMalloc->Release();
        }
    }
    return GetStringFromIIDBuffer;
}

//DeleteInterfaceImp calls 'delete' and NULLs the pointer
#define DeleteInterfaceImp(p) \
    { \
        if (p != NULL) \
        { \
            delete p; \
            p = NULL; \
        } \
    }

//ReleaseInterface calls 'Release' and NULLs the pointer
#define ReleaseInterface(p) \
    { \
        IUnknown* pt = (IUnknown*)p; \
        p = NULL; \
        if (pt != NULL) \
            pt->Release(); \
    }

//
// ****************************************************************************
// DllMain
//

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DLLInstance = hinstDLL;
    return TRUE; // DLL can be loaded
}

//
// ****************************************************************************
// LoadStr
//

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// UpdateInternetFeatureControls
//

void WriteFeatureControl(const char* feature, const char* exe, DWORD value)
{
    char buff[1000];
    wsprintf(buff, "Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\%s", feature);
    HKEY hKey;
    DWORD disp;
    RegCreateKeyEx(HKEY_CURRENT_USER, buff, 0, NULL,
                   REG_OPTION_NON_VOLATILE, KEY_CREATE_SUB_KEY | KEY_WRITE, NULL, &hKey, &disp);
    if (hKey != NULL)
    {
        RegSetValueEx(hKey, exe, 0, REG_DWORD, (LPBYTE)&value, sizeof(value));
        RegCloseKey(hKey);
    }
    else
    {
        TRACE_E("RegCreateKeyEx() failed");
    }
}

void UpdateInternetFeatureControl()
{
    //Internet Feature Control Keys
    //https://msdn.microsoft.com/en-us/library/ee330720%28v=vs.85%29.aspx

    // o FEATURE_96DPI_PIXEL se v MSDN zminuji ze je deprecated a nahrazena DOCHOSTUIFLAG_DPI_AWARE
    // https://msdn.microsoft.com/en-us/library/aa753277%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396
    // ale zatim to chodi i pod W10/IE11, takze neresim

    char exePath[MAX_PATH];
    if (!GetModuleFileName(NULL, exePath, MAX_PATH))
        exePath[0] = 0;
    char* s = strrchr(exePath, '\\');
    if (s == NULL)
    {
        TRACE_E("GetModuleFileName() failed");
        return;
    }
    s++;

    WriteFeatureControl("FEATURE_96DPI_PIXEL", s, 1);                      // enable High-DPI support
    WriteFeatureControl("FEATURE_BROWSER_EMULATION", s, 11000);            // enable latest IE engine
    WriteFeatureControl("FEATURE_ENABLE_CLIPCHILDREN_OPTIMIZATION", s, 1); // enable Child Window Clipping
    WriteFeatureControl("FEATURE_GPU_RENDERING", s, 1);                    // enable GPU rendering
    WriteFeatureControl("FEATURE_AJAX_CONNECTIONEVENTS", s, 1);            // enable AJAX Connection Events
    WriteFeatureControl("FEATURE_DISABLE_NAVIGATION_SOUNDS", s, 1);        // disable navigation sounds
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // nastavime SalamanderVersion pro "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Internet Explorer Viewer" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Internet Explorer Viewer" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();

    // povolime renderovani v aktualnim IE enginu, High-DPI support, GPU akceleraci, ...
    static BOOL ifcUpdated = FALSE;
    if (!ifcUpdated)
    {
        UpdateInternetFeatureControl();
        ifcUpdated = TRUE;
    }

    if (!InitViewer())
        return NULL; // chyba

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_LOADSAVECONFIGURATION | FUNCTION_VIEWER,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "IEVIEWER");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

//
// ****************************************************************************
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
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    BOOL ret = CIEMainWindow::ViewerWindowQueue.Empty();
    if (!ret)
    {
        ret = CIEMainWindow::ViewerWindowQueue.CloseAllWindows(force) || force;
    }
    if (ret)
    {
        if (!CIEMainWindow::ThreadQueue.KillAll(force) && !force)
            ret = FALSE;
        else
            ReleaseViewer();
    }
    return ret;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    if (regKey != NULL) // load z registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
        {
            ConfigVersion = CURRENT_CONFIG_VERSION; // asi nejakej nenechavec... ;-)
        }
    }
    else // default configuration
    {
        ConfigVersion = 0;
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
}

const char* MARKDOWN_EXTENSIONS = "*.md;*.mdown;*.markdown";

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    char buff[1000];
    sprintf_s(buff, "*.htm;*.html;*.xml;*.mht;%s", MARKDOWN_EXTENSIONS);

    salamander->AddViewer(buff, FALSE); // default (install pluginu), jinak Salam ignoruje

    if (ConfigVersion < 2) // pred SS 1.6 beta 4
    {
        salamander->AddViewer("*.xml", TRUE);   // pridame *.xml  (do 1.6 beta 3, jinak uz tam je, neprida se)
        salamander->ForceRemoveViewer("*.jpg"); // vyhodime *.jpg  (z beta 3, jinak tam neni)
        salamander->ForceRemoveViewer("*.gif"); // vyhodime *.gif  (z beta 3, jinak tam neni)
    }

    if (ConfigVersion < 3) // pred SS 2.5 beta 1
    {
        salamander->AddViewer("*.mht", TRUE); // pridame *.mht  (do 2.5 beta 1, jinak uz tam je, neprida se)
    }

    if (ConfigVersion < 4) // pred AD 3.1 beta 1
    {
        salamander->AddViewer(MARKDOWN_EXTENSIONS, TRUE); // podpora pro Markdown
    }
}

CPluginInterfaceForViewerAbstract*
CPluginInterface::GetInterfaceForViewer()
{
    return &InterfaceForViewer;
}

struct CTVData
{
    BOOL AlwaysOnTop;
    const char* Name;
    IStream* ContentStream;
    int Left, Top, Width, Height;
    UINT ShowCmd;
    BOOL ReturnLock;
    HANDLE* Lock;
    BOOL* LockOwner;
    BOOL Success;
    HANDLE Continue;
};

unsigned WINAPI ThreadIEMessageLoop(void* param)
{
    CALL_STACK_MESSAGE1("ThreadIEMessageLoop(Version 1.09)");
    SetThreadNameInVCAndTrace("IELoop");
    TRACE_I("Begin");

    CTVData* data = (CTVData*)param;

    CIEMainWindow* window = new CIEMainWindow;
    if (window != NULL)
    {
        if (data->ReturnLock)
        {
            *data->Lock = window->GetLock();
            *data->LockOwner = TRUE;
        }
        CALL_STACK_MESSAGE1("ThreadIEMessageLoop::CreateWindowEx");
        if ((!data->ReturnLock || *data->Lock != NULL) &&
            CreateWindowEx(data->AlwaysOnTop ? WS_EX_TOPMOST : 0,
                           WINDOW_CLASSNAME,
                           LoadStr(IDS_PLUGINNAME),
                           WS_OVERLAPPEDWINDOW,
                           data->Left,
                           data->Top,
                           data->Width,
                           data->Height,
                           NULL,
                           NULL,
                           DLLInstance,
                           window) != NULL)
        {
            SendMessage(window->HWindow, WM_SETICON, ICON_BIG,
                        (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_IEVIEWER)));
            SendMessage(window->HWindow, WM_SETICON, ICON_SMALL,
                        (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_IEVIEWER)));
            CALL_STACK_MESSAGE1("ThreadIEMessageLoop::ShowWindow");
            ShowWindow(window->HWindow, data->ShowCmd);
            SetForegroundWindow(window->HWindow);
            UpdateWindow(window->HWindow);
            data->Success = TRUE;
        }
        else
        {
            CALL_STACK_MESSAGE1("ThreadIEMessageLoop::delete-window");
            if (data->ReturnLock && *data->Lock != NULL)
                CloseHandle(*data->Lock);
            delete window;
            window = NULL;
        }
    }

    CALL_STACK_MESSAGE1("ThreadIEMessageLoop::SetEvent");
    char name[MAX_PATH];
    lstrcpyn(name, data->Name, MAX_PATH);
    IStream* contentStream = data->ContentStream;
    BOOL openFile = data->Success;
    SetEvent(data->Continue); // pustime dale hl. thread, od tohoto bodu nejsou data platna (=NULL)
    data = NULL;

    // pokud probehlo vse bez potizi, otevreme v okne pozadovany soubor
    if (window != NULL && openFile)
    {
        CALL_STACK_MESSAGE1("ThreadIEMessageLoop::Navigate");
        if (contentStream != NULL)
            window->m_IEViewer.Navigate(name, contentStream);
        else
            window->m_IEViewer.Navigate(name, NULL);

        CALL_STACK_MESSAGE1("ThreadIEMessageLoop::message-loop");
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            {
                CALL_STACK_MESSAGE5("MSG(0x%p, 0x%X, 0x%IX, 0x%IX)", msg.hwnd, msg.message, msg.wParam, msg.lParam);
                if (window->m_IEViewer.TranslateAccelerator(&msg) != S_OK)
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }

    // pravdepodobne casta chyba v IE - objekt se puvodne destruoval v reakci na WM_DESTROY
    // pak ale prisla message jeste pred ukoncenim message pumpy a byl osloven zdestruovany
    // objekt
    // proto jsem presunul destrukci okna sem - obdoba ooStatic z WinLibu
    CALL_STACK_MESSAGE1("ThreadIEMessageLoop::message_loop done");
    delete window;

    TRACE_I("End");
    return 0;
}

enum FileFormatEnum
{
    ffeHTML,
    ffeMarkdown
};

FileFormatEnum GetFileFormat(const char* name)
{
    // naivni detekce podle pripony, asi by si to zaslouzilo heuristiku podle obsahu
    FileFormatEnum ret = ffeHTML;
    CSalamanderMaskGroup* masks = SalamanderGeneral->AllocSalamanderMaskGroup();
    if (masks != NULL)
    {
        masks->SetMasksString(MARKDOWN_EXTENSIONS, FALSE);
        int err;
        if (masks->PrepareMasks(err) && masks->AgreeMasks(name, NULL))
            ret = ffeMarkdown;
        SalamanderGeneral->FreeSalamanderMaskGroup(masks);
    }
    return ret;
}

BOOL CPluginInterfaceForViewer::ViewFile(const char* name, int left, int top, int width,
                                         int height, UINT showCmd, BOOL alwaysOnTop,
                                         BOOL returnLock, HANDLE* lock, BOOL* lockOwner,
                                         CSalamanderPluginViewerData* viewerData,
                                         int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    CALL_STACK_MESSAGE11("CPluginInterfaceForViewer::ViewFile(%s, %d, %d, %d, %d, "
                         "0x%X, %d, %d, , , , %d, %d)",
                         name, left, top, width, height,
                         showCmd, alwaysOnTop, returnLock, enumFilesSourceUID, enumFilesCurrentIndex);

    FileFormatEnum fileFormat = GetFileFormat(name);

    CTVData data;
    data.AlwaysOnTop = alwaysOnTop;
    data.Name = name;
    data.ContentStream = NULL;
    if (fileFormat == ffeMarkdown)
        data.ContentStream = ConvertMarkdownToHTML(name); // pokud vrati NULL, zobrazime soubor jako HTML
    data.Left = left;
    data.Top = top;
    data.Width = width;
    data.Height = height;
    data.ShowCmd = showCmd;
    data.ReturnLock = returnLock;
    data.Lock = lock;
    data.LockOwner = lockOwner;
    data.Success = FALSE;
    data.Continue = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (data.Continue == NULL)
    {
        TRACE_E("Nepodarilo se vytvorit Continue event.");
        return FALSE;
    }

    if (CIEMainWindow::ThreadQueue.StartThread(ThreadIEMessageLoop, &data))
    {
        // pockame, az thread zpracuje predana data a vrati vysledky
        WaitForSingleObject(data.Continue, INFINITE);
    }
    else
        data.Success = FALSE;
    CloseHandle(data.Continue);

    if (!data.Success)
    {
        SalamanderGeneral->SalMessageBox(NULL, LoadStr(IDS_UNABLETOOPENIE), LoadStr(IDS_ERRORTITLE),
                                         MB_ICONEXCLAMATION | MB_OK | MB_SETFOREGROUND);
    }

    return data.Success;
}

//
// ****************************************************************************
// InitViewer & ReleaseViewer
//

BOOL InitViewer()
{
    CALL_STACK_MESSAGE1("InitViewer()");
    AtomObject = GlobalAddAtom("object handle");
    if (AtomObject == 0)
    {
        TRACE_E("GlobalAddAtom has failed");
        return FALSE;
    }

    WNDCLASS wc;
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = CIEMainWindow::CIEMainWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = DLLInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WINDOW_CLASSNAME;
    if (RegisterClass(&wc) == 0)
    {
        TRACE_E("RegisterClass has failed");
        return FALSE;
    }
    return TRUE;
}

void ReleaseViewer()
{
    CALL_STACK_MESSAGE1("ReleaseViewer()");
    if (AtomObject != 0)
        GlobalDeleteAtom(AtomObject);
    if (!UnregisterClass(WINDOW_CLASSNAME, DLLInstance))
        TRACE_E("UnregisterClass(WINDOW_CLASSNAME) has failed");
}

//***********************************************************************************
//
// CImpIOleClientSite
//

CImpIOleClientSite::CImpIOleClientSite(CSite* pSite, IUnknown* pUnkOuter)
{
    TRACE_I("CImpIOleClientSite::CImpIOleClientSite");
    m_cRef = 0;
    m_pSite = pSite;
    m_pUnkOuter = pUnkOuter;
}

CImpIOleClientSite::~CImpIOleClientSite()
{
    TRACE_I("CImpIOleClientSite::~CImpIOleClientSite");
    if (m_cRef != 0)
        TRACE_E("CImpIOleClientSite::~CImpIOleClientSite m_cRef == " << m_cRef);
}

//
// IUnknown methods
//

STDMETHODIMP CImpIOleClientSite::QueryInterface(REFIID riid, void** ppvObj)
{
    SLOW_CALL_STACK_MESSAGE1("CImpIOleClientSite::QueryInterface(, )");
    //  TRACE_I("CImpIOleClientSite::QueryInterface Interface "<<GetStringFromIID(riid));
    return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG)
CImpIOleClientSite::AddRef()
{
    CALL_STACK_MESSAGE1("CImpIOleClientSite::AddRef()");
    //  TRACE_I("CImpIOleClientSite::AddRef");
    ++m_cRef;
    return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG)
CImpIOleClientSite::Release()
{
    CALL_STACK_MESSAGE1("CImpIOleClientSite::Release()");
    //  TRACE_I("CImpIOleClientSite::Release");
    if (m_cRef == 0)
        TRACE_E("CImpIOleClientSite::Release m_cRef == 0");
    --m_cRef;
    return m_pUnkOuter->Release();
}

//
// IOleClientSite methods
//

STDMETHODIMP CImpIOleClientSite::SaveObject()
{
    TRACE_I("CImpIOleClientSite::SaveObject");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleClientSite::GetMoniker(DWORD dwAssign,
                                            DWORD dwWhichMoniker,
                                            LPMONIKER FAR* ppmk)
{
    TRACE_I("CImpIOleClientSite::GetMoniker");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleClientSite::GetContainer(LPOLECONTAINER* ppContainer)
{
    TRACE_I("CImpIOleClientSite::GetContainer");
    *ppContainer = NULL;
    return E_NOINTERFACE;
}

STDMETHODIMP CImpIOleClientSite::ShowObject()
{
    TRACE_I("CImpIOleClientSite::ShowObject");
    return S_OK;
}

STDMETHODIMP CImpIOleClientSite::OnShowWindow(BOOL fShow)
{
    TRACE_I("CImpIOleClientSite::OnShowWindow");
    return NOERROR;
}

STDMETHODIMP CImpIOleClientSite::RequestNewObjectLayout()
{
    TRACE_I("CImpIOleClientSite::RequestNewObjectLayout");
    return E_NOTIMPL;
}

//***********************************************************************************
//
// CImpIOleInPlaceSite
//

CImpIOleInPlaceSite::CImpIOleInPlaceSite(CSite* pSite, IUnknown* pUnkOuter)
{
    TRACE_I("CImpIOleInPlaceSite::CImpIOleInPlaceSite");
    m_cRef = 0;
    m_pSite = pSite;
    m_pUnkOuter = pUnkOuter;
}

CImpIOleInPlaceSite::~CImpIOleInPlaceSite()
{
    TRACE_I("CImpIOleInPlaceSite::~CImpIOleInPlaceSite");
    if (m_cRef != 0)
        TRACE_E("CImpIOleInPlaceSite::~CImpIOleInPlaceSite m_cRef == " << m_cRef);
}

//
// IUnknown methods
//

STDMETHODIMP CImpIOleInPlaceSite::QueryInterface(REFIID riid, void** ppvObj)
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceSite::QueryInterface(, )");
    //  TRACE_I("CImpIOleInPlaceSite::QueryInterface Interface "<<GetStringFromIID(riid));
    return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG)
CImpIOleInPlaceSite::AddRef()
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceSite::AddRef()");
    //  TRACE_I("CImpIOleInPlaceSite::AddRef");
    ++m_cRef;
    return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG)
CImpIOleInPlaceSite::Release()
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceSite::Release()");
    //  TRACE_I("CImpIOleInPlaceSite::Release");
    if (m_cRef == 0)
        TRACE_E("CImpIOleInPlaceSite::Release m_cRef == 0");
    --m_cRef;
    return m_pUnkOuter->Release();
}

//
// IOleWindow methods
//

STDMETHODIMP CImpIOleInPlaceSite::GetWindow(HWND* lphwnd)
{
    TRACE_I("CImpIOleInPlaceSite::GetWindow");
    *lphwnd = m_pSite->m_hParentWnd;
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::ContextSensitiveHelp(BOOL fEnterMode)
{
    TRACE_I("CImpIOleInPlaceSite::ContextSensitiveHelp");
    return E_NOTIMPL;
}

//
// IOleInPlaceSite methods
//

STDMETHODIMP CImpIOleInPlaceSite::CanInPlaceActivate()
{
    TRACE_I("CImpIOleInPlaceSite::CanInPlaceActivate");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::OnInPlaceActivate()
{
    TRACE_I("CImpIOleInPlaceSite::OnInPlaceActivate");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::OnUIActivate()
{
    TRACE_I("CImpIOleInPlaceSite::OnUIActivate");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::GetWindowContext(LPOLEINPLACEFRAME* lplpFrame,
                                                   LPOLEINPLACEUIWINDOW* lplpDoc,
                                                   LPRECT lprcPosRect,
                                                   LPRECT lprcClipRect,
                                                   LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceSite::GetWindowContext(, , , , )");
    TRACE_I("CImpIOleInPlaceSite::GetWindowContext");
    *lplpFrame = m_pSite->m_pImpIOleInPlaceFrame;
    (*lplpFrame)->AddRef();
    *lplpDoc = NULL;
    *lprcPosRect = m_pSite->m_rect;
    *lprcClipRect = m_pSite->m_rect;

    lpFrameInfo->cb = sizeof(OLEINPLACEFRAMEINFO);
    lpFrameInfo->fMDIApp = FALSE;
    lpFrameInfo->hwndFrame = m_pSite->m_hParentWnd;
    lpFrameInfo->haccel = NULL;
    lpFrameInfo->cAccelEntries = 0;
    return NOERROR;
}

STDMETHODIMP CImpIOleInPlaceSite::Scroll(SIZE scrollExtent)
{
    TRACE_I("CImpIOleInPlaceSite::Scroll");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::OnUIDeactivate(BOOL fUndoable)
{
    TRACE_I("CImpIOleInPlaceSite::OnUIDeactivate");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::OnInPlaceDeactivate()
{
    TRACE_I("CImpIOleInPlaceSite::OnInPlaceDeactivate");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::DiscardUndoState()
{
    TRACE_I("CImpIOleInPlaceSite::DiscardUndoState");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::DeactivateAndUndo()
{
    TRACE_I("CImpIOleInPlaceSite::DeactivateAndUndo");
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceSite::OnPosRectChange(LPCRECT lprcPosRect)
{
    TRACE_I("CImpIOleInPlaceSite::OnPosRectChange");
    return S_OK;
}

//***********************************************************************************
//
// CImpIOleInPlaceFrame
//

CImpIOleInPlaceFrame::CImpIOleInPlaceFrame(CSite* pSite, IUnknown* pUnkOuter)
{
    TRACE_I("CImpIOleInPlaceFrame::CImpIOleInPlaceFrame");
    m_cRef = 0;
    m_pSite = pSite;
    m_pUnkOuter = pUnkOuter;
}

CImpIOleInPlaceFrame::~CImpIOleInPlaceFrame()
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceFrame::~CImpIOleInPlaceFrame()");
    TRACE_I("CImpIOleInPlaceFrame::~CImpIOleInPlaceFrame");
    if (m_cRef != 0)
        TRACE_E("CImpIOleInPlaceFrame::~CImpIOleInPlaceFrame m_cRef == " << m_cRef);
}

//
// IUnknown methods
//

STDMETHODIMP CImpIOleInPlaceFrame::QueryInterface(REFIID riid, void** ppvObj)
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceFrame::QueryInterface(, )");
    //  TRACE_I("CImpIOleInPlaceFrame::QueryInterface Interface "<<GetStringFromIID(riid));
    return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG)
CImpIOleInPlaceFrame::AddRef()
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceFrame::AddRef()");
    //  TRACE_I("CImpIOleInPlaceFrame::AddRef");
    ++m_cRef;
    return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG)
CImpIOleInPlaceFrame::Release()
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceFrame::Release()");
    //  TRACE_I("CImpIOleInPlaceFrame::Release");
    if (m_cRef == 0)
        TRACE_E("CImpIOleInPlaceFrame::Release m_cRef == 0");
    --m_cRef;
    return m_pUnkOuter->Release();
}

//
// IOleWindow methods
//

STDMETHODIMP CImpIOleInPlaceFrame::GetWindow(HWND* lphwnd)
{
    *lphwnd = m_pSite->m_hParentWnd;
    return *lphwnd != NULL ? S_OK : E_FAIL;
}

STDMETHODIMP CImpIOleInPlaceFrame::ContextSensitiveHelp(BOOL fEnterMode)
{
    TRACE_E("CImpIOleInPlaceFrame::ContextSensitiveHelp");
    return E_NOTIMPL;
}

//
// IOleInPlaceUIWindow methods
//

STDMETHODIMP CImpIOleInPlaceFrame::GetBorder(LPRECT lprectBorder)
{
    TRACE_I("CImpIOleInPlaceFrame::GetBorder");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleInPlaceFrame::RequestBorderSpace(LPCBORDERWIDTHS lpborderwidths)
{
    TRACE_I("CImpIOleInPlaceFrame::RequestBorderSpace");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleInPlaceFrame::SetBorderSpace(LPCBORDERWIDTHS lpborderwidths)
{
    TRACE_I("CImpIOleInPlaceFrame::SetBorderSpace");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleInPlaceFrame::SetActiveObject(LPOLEINPLACEACTIVEOBJECT lpActiveObject,
                                                   LPCOLESTR lpszObjName)
{
    TRACE_I("CImpIOleInPlaceFrame::SetActiveObject");
    return E_NOTIMPL;
}

//
// IOleInPlaceFrame methods
//

STDMETHODIMP CImpIOleInPlaceFrame::InsertMenus(HMENU hmenuShared,
                                               LPOLEMENUGROUPWIDTHS lpMenuWidths)
{
    TRACE_I("CImpIOleInPlaceFrame::InsertMenus");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleInPlaceFrame::SetMenu(HMENU hmenuShared,
                                           HOLEMENU holemenu,
                                           HWND hwndActiveObject)
{
    TRACE_I("CImpIOleInPlaceFrame::SetMenu");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleInPlaceFrame::RemoveMenus(HMENU hmenuShared)
{
    TRACE_I("CImpIOleInPlaceFrame::RemoveMenus");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleInPlaceFrame::SetStatusText(LPCOLESTR lpszStatusText)
{
    CALL_STACK_MESSAGE1("CImpIOleInPlaceFrame::SetStatusText()");
    char buff[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, lpszStatusText, -1, buff,
                        MAX_PATH, NULL, NULL);
    buff[MAX_PATH - 1] = 0;
    TRACE_I("CImpIOleInPlaceFrame::SetStatusText: " << buff);
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleInPlaceFrame::EnableModeless(BOOL fEnable)
{
    TRACE_I("CImpIOleInPlaceSite::EnableModeless fEnable = " << fEnable);
    return S_OK;
}

STDMETHODIMP CImpIOleInPlaceFrame::TranslateAccelerator(LPMSG lpmsg,
                                                        WORD wID)
{
    TRACE_I("CImpIOleInPlaceFrame::TranslateAccelerator");
    return S_FALSE; // The keystroke was not used.
}

//***********************************************************************************
//
// CImpIOleControlSite
//

CImpIOleControlSite::CImpIOleControlSite(CSite* pSite, IUnknown* pUnkOuter)
{
    TRACE_I("CImpIOleControlSite::CImpIOleControlSite");
    m_cRef = 0;
    m_pSite = pSite;
    m_pUnkOuter = pUnkOuter;
}

CImpIOleControlSite::~CImpIOleControlSite()
{
    TRACE_I("CImpIOleControlSite::~CImpIOleControlSite");
    if (m_cRef != 0)
        TRACE_E("CImpIOleControlSite::~CImpIOleControlSite m_cRef == " << m_cRef);
}

// IUnknown methods
//
STDMETHODIMP CImpIOleControlSite::QueryInterface(REFIID riid, LPVOID* ppvObj)
{
    CALL_STACK_MESSAGE1("CImpIOleControlSite::QueryInterface(, )");
    //  TRACE_I("CImpIOleControlSite::QueryInterface Interface "<<GetStringFromIID(riid));
    return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG)
CImpIOleControlSite::AddRef()
{
    CALL_STACK_MESSAGE1("CImpIOleControlSite::AddRef()");
    //  TRACE_I("CImpIOleControlSite::AddRef");
    ++m_cRef;
    return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG)
CImpIOleControlSite::Release()
{
    CALL_STACK_MESSAGE1("CImpIOleControlSite::Release()");
    //  TRACE_I("CImpIOleControlSite::Release");
    if (m_cRef == 0)
        TRACE_E("CImpIOleControlSite::Release m_cRef == 0");
    --m_cRef;
    return m_pUnkOuter->Release();
}

// CImpIOleControlSite methods
//
STDMETHODIMP CImpIOleControlSite::OnControlInfoChanged()
{
    TRACE_I("CImpIOleControlSite::OnControlInfoChanged");
    return NOERROR;
}

STDMETHODIMP CImpIOleControlSite::LockInPlaceActive(BOOL fLock)
{
    TRACE_I("CImpIOleControlSite::LockInPlaceActive");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleControlSite::GetExtendedControl(LPDISPATCH* ppDisp)
{
    TRACE_I("CImpIOleControlSite::GetExtendedControl");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleControlSite::TransformCoords(POINTL* lpptlHimetric,
                                                  POINTF* lpptfContainer,
                                                  DWORD flags)
{
    TRACE_I("CImpIOleControlSite::TransformCoords");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleControlSite::TranslateAccelerator(LPMSG lpMsg,
                                                       DWORD grfModifiers)
{
    CALL_STACK_MESSAGE2("CImpIOleControlSite::TranslateAccelerator(, 0x%X)",
                        grfModifiers);
    TRACE_I("CImpIOleControlSite::TranslateAccelerator");
    if (lpMsg->message == WM_KEYDOWN &&
        lpMsg->wParam == 'R' &&
        (GetKeyState(VK_CONTROL) & 0x8000) != 0)
    {
        if (m_pSite->MarkdownFilename[0] != 0)
        {
            IStream* contentStream = ConvertMarkdownToHTML(m_pSite->MarkdownFilename);
            if (contentStream != NULL && m_pSite->m_pIWebBrowser2 != NULL)
            {
                NavigateAux(m_pSite, m_pSite->MarkdownFilename, contentStream);
                return S_OK;
            }
        }
        m_pSite->m_pIWebBrowser->Refresh();
        return S_OK;
    }
    return E_NOTIMPL;
}

STDMETHODIMP CImpIOleControlSite::OnFocus(BOOL fGotFocus)
{
    TRACE_I("CImpIOleControlSite::OnFocus fGotFocus = " << fGotFocus);
    return S_OK;
}

STDMETHODIMP CImpIOleControlSite::ShowPropertyFrame()
{
    TRACE_I("CImpIOleControlSite::ShowPropertyFrame");
    return E_NOTIMPL;
}

//***********************************************************************************
//
// CImpIAdviseSink
//
/*
CImpIAdviseSink::CImpIAdviseSink(CSite *pSite, IUnknown *pUnkOuter)
{
  TRACE_I("CImpIAdviseSink::CImpIAdviseSink");
  m_cRef = 0;
  m_pSite = pSite;
  m_pUnkOuter = pUnkOuter;
}

CImpIAdviseSink::~CImpIAdviseSink()
{
  TRACE_I("CImpIAdviseSink::~CImpIAdviseSink");
  if (m_cRef != 0) TRACE_E("CImpIAdviseSink::~CImpIAdviseSink m_cRef == "<<m_cRef);
}

// IUnknown methods
//
STDMETHODIMP CImpIAdviseSink::QueryInterface(REFIID riid, LPVOID *ppvObj)
{
  CALL_STACK_MESSAGE1("CImpIAdviseSink::QueryInterface(, )");
//  TRACE_I("CImpIAdviseSink::QueryInterface Interface "<<GetStringFromIID(riid));
  return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG) CImpIAdviseSink::AddRef()
{
  CALL_STACK_MESSAGE1("CImpIAdviseSink::AddRef()");
//  TRACE_I("CImpIAdviseSink::AddRef");
  ++m_cRef;
  return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG) CImpIAdviseSink::Release()
{
  CALL_STACK_MESSAGE1("CImpIAdviseSink::Release()");
//  TRACE_I("CImpIAdviseSink::Release");
  if (m_cRef == 0) TRACE_E("CImpIAdviseSink::Release m_cRef == 0");
  --m_cRef;
  return m_pUnkOuter->Release();
}

// IAdviseSink methods
//
STDMETHODIMP_(void) CImpIAdviseSink::OnDataChange(FORMATETC FAR* pFormatetc, STGMEDIUM FAR* pStgmed)
{
  TRACE_I("CImpIAdviseSink::OnDataChange");
}

STDMETHODIMP_(void) CImpIAdviseSink::OnViewChange(DWORD dwAspect, LONG lindex)
{
  CALL_STACK_MESSAGE3("CImpIAdviseSink::OnViewChange(0x%X, %d)", dwAspect,
                      lindex);
  TRACE_I("CImpIAdviseSink::OnViewChange");
  BSTR pbstrLocationURL;
  if (m_pSite->m_pIWebBrowser != NULL &&
      m_pSite->m_pIWebBrowser->get_LocationURL(&pbstrLocationURL) == S_OK)
  {
    char locationURL[1024];
    char locationName[1024];
    locationURL[0] = 0;
    locationName[0] = 0;

    WideCharToMultiByte(CP_ACP, 0, pbstrLocationURL, -1, locationURL, 1024, NULL, NULL);
    locationURL[1024 - 1] = 0;

    BSTR pbstrLocationName;
    if (m_pSite->m_pIWebBrowser2 != NULL &&
        m_pSite->m_pIWebBrowser2->get_LocationName(&pbstrLocationName) == S_OK)
    {
      WideCharToMultiByte(CP_ACP, 0, pbstrLocationName, -1, locationName, 1024, NULL, NULL);
      locationName[1024 - 1] = 0;
      SysFreeString(pbstrLocationName);
    }

    char location[1024];
    BOOL file = strncmp(locationURL, "file:", 5) == 0;
    if (file || locationName[0] == 0)
      lstrcpy(location, locationURL);
    else
      lstrcpy(location, locationName);

    char title[MAX_PATH + 200];
    title[0] = 0;
    if (location[0] != 0)
      sprintf(title, "%s - ", location);
    lstrcat(title, LoadStr(IDS_PLUGINNAME));
//    SetWindowText(m_pSite->m_hParentWnd, title);

    if (m_pSite->m_pIWebBrowser2 != NULL)
      m_pSite->m_pIWebBrowser2->put_StatusBar(TRUE);
    SysFreeString(pbstrLocationURL);
  }
  m_pSite->DoVerb(OLEIVERB_UIACTIVATE);
}

STDMETHODIMP_(void) CImpIAdviseSink::OnRename(LPMONIKER pmk)
{
  TRACE_I("CImpIAdviseSink::OnRename");
}

STDMETHODIMP_(void) CImpIAdviseSink::OnSave()
{
  TRACE_I("CImpIAdviseSink::OnSave");
}

STDMETHODIMP_(void) CImpIAdviseSink::OnClose()
{
  TRACE_I("CImpIAdviseSink::OnClose");
}
*/
//***********************************************************************************
//
// CImpIDispatch
//

CImpIDispatch::CImpIDispatch(CSite* pSite, IUnknown* pUnkOuter)
{
    TRACE_I("CImpIDispatch::CImpIDispatch");
    m_cRef = 0;
    m_pSite = pSite;
    m_pUnkOuter = pUnkOuter;
}

CImpIDispatch::~CImpIDispatch()
{
    TRACE_I("CImpIDispatch::~CImpIDispatch");
    if (m_cRef != 0)
        TRACE_E("CImpIDispatch::~CImpIDispatch m_cRef == " << m_cRef);
}

// IUnknown methods
//
STDMETHODIMP CImpIDispatch::QueryInterface(REFIID riid, LPVOID* ppvObj)
{
    CALL_STACK_MESSAGE1("CImpIDispatch::QueryInterface(, )");
    //  TRACE_I("CImpIDispatch::QueryInterface Interface "<<GetStringFromIID(riid));
    return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG)
CImpIDispatch::AddRef()
{
    CALL_STACK_MESSAGE1("CImpIDispatch::AddRef()");
    //  TRACE_I("CImpIDispatch::AddRef");
    ++m_cRef;
    return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG)
CImpIDispatch::Release()
{
    CALL_STACK_MESSAGE1("CImpIDispatch::Release()");
    //  TRACE_I("CImpIDispatch::Release");
    if (m_cRef == 0)
        TRACE_E("CImpIDispatch::Release m_cRef == 0");
    --m_cRef;
    return m_pUnkOuter->Release();
}

// CImpIDispatch methods
//
STDMETHODIMP CImpIDispatch::GetTypeInfoCount(unsigned int* pctinfo)
{
    TRACE_I("CImpIDispatch::GetTypeInfoCount");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIDispatch::GetTypeInfo(unsigned int itinfo, LCID lcid, ITypeInfo** pptinfo)
{
    TRACE_I("CImpIDispatch::GetTypeInfo");
    return E_NOTIMPL;
}

STDMETHODIMP CImpIDispatch::GetIDsOfNames(REFIID riid,
                                          OLECHAR** rgszNames,
                                          unsigned int cNames,
                                          LCID lcid,
                                          DISPID* rgdispid)
{
    TRACE_I("CImpIDispatch::GetIDsOfNames");
    return E_NOTIMPL;
}

BOOL CImpIDispatch::CanonizeURL(char* psz)
{
    LPTSTR pszSource = psz;
    LPTSTR pszDest = psz;

    BOOL file = _strnicmp(psz, "file://", 7) == 0;

    static const char szHex[] = ("0123456789ABCDEF");

    // unescape special characters

    while (*pszSource != '\0')
    {
        //    if (*pszSource == '+')   // j.r. padalo v pripade zobrazeni souboru v adresari obsahujici v nazvu '+'
        //      *pszDest++ = ' ';      // netusim, proc tam eliminace '+' puvodne byla (prevzaty kod)
        //    else
        if (*pszSource == '%')
        {
            TCHAR nValue = '?';
            LPCTSTR pszLow;
            LPCTSTR pszHigh;
            pszSource++;

            *pszSource = toupper(*pszSource);

            pszHigh = strchr(szHex, *pszSource);

            if (pszHigh != NULL)
            {
                pszSource++;
                *pszSource = toupper(*pszSource);
                pszLow = strchr(szHex, *pszSource);
                if (pszLow != NULL)
                {
                    nValue = (TCHAR)(((pszHigh - szHex) << 4) + (pszLow - szHex));
                }
            }

            *pszDest = nValue;
        }
        else
            *pszDest = *pszSource;
        if (file && *pszDest == '/')
            *pszDest = '\\';
        pszDest++;
        pszSource++;
    }
    *pszDest = '\0';
    if (file)
    {
        int offset = 7;
        if (m_pSite->m_pIWebBrowser2 != NULL)
        {
            // > IE 3.02 - novejsi IE pisi "file:\\\C:\" nebo "file:\\john\c"
            if (psz[7] == '\\' && __isascii(psz[8]))
                offset++;
            else
                offset -= 2;
        }
        memmove(psz, psz + offset, lstrlen(psz) - offset + 1);
    }
    return file;
}

STDMETHODIMP CImpIDispatch::Invoke(DISPID dispID,
                                   REFIID riid,
                                   LCID lcid,
                                   unsigned short wFlags,
                                   DISPPARAMS* pDispParams,
                                   VARIANT* pVarResult,
                                   EXCEPINFO* pExcepInfo,
                                   unsigned int* puArgErr)
{
    TRACE_I("CImpIDispatch::Invoke dispID=" << dispID);
    if (!pDispParams)
        return E_INVALIDARG;

    switch (dispID)
    {
    case DISPID_DOWNLOADCOMPLETE:
    {
        if (!m_pSite->m_fOpening)
            m_pSite->m_fCanClose = TRUE;
        break;
    }

    case DISPID_TITLECHANGE:
    {
        if (pDispParams->rgvarg[0].vt == VT_BSTR)
        {
            char title[1024];
            title[0] = 0;

            WideCharToMultiByte(CP_ACP, 0, pDispParams->rgvarg[0].bstrVal, -1, title, 1024, NULL, NULL);
            title[1024 - 1] = 0;

            char locationURL[1024];
            locationURL[0] = 0;
            BSTR pbstrLocationURL;
            if (m_pSite->m_pIWebBrowser != NULL &&
                m_pSite->m_pIWebBrowser->get_LocationURL(&pbstrLocationURL) == S_OK)
            {
                WideCharToMultiByte(CP_ACP, 0, pbstrLocationURL, -1, locationURL, 1024, NULL, NULL);
                locationURL[1024 - 1] = 0;

                BOOL file = CanonizeURL(locationURL);

                char buff[3000];
                if (m_pSite->MarkdownFilename[0] != 0)
                    lstrcpy(buff, m_pSite->MarkdownFilename);
                else
                {
                    lstrcpy(buff, locationURL);
                    if (!file || lstrcmp(locationURL, title) != 0)
                        sprintf(buff + lstrlen(buff), " (%s)", title);
                }

                sprintf(buff + lstrlen(buff), " - %s", LoadStr(IDS_PLUGINNAME));

                SetWindowText(m_pSite->m_hParentWnd, buff);
            }
            m_pSite->DoVerb(OLEIVERB_UIACTIVATE);
        }
    }
    }

    return S_OK;
}

//***********************************************************************************
//
// CSite
//

CSite::CSite()
{
    TRACE_I("CSite::CSite");
    m_cRef = 0;
    m_fInitialized = FALSE;
    m_fCreated = FALSE;
    m_fCanClose = TRUE;
    m_fOpening = FALSE;
    m_hParentWnd = NULL;

    //Object interfaces
    m_pIUnknown = NULL;
    m_pIWebBrowser = NULL;
    m_pIWebBrowser2 = NULL; // !!! pozor - pro IE3.02 muze byt NULL
    m_pIOleObject = NULL;
    m_pIOleInPlaceObject = NULL;
    m_pIOleInPlaceActiveObject = NULL;
    m_pIViewObject = NULL;
    m_pConnectionPoint = NULL;

    //Our interfaces
    m_pImpIOleClientSite = NULL;
    m_pImpIOleInPlaceSite = NULL;
    m_pImpIOleInPlaceFrame = NULL;
    m_pImpIOleControlSite = NULL;
    //  m_pImpIAdviseSink = NULL;
    m_pImpIDispatch = NULL;

    MarkdownFilename[0] = 0;
}

CSite::~CSite()
{
    TRACE_I("CSite::~CSite");
    if (m_cRef != 0)
        TRACE_E("CSite::~CSite m_cRef == " << m_cRef);
}

//
// IUnknown methods for delegation
//

STDMETHODIMP CSite::QueryInterface(REFIID riid, void** ppvObj)
{
    CALL_STACK_MESSAGE1("CSite::QueryInterface(, )");
    TRACE_I("CSite::QueryInterface Interface " << GetStringFromIID(riid));
    *ppvObj = NULL;

    if (riid == IID_IUnknown)
        *ppvObj = this;

    if (riid == IID_IOleClientSite)
        *ppvObj = m_pImpIOleClientSite;

    if (riid == IID_IOleWindow || riid == IID_IOleInPlaceSite)
        *ppvObj = m_pImpIOleInPlaceSite;

    if (riid == IID_IOleInPlaceFrame)
        *ppvObj = m_pImpIOleInPlaceFrame;

    if (riid == IID_IOleControlSite)
        *ppvObj = m_pImpIOleControlSite;

    //  if (riid == IID_IAdviseSink)
    //    *ppvObj = m_pImpIAdviseSink;

    if (riid == DIID_DWebBrowserEvents || riid == IID_IDispatch)
        *ppvObj = m_pImpIDispatch;

    if (*ppvObj != NULL)
    {
        ((LPUNKNOWN)*ppvObj)->AddRef();
        return NOERROR;
    }

    //  TRACE_I("CSite::QueryInterface Unknown Interface "<<GetStringFromIID(riid));

    return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CSite::AddRef()
{
    //  TRACE_I("CSite::AddRef");
    return ++m_cRef;
}

STDMETHODIMP_(ULONG)
CSite::Release()
{
    CALL_STACK_MESSAGE1("CSite::Release()");
    //  TRACE_I("CSite::Release");
    if (m_cRef == 0)
        TRACE_E("CSite::Release m_cRef == 0");
    return --m_cRef;
}

//
// Our methods
//

BOOL CSite::ObjectInitialize()
{
    CALL_STACK_MESSAGE1("CSite::ObjectInitialize()");
    if (m_hParentWnd == NULL)
    {
        TRACE_E("CSite::ObjectInitialize Parent window must be created");
        return FALSE;
    }

    if (m_fInitialized)
    {
        TRACE_E("CSite::ObjectInitialize Object is alredy intialized");
        return FALSE;
    }

    m_pImpIOleClientSite = new CImpIOleClientSite(this, this);
    m_pImpIOleInPlaceSite = new CImpIOleInPlaceSite(this, this);
    m_pImpIOleInPlaceFrame = new CImpIOleInPlaceFrame(this, this);
    m_pImpIOleControlSite = new CImpIOleControlSite(this, this);
    //  m_pImpIAdviseSink = new CImpIAdviseSink(this, this);
    m_pImpIDispatch = new CImpIDispatch(this, this);

    if (m_pImpIOleClientSite == NULL || m_pImpIOleInPlaceSite == NULL ||
        m_pImpIOleInPlaceFrame == NULL || m_pImpIOleControlSite == NULL ||
        /*m_pImpIAdviseSink == NULL ||*/ m_pImpIDispatch == NULL)
    {
        TRACE_E("Low memory");
        return FALSE;
    }

    m_fInitialized = TRUE;
    return TRUE;
}

// tahle potvora chodi i s IE 3.02
static CLSID const my_CLSID_WebBrowser =
    {0xeab22ac3, 0x30c1, 0x11cf, {0xa7, 0xeb, 0x0, 0x0, 0xc0, 0x5b, 0xae, 0x0b}};

BOOL CSite::Create(HWND hParentWnd)
{
    CALL_STACK_MESSAGE1("CSite::Create()");
    if (m_hParentWnd != NULL)
        TRACE_E("CSite::Create m_hParentWnd != NULL");
    m_hParentWnd = hParentWnd;

    if (FAILED(OleInitialize(NULL)))
    {
        TRACE_E("OleInitialize failed");
        return FALSE;
    }

    if (!ObjectInitialize())
        return FALSE;

    // Necham vytvorit instance CLSID_WebBrowser a vytahnu IID_IUnknown
    HRESULT hr = CoCreateInstance(my_CLSID_WebBrowser, NULL,
                                  CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER,
                                  IID_IUnknown, (void**)&m_pIUnknown);

    if (hr == REGDB_E_CLASSNOTREG) // spravna varianta
    {
        TRACE_E("A InternetExplorer class is not registered in the registration database");
        return FALSE;
    }

    if (FAILED(hr))
    {
        TRACE_E("CoCreateInstance error");
        return FALSE;
    }

    // Casto potrebuji IID_IWebBrowser, tak si jeden vytahnu
    hr = m_pIUnknown->QueryInterface(IID_IWebBrowser, (void**)&m_pIWebBrowser);
    if (FAILED(hr))
    {
        TRACE_E("WebBrowser->QueryInterface failed on interface IID_IWebBrowser");
        return FALSE;
    }

    // zkusim, jestli neni k mani take IID_IWebBrowser2
    hr = m_pIUnknown->QueryInterface(IID_IWebBrowser2, (void**)&m_pIWebBrowser2);
    if (FAILED(hr))
    {
        TRACE_I("Neni IID_IWebBrowser2");
    }
    /*
  VARIANT_BOOL offline;
  m_pIWebBrowser2->get_Offline(&offline);

  m_pIWebBrowser2->put_Offline(-1);

  m_pIWebBrowser2->get_Offline(&offline);
*/
    // Casto potrebuji IOleObject, tak si jeden vytahnu
    hr = m_pIUnknown->QueryInterface(IID_IOleObject, (void**)&m_pIOleObject);
    if (FAILED(hr))
    {
        TRACE_E("WebBrowser->QueryInterface failed on interface IID_IOleObject");
        return FALSE;
    }

    // Casto potrebuji IOleInPlaceObject, tak si jeden vytahnu
    hr = m_pIUnknown->QueryInterface(IID_IOleInPlaceObject, (void**)&m_pIOleInPlaceObject);
    if (FAILED(hr))
    {
        TRACE_E("WebBrowser->QueryInterface failed on interface IID_IOleInPlaceObject");
        return FALSE;
    }

    // Casto potrebuji IID_IOleInPlaceActiveObject, tak si jeden vytahnu
    hr = m_pIUnknown->QueryInterface(IID_IOleInPlaceActiveObject, (void**)&m_pIOleInPlaceActiveObject);
    if (FAILED(hr))
    {
        TRACE_E("WebBrowser->QueryInterface failed on interface IID_IOleInPlaceActiveObject");
        return FALSE;
    }

    // Casto potrebuji IID_IViewObject, tak si jeden vytahnu
    hr = m_pIUnknown->QueryInterface(IID_IViewObject, (void**)&m_pIViewObject);
    if (FAILED(hr))
    {
        TRACE_E("WebBrowser->QueryInterface failed on interface IID_IViewObject");
        return FALSE;
    }
    /*
  if (FAILED(hr = m_pIViewObject->SetAdvise(DVASPECT_CONTENT, ADVF_PRIMEFIRST,
                                            m_pImpIAdviseSink)))
  {
    TRACE_E("SetAdvise on WebBrowser failed.");
    return FALSE;
  }
*/
    // Nastavim objektu ClientSite
    if (FAILED(hr = m_pIOleObject->SetClientSite(m_pImpIOleClientSite)))
    {
        TRACE_E("SetClientSite on WebBrowser failed.");
        return FALSE;
    }

    GetClientRect(m_hParentWnd, &m_rect);

    SIZE size;
    size.cx = m_rect.right - m_rect.left;
    size.cy = m_rect.bottom - m_rect.top;

    hr = m_pIOleObject->SetExtent(DVASPECT_CONTENT, (SIZEL*)&size);
    if (FAILED(hr))
        TRACE_E("m_pIOleObject->SetExtent failed");

    hr = m_pIOleObject->DoVerb(OLEIVERB_INPLACEACTIVATE,
                               NULL, m_pImpIOleClientSite, 0,
                               m_hParentWnd, &m_rect);
    if (FAILED(hr))
        TRACE_E("m_pIOleObject->DoVerb failed");

    hr = m_pIOleInPlaceObject->SetObjectRects(&m_rect, &m_rect);
    if (FAILED(hr))
        TRACE_E("m_pIOleInPlaceObject->SetObjectRects failed");

    ConnectEvents();

    m_fCreated = TRUE;
    m_fOpening = TRUE;
    return TRUE;
}

void CSiteCloseAux()
{
    __try
    {
        OleUninitialize();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void CSite::Close()
{
    CALL_STACK_MESSAGE1("CSite::Close()");
    TRACE_I("CSite::Close()");
    HRESULT hr;

    m_fCreated = FALSE;

    // deaktivuju InPlaceObject
    if (m_pIOleInPlaceObject != NULL)
    {
        hr = m_pIOleInPlaceObject->InPlaceDeactivate();
        if (FAILED(hr))
            TRACE_E("m_pIOleInPlaceObject->InPlaceDeactivate failed");
        // uvolnim interface
        ReleaseInterface(m_pIOleInPlaceObject);
    }

    DisconnectEvents();
    /*
  if (m_pIViewObject != NULL)
  {
    if (FAILED(hr = m_pIViewObject->SetAdvise(DVASPECT_CONTENT, ADVF_PRIMEFIRST, NULL)))
      TRACE_E("SetAdvise on WebBrowser failed.");
  }
*/
    // zavru OleObject
    if (m_pIOleObject != NULL)
    {
        hr = m_pIOleObject->SetClientSite(NULL);
        if (FAILED(hr))
            TRACE_E("m_pIOleObject->SetClientSite failed");
        hr = m_pIOleObject->Close(OLECLOSE_NOSAVE);
        if (FAILED(hr))
            TRACE_E("m_pIOleObject->Close failed");
        // uvolnim interface
        ReleaseInterface(m_pIOleObject);
    }

    // uvolnim pomocne interfacy
    ReleaseInterface(m_pIUnknown);
    ReleaseInterface(m_pIWebBrowser);
    if (m_pIWebBrowser2 != NULL)
        ReleaseInterface(m_pIWebBrowser2);
    ReleaseInterface(m_pIOleInPlaceActiveObject);
    ReleaseInterface(m_pIViewObject);

    // uvolnim svoje interfacy
    DeleteInterfaceImp(m_pImpIOleClientSite);
    DeleteInterfaceImp(m_pImpIOleInPlaceSite);
    DeleteInterfaceImp(m_pImpIOleInPlaceFrame);
    DeleteInterfaceImp(m_pImpIOleControlSite);
    //  DeleteInterfaceImp(m_pImpIAdviseSink);
    DeleteInterfaceImp(m_pImpIDispatch);

    CSiteCloseAux();
}

HRESULT CSite::DoVerb(LONG iVerb)
{
    CALL_STACK_MESSAGE2("CSite::DoVerb(%d)", iVerb);
    if (m_fCreated)
        return m_pIOleObject->DoVerb(OLEIVERB_UIACTIVATE, NULL,
                                     m_pImpIOleClientSite, 0,
                                     m_hParentWnd, &m_rect);
    return OLEOBJ_S_CANNOT_DOVERB_NOW;
}

void CSite::ConnectEvents()
{
    CALL_STACK_MESSAGE1("CSite::ConnectEvents()");

    IConnectionPointContainer* pCPContainer;
    // Step 1: Get a pointer to the connection point container
    HRESULT hr = m_pIWebBrowser->QueryInterface(IID_IConnectionPointContainer,
                                                (void**)&pCPContainer);
    if (SUCCEEDED(hr))
    {
        // m_pConnectionPoint is defined like this:
        // IConnectionPoint* m_pConnectionPoint;

        // Step 2: Find the connection point
        hr = pCPContainer->FindConnectionPoint(DIID_DWebBrowserEvents, &m_pConnectionPoint);
        if (SUCCEEDED(hr))
        {
            // Step 3: Advise
            hr = m_pConnectionPoint->Advise(this, &m_dwCookie);
            //       if (FAILED(hr))
            //       {
            //         ::MessageBox(NULL, "Failed to Advise", "C++ Event Sink", MB_OK);
            //       }
        }

        pCPContainer->Release();
    }
}

void CSite::DisconnectEvents()
{
    CALL_STACK_MESSAGE1("CSite::DisconnectEvents()");
    // Step 5: Unadvise
    if (m_pConnectionPoint)
    {
        HRESULT hr = m_pConnectionPoint->Unadvise(m_dwCookie);
        //    if (FAILED(hr))
        //    {
        //      ::MessageBox(NULL, "Failed to Unadvise", "C++ Event Sink", MB_OK);
        //    }
    }
}

//***********************************************************************************
//
// CIEWindow
//

BOOL CIEWindow::CreateSite(HWND hParent)
{
    CALL_STACK_MESSAGE1("CIEWindow::CreateSite()");
    TRACE_I("CIEWindow::CreateSite()");
    if (!m_Site.Create(hParent))
        return FALSE;

    HWND hwnd;
    HRESULT hr = m_Site.m_pIOleInPlaceObject->GetWindow(&hwnd);
    if (FAILED(hr))
    {
        TRACE_E("m_pIOleInPlaceObject->GetWindow failed");
        return FALSE;
    }
    HWindow = hwnd;
    return TRUE;
}

void CIEWindow::CloseSite()
{
    CALL_STACK_MESSAGE1("CIEWindow::CloseSite()");
    TRACE_I("CIEWindow::CloseSite()");
    m_Site.Close();
}

HRESULT LoadWebBrowserFromStream(IWebBrowser* pWebBrowser, IStream* pStream)
{
    HRESULT hr;
    IDispatch* pHtmlDoc = NULL;
    IPersistStreamInit* pPersistStreamInit = NULL;

    // Retrieve the document object.
    hr = pWebBrowser->get_Document(&pHtmlDoc);
    if (SUCCEEDED(hr))
    {
        // Query for IPersistStreamInit.
        hr = pHtmlDoc->QueryInterface(IID_IPersistStreamInit, (void**)&pPersistStreamInit);
        if (SUCCEEDED(hr))
        {
            // Initialize the document.
            hr = pPersistStreamInit->InitNew();
            if (SUCCEEDED(hr))
            {
                // Load the contents of the stream.
                hr = pPersistStreamInit->Load(pStream);
            }
            else
            {
                TRACE_E("pPersistStreamInit->InitNew() failed");
            }
            pPersistStreamInit->Release();
        }
        else
        {
            TRACE_E("QueryInterface() on IID_IPersistStreamInit failed");
        }
        pHtmlDoc->Release();
    }
    else
    {
        TRACE_E("get_Document() failed");
    }
    return 0;
}

void NavigateAux(CSite* m_pSite, const char* fileName, IStream* contentStream)
{
    HRESULT hr = m_pSite->m_pIWebBrowser2->Navigate(_bstr_t("about:blank"), NULL, NULL, NULL, NULL);
    // hack - misto pomerne slozitejsiho reseni pres event DWebBrowserEvents2::DocumentComplete
    // viz https://msdn.microsoft.com/en-us/library/aa752047%28v=vs.85%29.aspx
    // proste pockame, az zacne browser vracet READYSTATE == READYSTATE_COMPLETE
    // viz https://support.microsoft.com/en-us/kb/180366 a http://www.dsource.org/forums/viewtopic.php?t=2953
    READYSTATE rs;
    do
    {
        // musime pumpovat zpravy, jinak vraci get_ReadyState() porad READYSTATE_LOADING
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        m_pSite->m_pIWebBrowser2->get_ReadyState(&rs);
    } while (rs != READYSTATE_COMPLETE);
    strcpy(m_pSite->MarkdownFilename, fileName);
    // Call the helper function to load the browser from the stream.
    LoadWebBrowserFromStream(m_pSite->m_pIWebBrowser, contentStream);
    contentStream->Release();
}

void CIEWindow::Navigate(LPCTSTR lpszURL, IStream* contentStream)
{
    CALL_STACK_MESSAGE2("CIEWindow::Navigate(%s)", lpszURL);

    if (contentStream != NULL && m_Site.m_pIWebBrowser2 != NULL)
    {
        NavigateAux(&m_Site, lpszURL, contentStream);
        return;
    }

    OLECHAR szTemp[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lpszURL, -1, szTemp, MAX_PATH);
    szTemp[MAX_PATH - 1] = 0;

    VARIANT vURL;
    vURL.vt = VT_BSTR;
    vURL.bstrVal = szTemp;

    VARIANT vHeaders;
    vHeaders.vt = VT_BSTR;
    vHeaders.bstrVal = NULL;

    VARIANT vTargetFrameName;
    vTargetFrameName.vt = VT_BSTR;
    vTargetFrameName.bstrVal = NULL;

    VARIANT vPostData;
    vPostData.vt = VT_I4;
    vPostData.lVal = 0;

    VARIANT vFlags;
    vTargetFrameName.vt = VT_I4;
    vTargetFrameName.lVal = navNoHistory;

    m_Site.m_fCanClose = TRUE;
    const char* ext = strrchr(lpszURL, '.');
    if (ext != NULL && _stricmp(ext + 1, "xml") == 0)
        m_Site.m_fCanClose = FALSE;
    m_Site.m_fOpening = TRUE;
    HRESULT hr = m_Site.m_pIWebBrowser->Navigate(vURL.bstrVal, &vFlags, &vTargetFrameName, &vPostData, &vHeaders);
    if (hr == E_INVALIDARG)
        TRACE_E("m_Site.m_pIWebBrowser->Navigate failed: E_INVALIDARG");
    else if (hr == E_OUTOFMEMORY)
        TRACE_E("m_Site.m_pIWebBrowser->Navigate failed: E_OUTOFMEMORY");
    m_Site.m_fOpening = FALSE;
}

BOOL CIEWindow::CanClose()
{
    return m_Site.m_fCanClose;
}

HRESULT CIEWindow::TranslateAccelerator(LPMSG lpmsg)
{
    CALL_STACK_MESSAGE1("CIEWindow::TranslateAccelerator()");
    // odchytim klavesu ESCAPE a zavru viewer
    if (lpmsg->message == WM_KEYDOWN && lpmsg->wParam == VK_ESCAPE)
    {
        TRACE_I("Posting WM_CLOSE");
        PostMessage(m_Site.m_hParentWnd, WM_CLOSE, 0, 0);
        return S_OK;
    }

    if (m_Site.m_pIOleInPlaceActiveObject != NULL)
        return m_Site.m_pIOleInPlaceActiveObject->TranslateAccelerator(lpmsg);
    else
        return S_FALSE;
}

//
// ****************************************************************************
// CIEMainWindowQueue
//

CIEMainWindowQueue::~CIEMainWindowQueue()
{
    if (!Empty())
        TRACE_E("Nejake okno viewru zustalo otevrene!");
    // tady uz multi-threadovost nehrozi (konci plugin, thready jsou/byly ukonceny)
    // dealokujeme aspon nejakou pamet
    CIEMainWindowQueueItem* last;
    CIEMainWindowQueueItem* item = Head;
    while (item != NULL)
    {
        last = item;
        item = item->Next;
        delete last;
    }
}

BOOL CIEMainWindowQueue::Add(CIEMainWindowQueueItem* item)
{
    CALL_STACK_MESSAGE1("CIEMainWindowQueue::Add()");
    CS.Enter();
    if (item != NULL)
    {
        item->Next = Head;
        Head = item;
        CS.Leave();
        return TRUE;
    }
    CS.Leave();
    return FALSE;
}

void CIEMainWindowQueue::Remove(HWND hWindow)
{
    CALL_STACK_MESSAGE1("CIEMainWindowQueue::Remove()");
    CS.Enter();
    CIEMainWindowQueueItem* last = NULL;
    CIEMainWindowQueueItem* item = Head;
    while (item != NULL)
    {
        if (item->HWindow == hWindow) // nalezeno, odstranime
        {
            if (last != NULL)
                last->Next = item->Next;
            else
                Head = item->Next;
            delete item;
            CS.Leave();
            return;
        }
        last = item;
        item = item->Next;
    }
    CS.Leave();
}

BOOL CIEMainWindowQueue::Empty()
{
    BOOL e;
    CS.Enter();
    e = Head == NULL;
    CS.Leave();
    return e;
}

BOOL CIEMainWindowQueue::CloseAllWindows(BOOL force, int waitTime, int forceWaitTime)
{
    CALL_STACK_MESSAGE4("CIEMainWindowQueue::CloseAllWindows(%d, %d, %d)", force, waitTime, forceWaitTime);
    // posleme zadost o zavreni vsech oken
    CS.Enter();
    CIEMainWindowQueueItem* item = Head;
    while (item != NULL)
    {
        PostMessage(item->HWindow, WM_CLOSE, 0, 0);
        item = item->Next;
    }
    CS.Leave();

    // pockame az/jestli se zavrou
    DWORD ti = GetTickCount();
    DWORD w = force ? forceWaitTime : waitTime;
    while ((w == INFINITE || w > 0) && !Empty())
    {
        DWORD t = GetTickCount() - ti;
        if (w == INFINITE || t < w) // mame jeste cekat
        {
            if (w == INFINITE || 50 < w - t)
                Sleep(50);
            else
            {
                Sleep(w - t);
                break;
            }
        }
        else
            break;
    }
    return force || Empty();
}

//
// ****************************************************************************
// CIEMainWindow
//

CIEMainWindow::CIEMainWindow()
{
    HWindow = NULL;
    Lock = NULL;
}

LRESULT
CIEMainWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CIEMainWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        if (!m_IEViewer.CreateSite(HWindow))
            return -1;
        ShowWindow(m_IEViewer.HWindow, SW_SHOW);
        return 0;
    }

    case WM_CLOSE:
    {
        if (!m_IEViewer.CanClose())
            return 0;
        break;
    }

    case WM_DESTROY:
    {
        TRACE_I("CIEMainWindow::WindowProc WM_DESTROY");
        if (Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL;
        }
        TRACE_I("CIEMainWindow::WindowProc m_IEViewer.CloseSite()");
        m_IEViewer.CloseSite();
        TRACE_I("CIEMainWindow::WindowProc PostQuitMessage");
        PostQuitMessage(0);
        break;
    }

    case WM_SETFOCUS:
    {
        HWND hWnd = m_IEViewer.HWindow;
        HWND hIterator = hWnd;
        do
        {
            hIterator = GetWindow(hIterator, GW_CHILD);
            if (hIterator != NULL)
                hWnd = hIterator;
        } while (hIterator != NULL);
        SetFocus(hWnd);
        return 0;
    }

    case WM_ACTIVATE:
    {
        if (!LOWORD(wParam))
        {
            // hlavni okno pri prepnuti z viewru nebude delat refresh
            SalamanderGeneral->SkipOneActivateRefresh();
        }
        break;
    }

    case WM_SIZE:
    {
        if (m_IEViewer.HWindow != NULL)
            SetWindowPos(m_IEViewer.HWindow, HWND_TOP,
                         0, 0, LOWORD(lParam), HIWORD(lParam),
                         0);
        break;
    }
    }
    return DefWindowProc(HWindow, uMsg, wParam, lParam);
}

HANDLE
CIEMainWindow::GetLock()
{
    if (Lock == NULL)
        Lock = CreateEvent(NULL, FALSE, FALSE, NULL);
    return Lock;
}

// ****************************************************************************
// staticka funkce CIEMainWindow::CIEMainWindowProc pro vsechny message vsech oken
// viewru, odtud probiha distribuce messages do jednotlivych oken viewru - metoda
// CIEMainWindow::WindowProc (vhodne misto pro zpracovani zprav jednotlivych oken)

LRESULT CALLBACK
CIEMainWindow::CIEMainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE5("CIEMainWindow::CIEMainWindowProc(0x%p, 0x%X, 0x%IX, 0x%IX)", hwnd, uMsg, wParam, lParam);
    CIEMainWindow* wnd;
    switch (uMsg)
    {
    case WM_CREATE: // prvni zprava - pripojeni objektu k oknu
    {
        wnd = (CIEMainWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        if (wnd == NULL)
        {
            TRACE_E("Chyba pri vytvareni okna.");
            return FALSE;
        }
        else
        {
            wnd->HWindow = hwnd;
            SetProp(hwnd, (LPCTSTR)AtomObject, (HANDLE)wnd);
            ViewerWindowQueue.Add(new CIEMainWindowQueueItem(wnd->HWindow));
        }
        break;
    }

    case WM_DESTROY: // posledni zprava - odpojeni objektu od okna
    {
        wnd = (CIEMainWindow*)GetProp(hwnd, (LPCTSTR)AtomObject);
        if (wnd != NULL)
        {
            LRESULT res = wnd->WindowProc(uMsg, wParam, lParam);
            ViewerWindowQueue.Remove(hwnd);
            RemoveProp(hwnd, (LPCTSTR)AtomObject);
            //        delete wnd;
            if (res == 0)
                return 0; // aplikace ji zpracovala
            wnd = NULL;
        }
        break;
    }

    default:
    {
        wnd = (CIEMainWindow*)GetProp(hwnd, (LPCTSTR)AtomObject);
    }
    }
    //--- zavolani metody WindowProc(...) prislusneho objektu okna
    if (wnd != NULL)
        return wnd->WindowProc(uMsg, wParam, lParam);
    else
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
