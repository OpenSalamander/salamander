// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// DiskMapPlugin.cpp : Defines the entry point for the DLL application.
//

#include "precomp.h"
#include "DiskMapPlugin.h"

#include "../DiskMap/GUI.MainWindow.h"

//for plugin registration... not translatable?
#define PLUGIN_NAME_EN "DiskMap" //non-translated plugin name, used before loading the language module + for debug purposes
#define PLUGIN_FILE "DISKMAP"    //registry key

int SalamanderVersion;

CPluginInterface PluginInterface;
CPluginInterfaceForMenuExt InterfaceForMenuExt;

CMyThreadQueue ThreadQueue("DiskMap Windows and Workers");

BOOL Config_CloseConfirmation = TRUE;
BOOL Config_ShowFolders = TRUE;
BOOL Config_ShowTooltip = TRUE;
int Config_PathFormat = 2;
const char* CONFIG_CLOSECONFIRMATION = "Confirm ESC Close";
const char* CONFIG_SHOWFOLDERS = "Highlight Folders";
const char* CONFIG_SHOWTOOLTIP = "Display Tooltip";
const char* CONFIG_PATHFORMAT = "Tooltip Path Format";

char* LoadStr(int resID);

TCHAR szPluginWebsite[] = TEXT("www.altap.cz"); // original domain not running: http://salamander.diskmap.net

HINSTANCE DLLInstance = NULL; // handle to SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to SLG - language-dependent resources

HACCEL hAccelTable = NULL;

// general Salamander interface - valid from the moment the plugin starts until it shuts down
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// interface providing custom Windows controls used in Salamander
CSalamanderGUIAbstract* SalamanderGUI = NULL;

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DLLInstance = hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

class CSalamanderCallback : public CSalamanderCallbackAbstract
{
    TCHAR FocusPathBuf[MAX_PATH];

public:
    BOOL FocusFile(TCHAR const* fileName)
    {
        if (SalamanderGeneral->SalamanderIsNotBusy(NULL))
        {
            lstrcpyn(FocusPathBuf, fileName, MAX_PATH);
            SalamanderGeneral->PostMenuExtCommand(MENUCMD_FAKE_FOCUS, TRUE);
            Sleep(500);          // the switch to the panel happens, so this wait occurs in the viewer's inactive window and therefore does not matter
            FocusPathBuf[0] = 0; // after 0.5 second we no longer care about the focus (handles the case where we hit the beginning of Salamander's BUSY mode)
            return TRUE;
        }
        //TODO Error reporting
        return FALSE;
    }
    BOOL DoFocusFile()
    {
        char focusPath[MAX_PATH];
        lstrcpyn(focusPath, FocusPathBuf, MAX_PATH);
        FocusPathBuf[0] = 0;
        if (focusPath[0] != 0) // only if we were not unlucky (we did not hit the beginning of Salamander's BUSY mode)
        {
            LPTSTR name;
            if (SalamanderGeneral->CutDirectory(focusPath, &name))
            {
                SalamanderGeneral->SkipOneActivateRefresh(); // the main window will not refresh when switching from the viewer
                SalamanderGeneral->FocusNameInPanel(PANEL_SOURCE, focusPath, name);
                return TRUE;
            }
        }
        return FALSE;
    }
    BOOL DoOpenFolder()
    {
        char focusPath[MAX_PATH];
        lstrcpyn(focusPath, FocusPathBuf, MAX_PATH);
        FocusPathBuf[0] = 0;
        if (focusPath[0] != 0) // only if we were not unlucky (we did not hit the beginning of Salamander's BUSY mode)
        {
            SalamanderGeneral->SkipOneActivateRefresh(); // the main window will not refresh when switching from the viewer
            //SalamanderGeneral->ChangePanelPath(PANEL_SOURCE, focusPath);
            SalamanderGeneral->FocusNameInPanel(PANEL_SOURCE, focusPath, "");
            return TRUE;
        }
        return FALSE;
    }
    BOOL OpenFolder(TCHAR const* path)
    {
        if (SalamanderGeneral->SalamanderIsNotBusy(NULL))
        {
            lstrcpyn(FocusPathBuf, path, MAX_PATH);
            SalamanderGeneral->PostMenuExtCommand(MENUCMD_FAKE_OPEN, TRUE);
            Sleep(500);          // the switch to the panel happens, so this wait occurs in the viewer's inactive window and therefore does not matter
            FocusPathBuf[0] = 0; // after 0.5 second we no longer care about the focus (handles the case where we hit the beginning of Salamander's BUSY mode)
            return TRUE;
        }
        return FALSE;
    }
    BOOL CanOpenFolder()
    {
        return SalamanderGeneral->SalamanderIsNotBusy(NULL);
    }
    //config Properties
    void SetCloseConfirm(BOOL value) { Config_CloseConfirmation = value; }
    BOOL GetCloseConfirm() { return Config_CloseConfirmation; }

    void SetShowFolders(BOOL value) { Config_ShowFolders = value; }
    BOOL GetShowFolders() { return Config_ShowFolders; }

    void SetShowTooltip(BOOL value) { Config_ShowTooltip = value; }
    BOOL GetShowTooltip() { return Config_ShowTooltip; }

    void SetPathFormat(int value) { Config_PathFormat = value; }
    int GetPathFormat() { return Config_PathFormat; }

    BOOL ConfirmClose(HWND parent)
    {
        if (Config_CloseConfirmation == FALSE)
            return TRUE;

        BOOL checkstate = FALSE;
        MSGBOXEX_PARAMS msgpars;
        memset(&msgpars, 0, sizeof(msgpars));
        msgpars.HParent = parent;
        msgpars.Caption = LoadStr(IDS_PLUGIN_NAME);
        msgpars.Text = LoadStr(IDS_CLOSE_CONFIRMATION);
        msgpars.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_DEFBUTTON1 | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_SILENT;
        msgpars.HIcon = NULL;
        msgpars.ContextHelpId = 0;
        msgpars.HelpCallback = NULL;
        msgpars.CheckBoxText = LoadStr(IDS_CLOSE_CONFIRMATION_CHECKBOX);
        msgpars.CheckBoxValue = &checkstate;
        msgpars.AliasBtnNames = NULL; //TODO: better buttons texts
        BOOL ret = (SalamanderGeneral->SalMessageBoxEx(&msgpars) == DIALOG_YES);
        if (checkstate == TRUE)
        {
            this->SetCloseConfirm(FALSE);
        }
        return ret;
    }
    BOOL CanFocusFile()
    {
        return SalamanderGeneral->SalamanderIsNotBusy(NULL);
    }
    void About(HWND hWndParent)
    {
        PluginInterface.About(hWndParent);
    }
    int GetClusterSize(TCHAR const* path)
    {
        DWORD SectorsPerCluster = 0;
        DWORD BytesPerSector = 0;
        DWORD NumberOfFreeClusters = 0;
        DWORD TotalNumberOfClusters = 0;
        if (!SalamanderGeneral->SalGetDiskFreeSpace(path, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters))
        {
            return 512;
        }
        return BytesPerSector * SectorsPerCluster;
    }
};

CSalamanderCallback SalamanderCallback;
// ****************************************************************************

char* LoadStr(int resID)
{
    static char buffer[5000]; // buffer for many strings
    static char* act = buffer;

    //HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    if (5000 - (act - buffer) < 200)
        act = buffer;

RELOAD:
    int size = LoadString(HLanguage, resID, act, 5000 - (int)(act - buffer));
    // size contains the number of copied characters without the terminator
    //  DWORD error = GetLastError();
    char* ret;
    if (size != 0 /* || error == NO_ERROR*/) // error is NO_ERROR even when the string does not exist - unusable
    {
        if ((5000 - (act - buffer) == size + 1) && (act > buffer))
        {
            // if the string was exactly at the end of the buffer, it could
            // be a truncated string -- if we can move the window
            // to the beginning of the buffer, load the string once again
            act = buffer;
            goto RELOAD;
        }
        else
        {
            ret = act;
            act += size + 1;
        }
    }
    else
    {
        //TRACE_E("Error in LoadStr(" << resID << ")." /*"): " << GetErrorText(error)*/);
        static char errorBuff[] = "ERROR LOADING STRING";
        ret = errorBuff;
    }

    //HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

int WINAPI SalamanderPluginGetReqVer()
{
#ifdef OPENSAL_VERSION // version built for Salamander distribution will not be backward compatible with version 4.0 (unnecessary complication)
    return LAST_VERSION_OF_SALAMANDER;
#else  // OPENSAL_VERSION
    return SALSDK_COMPATIBLE_WITH_VER;
#endif // OPENSAL_VERSION
}

int WINAPI SalamanderPluginGetSDKVer()
{
    return LAST_VERSION_OF_SALAMANDER; // return current SDK version
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();
    //HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

#ifdef OPENSAL_VERSION // version built for Salamander distribution will not be backward compatible with version 4.0 (unnecessary complication)
    // this plugin is made for the current Salamander version and higher - perform a check
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(), REQUIRE_LAST_VERSION_OF_SALAMANDER, PLUGIN_NAME_EN, MB_OK | MB_ICONERROR);
        return NULL;
    }
#else  // OPENSAL_VERSION
       // this plugin is made for Salamander 4.0 and higher - perform a check
    if (SalamanderVersion < SALSDK_COMPATIBLE_WITH_VER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(), REQUIRE_COMPATIBLE_SAL_VERSION, PLUGIN_NAME_EN, MB_OK | MB_ICONERROR);
        return NULL;
    }
#endif // OPENSAL_VERSION

    //TODO: let the language module (.slg) load
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), PLUGIN_NAME_EN);
    if (HLanguage == NULL)
    {
        //MessageBox(salamander->GetParentWindow(), "SLG not found...", PLUGIN_NAME_EN, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // obtain the general Salamander interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    // obtain the interface providing customized Windows controls used in Salamander
    SalamanderGUI = salamander->GetSalamanderGUI();

    // set the name of the help file
    SalamanderGeneral->SetHelpFileName("diskmap.chm");

    CWindow::SetHInstance(DLLInstance, HLanguage);

    if (!CMainWindow::RegisterClass())
    {
        MessageBox(salamander->GetParentWindow(),
                   TEXT("RegisterClassEx() failed"),
                   LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONERROR);
        return NULL;
    }

    CMainWindow::LoadResourceStrings();

    // set basic information about the plugin
    salamander->SetBasicPluginData(
        LoadStr(IDS_PLUGIN_NAME),
        FUNCTION_LOADSAVECONFIGURATION, //no functions :-P
        VERSINFO_VERSION_NO_PLATFORM, VERSINFO_COPYRIGHT,
        LoadStr(IDS_PLUGIN_DESCRIPTION),
        PLUGIN_FILE, //regKeyName
        NULL,        //extensions
        NULL         //fsName
    );

    // set the plugin home page URL
    salamander->SetPluginHomePageURL(szPluginWebsite);

    // only needed for the message loop in the plugin - windows do not need it
    hAccelTable = LoadAccelerators(HLanguage, MAKEINTRESOURCE(IDC_ZAREVAKDISKMAP));

    return &PluginInterface;
}

void WINAPI CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _sntprintf(buf, 1000,
               "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
               "%s",
               LoadStr(IDS_PLUGIN_NAME),
               LoadStr(IDS_PLUGIN_DESCRIPTION));
    buf[999] = TEXT('\0');
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_PLUGIN_ABOUT), MB_OK | MB_ICONINFORMATION);
}

void WINAPI CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    //CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /* TODO */ //Petr: now it is always Windows 2000, XP, Vista (there used to be a condition for W2K+)

    HICON icon = LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_ZAREVAKDISKMAP));
    if (icon != NULL)
    {
        CGUIIconListAbstract* iconList = SalamanderGUI->CreateIconList();
        if (iconList != NULL)
        {
            iconList->Create(16, 16, 1);
            iconList->ReplaceIcon(0, icon);
            salamander->SetIconListForGUI(iconList);
            salamander->SetPluginIcon(0);
            salamander->SetPluginMenuAndToolbarIcon(0);
        }
        DestroyIcon(icon);
    }
    /* used by the export_mnu.py script, which generates salmenu.mnu for Translator
   keep synchronized with the salamander->AddMenuItem() calls below...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_PLUGIN_MENU
	{MNTT_PE, 0
};
*/
    //('eventMask' & 'state_or') != 0 && ('eventMask' & 'state_and') == 'state_and',
    salamander->AddMenuItem(
        -1,                                              //icon
        LoadStr(IDS_PLUGIN_MENU),                        //text
        SALHOTKEY('D', HOTKEYF_CONTROL | HOTKEYF_SHIFT), //hotkey
        MENUCMD_OPEN,                                    //id
        FALSE,                                           //callGetState
        MENU_EVENT_TRUE,                                 //OR mask
        MENU_EVENT_DISK,                                 //AND mask
        MENU_SKILLLEVEL_ALL);
}

void WINAPI CPluginInterface::Event(int event, DWORD param)
{
}

CPluginInterfaceForMenuExtAbstract* WINAPI CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

void WINAPI CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    if (regKey != NULL) //loading from the registry
    {
        registry->GetValue(regKey, CONFIG_CLOSECONFIRMATION, REG_DWORD, &Config_CloseConfirmation, sizeof(Config_CloseConfirmation));
        registry->GetValue(regKey, CONFIG_SHOWFOLDERS, REG_DWORD, &Config_ShowFolders, sizeof(Config_ShowFolders));
        registry->GetValue(regKey, CONFIG_SHOWTOOLTIP, REG_DWORD, &Config_ShowTooltip, sizeof(Config_ShowTooltip));
        registry->GetValue(regKey, CONFIG_PATHFORMAT, REG_DWORD, &Config_PathFormat, sizeof(Config_PathFormat));
    }
}

void WINAPI CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    registry->SetValue(regKey, CONFIG_CLOSECONFIRMATION, REG_DWORD, &Config_CloseConfirmation, sizeof(Config_CloseConfirmation));
    registry->SetValue(regKey, CONFIG_SHOWFOLDERS, REG_DWORD, &Config_ShowFolders, sizeof(Config_ShowFolders));
    registry->SetValue(regKey, CONFIG_SHOWTOOLTIP, REG_DWORD, &Config_ShowTooltip, sizeof(Config_ShowTooltip));
    registry->SetValue(regKey, CONFIG_PATHFORMAT, REG_DWORD, &Config_PathFormat, sizeof(Config_PathFormat));
}

struct TThreadInfoItem
{
    HANDLE Thread;
    HWND MainWindow;
    TThreadInfoItem* Next;
    TThreadInfoItem() { Clear(); }
    void Clear()
    {
        Thread = 0;
        MainWindow = 0;
        Next = NULL;
    }
};

TThreadInfoItem BaseThreadItem;
TThreadInfoItem* FirstThreadItem = &BaseThreadItem;

// variant of starting the viewer thread without using the CThread object
struct CTVData
{
    BOOL AlwaysOnTop;
    const char* Name;
    int Left, Top, Width, Height;
    UINT ShowCmd;
    TThreadInfoItem* ThreadInfo;
    //BOOL ReturnLock;
    //HANDLE *Lock;
    //BOOL *LockOwner;
    BOOL Success;
    HANDLE Continue;
};

unsigned int WINAPI WindowThreadBody(void* param)
{
    CALL_STACK_MESSAGE1("WindowThreadBody()");
    TraceAttachCurrentThread();
    SetThreadNameInVCAndTrace(PLUGIN_NAME_EN);
    TRACE_I("Begin");

    // example of an application crash
    //  int *p = 0;
    //  *p = 0;       // ACCESS VIOLATION !

    CTVData* data = (CTVData*)param;

    CMainWindow* window = new CMainWindow(NULL);
    if (window == NULL)
    {
        SetEvent(data->Continue); // let the main thread continue, from this point the data are no longer valid (=NULL)
        data = NULL;

        TRACE_I("CMainWindow Failed");
        return 1;
    }

    CoInitialize(NULL);

    if (window->Create(data->Left, data->Top, data->Width, data->Height, data->AlwaysOnTop) == NULL)
    {
        //TRACE_I("Notcreated");
        MessageBoxA(NULL, "DiskMap window could not be created", PLUGIN_NAME_EN, 0);

        // free the window memory
        delete window;

        CoUninitialize();

        TRACE_I("End");
        return 2;
    }

    //CALL_STACK_MESSAGE1("ViewerThreadBody::ShowWindow");
    //TRACE_I("SetPath");
    window->SetCallback(&SalamanderCallback);
    window->Show(data->ShowCmd);
    data->Success = window->SetPath(data->Name, FALSE);

    TThreadInfoItem* MyThreadInfo = data->ThreadInfo;

    if (MyThreadInfo != NULL)
    {
        MyThreadInfo->MainWindow = window->GetHandle();
    }

    //CALL_STACK_MESSAGE1("ViewerThreadBody::SetEvent");
    SetEvent(data->Continue); // let the main thread continue, from this point the data are no longer valid (=NULL)
    data = NULL;

    // if everything succeeded, start searching for files
    //CALL_STACK_MESSAGE1("ViewerThreadBody::OpenFile");
    window->PopulateStart();

    //CALL_STACK_MESSAGE1("ViewerThreadBody::message-loop");
    //TRACE_I("MsgLoop");
    // message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        //if (!window->IsMenuBarMessage(&msg))
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (MyThreadInfo != NULL)
    {
        // report the closed window back
        MyThreadInfo->MainWindow = 0;
    }

    // free the window memory
    delete window;

    CoUninitialize();

    TRACE_I("End");
    return 0;
}
DWORD WINAPI WindowThreadBodyTrace(void* param)
{
    return SalamanderDebug->CallWithCallStack(WindowThreadBody, param);
}

BOOL OpenDiskMapWindow(HWND parent, char* name)
{
    TThreadInfoItem* NewThreadInfo = NULL;
    TThreadInfoItem* tmpThreadInfo = FirstThreadItem;

    // find the first unused one
    while (true) //tmpThreadInfo->Next != NULL)
    {
        // only for those that do not currently have a window displayed
        if (tmpThreadInfo->MainWindow == 0)
        {
            // the thread has never existed
            if (tmpThreadInfo->Thread == 0)
            {
                NewThreadInfo = tmpThreadInfo;
                break;
            }
            else
            {
                if (ThreadQueue.WaitForExit(tmpThreadInfo->Thread, 0))
                {
                    // can be used if the thread is no longer running...
                    NewThreadInfo = tmpThreadInfo;
                    break;
                }
            }
        }

        // the last element is also used, so add a new one
        if (tmpThreadInfo->Next == NULL)
        {
            NewThreadInfo = new TThreadInfoItem();
            tmpThreadInfo->Next = NewThreadInfo;
            break;
        }

        tmpThreadInfo = tmpThreadInfo->Next;
    }

    CTVData data;

    data.Name = name;

    data.AlwaysOnTop = FALSE;
    data.Left = CW_USEDEFAULT;
    data.Top = CW_USEDEFAULT;
    data.Width = CW_USEDEFAULT;
    data.Height = CW_USEDEFAULT;
    data.ShowCmd = SW_SHOW;

    data.ThreadInfo = NewThreadInfo;

    data.Success = FALSE;

    WINDOWPLACEMENT wndpl;
    wndpl.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(parent, &wndpl))
    {
        data.ShowCmd = wndpl.showCmd;
    }

    RECT rect;
    if (GetWindowRect(parent, &rect))
    {
        data.Left = rect.left;
        data.Top = rect.top;

        data.Width = rect.right - rect.left;
        data.Height = rect.bottom - rect.top;
    }

    BOOL alwaysOnTop = FALSE;
    if (SalamanderGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &alwaysOnTop, sizeof(alwaysOnTop), NULL))
    {
        data.AlwaysOnTop = alwaysOnTop;
    }

    data.Continue = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (data.Continue == NULL)
    {
        //TRACE_E("Unable to create Continue event.");
        return FALSE;
    }

    DWORD threadId = 0;
    HANDLE hThread = CreateThread(
        NULL,                                 // default security attributes
        0,                                    // use default stack size
        WindowThreadBodyTrace,                // thread function
        &data,                                // argument to thread function
        0 /*(suspended)?CREATE_SUSPENDED:0*/, // use default creation flags
        &threadId);                           // returns the thread identifier

    if (hThread != NULL)
    {
        ThreadQueue.Add(hThread, threadId);
        NewThreadInfo->Thread = hThread;
        //TRACE_I("hThread created");
        // wait until the thread processes the provided data and returns the results
        WaitForSingleObject(data.Continue, INFINITE);
        CloseHandle(data.Continue);

        return data.Success;
    }
    else
    {
        NewThreadInfo->Thread = 0; // signals that the thread was not created
        //TRACE_I("hThread problem");
        CloseHandle(data.Continue);
        return FALSE;
    }
}

BOOL WINAPI CPluginInterface::Release(HWND parent, BOOL force)
{
    //SalamanderGeneral->ShowMessageBox("CPluginInterface::Release", PLUGIN_NAME_EN, MSGBOX_INFO);
    TThreadInfoItem* curThreadInfo = FirstThreadItem;
    while (curThreadInfo != NULL)
    {
        if (curThreadInfo->Thread != 0) // belongs to an existing thread
        {
            HWND threadWindow = curThreadInfo->MainWindow; // we must copy it because it can change at any time
            if (threadWindow != 0)                         // the window still exists
            {
                PostMessage(threadWindow, WM_CLOSE, 0, 0);
            }
        }
        if (curThreadInfo != &BaseThreadItem) // the first item cannot be deleted because it is not dynamic
        {
            TThreadInfoItem* tmpThreadInfo = curThreadInfo;
            curThreadInfo = curThreadInfo->Next;
            delete tmpThreadInfo;
        }
        else
        {
            curThreadInfo = curThreadInfo->Next;
            BaseThreadItem.Clear();
        }
    }

    if (!ThreadQueue.KillAll(force) && !force)
        return FALSE; // we cannot finish yet, threads are still running

    DestroyAcceleratorTable(hAccelTable);
    CMainWindow::UnloadResourceStrings();
    if (!CMainWindow::UnregisterClass())
    {
        //SalamanderGeneral->ShowMessageBox("CPluginInterface::Release failed", PLUGIN_NAME_EN, MSGBOX_INFO);
    }
    return TRUE;
}

BOOL WINAPI CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent, int id, DWORD eventMask)
{
    switch (id)
    {
    case MENUCMD_FAKE_FOCUS:
    {
        SalamanderCallback.DoFocusFile();
        return TRUE;
    }
    case MENUCMD_FAKE_OPEN:
    {
        SalamanderCallback.DoOpenFolder();
        return TRUE;
    }
    case MENUCMD_OPEN: // example of how to process the input path to a directory/file
    {
        //TRACE_I("MENUCMD_OPEN");
        // path suggestion - here only the Windows path in the target panel (otherwise an empty string)

        // the current path is needed to convert relative paths to absolute ones
        int type;
        BOOL curPathIsDisk = FALSE;
        char curPath[MAX_PATH] = "";
        if (SalamanderGeneral->GetPanelPath(PANEL_SOURCE, curPath, MAX_PATH, &type, NULL))
        {
            if (type != PATH_TYPE_WINDOWS)
                curPath[0] = 0; // we take only disk paths
            else
                curPathIsDisk = TRUE;
        }

        //TODO: there should be a prompt asking the user for the path and confirming it...

        if (curPathIsDisk) // 'path' is the path to a file/directory, perform the requested action with it
        {
            //TRACE_I("Opening(" << curPath << ")." /*"): " << GetErrorText(error)*/);
            OpenDiskMapWindow(parent, curPath);
        }
        else
        {
            //TRACE_I("Not disk.");
        }

        return FALSE; // do not unselect items in the panel
    }
    default:
        SalamanderGeneral->ShowMessageBox("Unknown command.", "DEMOPLUG", MSGBOX_ERROR);
        break;
    }
    return FALSE;
}

BOOL WINAPI CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case MENUCMD_OPEN:
        helpID = IDH_SHOWDISKMAP;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}
