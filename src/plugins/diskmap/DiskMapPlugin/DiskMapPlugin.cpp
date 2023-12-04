// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// DiskMapPlugin.cpp : Defines the entry point for the DLL application.
//

#include "precomp.h"
#include "DiskMapPlugin.h"

#include "../DiskMap/GUI.MainWindow.h"

//pro registraci pluginu... neprekladatelne?
#define PLUGIN_NAME_EN "DiskMap" //neprekladane jmeno pluginu, pouziti pred loadem jazykoveho modulu + pro debug veci
#define PLUGIN_FILE "DISKMAP"    //klic v registry

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

TCHAR szPluginWebsite[] = TEXT("www.altap.cz"); // puvodni domena nebezi: http://salamander.diskmap.net

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

HACCEL hAccelTable = NULL;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
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
            Sleep(500);          // dojde k prepnuti do panelu, takze toto cekani bude v neaktivnim okne vieweru, tedy nicemu nevadi
            FocusPathBuf[0] = 0; // po 0.5 sekunde uz o fokus nestojime (resi pripad, kdy jsme trefili zacatek BUSY rezimu Salamandera)
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
        if (focusPath[0] != 0) // jen pokud jsme nemeli smulu (netrefili jsme zacatek BUSY rezimu Salamandera)
        {
            LPTSTR name;
            if (SalamanderGeneral->CutDirectory(focusPath, &name))
            {
                SalamanderGeneral->SkipOneActivateRefresh(); // hlavni okno pri prepnuti z viewru nebude delat refresh
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
        if (focusPath[0] != 0) // jen pokud jsme nemeli smulu (netrefili jsme zacatek BUSY rezimu Salamandera)
        {
            SalamanderGeneral->SkipOneActivateRefresh(); // hlavni okno pri prepnuti z viewru nebude delat refresh
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
            Sleep(500);          // dojde k prepnuti do panelu, takze toto cekani bude v neaktivnim okne vieweru, tedy nicemu nevadi
            FocusPathBuf[0] = 0; // po 0.5 sekunde uz o fokus nestojime (resi pripad, kdy jsme trefili zacatek BUSY rezimu Salamandera)
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
    static char buffer[5000]; // buffer pro mnoho stringu
    static char* act = buffer;

    //HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    if (5000 - (act - buffer) < 200)
        act = buffer;

RELOAD:
    int size = LoadString(HLanguage, resID, act, 5000 - (int)(act - buffer));
    // size obsahuje pocet nakopirovanych znaku bez terminatoru
    //  DWORD error = GetLastError();
    char* ret;
    if (size != 0 /* || error == NO_ERROR*/) // error je NO_ERROR, i kdyz string neexistuje - nepouzitelne
    {
        if ((5000 - (act - buffer) == size + 1) && (act > buffer))
        {
            // pokud byl retezec presne na konci bufferu, mohlo
            // jit o oriznuti retezce -- pokud muzeme posunout okno
            // na zacatek bufferu, nacteme string jeste jednou
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
#ifdef OPENSAL_VERSION // verze kopilovana pro distribuci se Salamanderem nebude zpetne kompatibilni s verzi 4.0 (zbytecna komplikace)
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
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // nastavime SalamanderVersion pro "spl_com.h"
    SalamanderVersion = salamander->GetVersion();
    //HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

#ifdef OPENSAL_VERSION // verze kopilovana pro distribuci se Salamanderem nebude zpetne kompatibilni s verzi 4.0 (zbytecna komplikace)
    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(), REQUIRE_LAST_VERSION_OF_SALAMANDER, PLUGIN_NAME_EN, MB_OK | MB_ICONERROR);
        return NULL;
    }
#else  // OPENSAL_VERSION
       // tento plugin je delany pro Salamandera 4.0 a vyssi - provedeme kontrolu
    if (SalamanderVersion < SALSDK_COMPATIBLE_WITH_VER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(), REQUIRE_COMPATIBLE_SAL_VERSION, PLUGIN_NAME_EN, MB_OK | MB_ICONERROR);
        return NULL;
    }
#endif // OPENSAL_VERSION

    //TODO: nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), PLUGIN_NAME_EN);
    if (HLanguage == NULL)
    {
        //MessageBox(salamander->GetParentWindow(), "SLG not found...", PLUGIN_NAME_EN, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

    // nastavime jmeno souboru s helpem
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

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(
        LoadStr(IDS_PLUGIN_NAME),
        FUNCTION_LOADSAVECONFIGURATION, //zadne funkce :-P
        VERSINFO_VERSION_NO_PLATFORM, VERSINFO_COPYRIGHT,
        LoadStr(IDS_PLUGIN_DESCRIPTION),
        PLUGIN_FILE, //regKeyName
        NULL,        //extensions
        NULL         //fsName
    );

    // nastavime URL home-page pluginu
    salamander->SetPluginHomePageURL(szPluginWebsite);

    //jen pro potreby message loop v pluginu - okna nepotrebuji
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

    /* TODO */ //Petr: ted uz je to vzdy Windows 2000, XP, Vista (byla tu podminka na W2K+)

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
    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
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
    if (regKey != NULL) //nacteni z registry
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

// varianta spusteni threadu vieweru bez pouziti objektu CThread
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

    // ukazka padu aplikace
    //  int *p = 0;
    //  *p = 0;       // ACCESS VIOLATION !

    CTVData* data = (CTVData*)param;

    CMainWindow* window = new CMainWindow(NULL);
    if (window == NULL)
    {
        SetEvent(data->Continue); // pustime dale hl. thread, od tohoto bodu nejsou data platna (=NULL)
        data = NULL;

        TRACE_I("CMainWindow Failed");
        return 1;
    }

    CoInitialize(NULL);

    if (window->Create(data->Left, data->Top, data->Width, data->Height, data->AlwaysOnTop) == NULL)
    {
        //TRACE_I("Notcreated");
        MessageBoxA(NULL, "DiskMap window could not be created", PLUGIN_NAME_EN, 0);

        //zrusit pamet okna
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
    SetEvent(data->Continue); // pustime dale hl. thread, od tohoto bodu nejsou data platna (=NULL)
    data = NULL;

    // pokud probehlo vse bez potizi, zacneme hledat soubory
    //CALL_STACK_MESSAGE1("ViewerThreadBody::OpenFile");
    window->PopulateStart();

    //CALL_STACK_MESSAGE1("ViewerThreadBody::message-loop");
    //TRACE_I("MsgLoop");
    // message loopa
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
        //hlasit zpet zrusene okno
        MyThreadInfo->MainWindow = 0;
    }

    //zrusit pamet okna
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

    //najit prvni nevyuzity
    while (true) //tmpThreadInfo->Next != NULL)
    {
        //jen pro ty, co zrovna nemaji zobrazene okno
        if (tmpThreadInfo->MainWindow == 0)
        {
            //thread nikdy neexistoval
            if (tmpThreadInfo->Thread == 0)
            {
                NewThreadInfo = tmpThreadInfo;
                break;
            }
            else
            {
                if (ThreadQueue.WaitForExit(tmpThreadInfo->Thread, 0))
                {
                    //lze pouzit, pokud thread jiz nebezi...
                    NewThreadInfo = tmpThreadInfo;
                    break;
                }
            }
        }

        //posledni prvek taky pouzit, takze pridat novy
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
        // pockame, az thread zpracuje predana data a vrati vysledky
        WaitForSingleObject(data.Continue, INFINITE);
        CloseHandle(data.Continue);

        return data.Success;
    }
    else
    {
        NewThreadInfo->Thread = 0; //signalizace nevytvoreneho vlakna
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
        if (curThreadInfo->Thread != 0) //patri k existujicimu threadu
        {
            HWND threadWindow = curThreadInfo->MainWindow; //musime okopirovat, protoze je mozne, ze se muze kdykoliv zmenit
            if (threadWindow != 0)                         //okno jeste existuje
            {
                PostMessage(threadWindow, WM_CLOSE, 0, 0);
            }
        }
        if (curThreadInfo != &BaseThreadItem) //prvni polozku nelze rusit, protoze neni dynamicka
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
        return FALSE; // jeste nemuzeme koncit, thready stale bezi

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
    case MENUCMD_OPEN: // ukazka postupu, jak se osetruje vstup cesty k adresari/souboru
    {
        //TRACE_I("MENUCMD_OPEN");
        // navrh retezce - zde jen windowsova cesta v cilovem panelu (jinak prazdny retezec)

        // aktualni cesta je potreba pro prevody relativnich cest na absolutni
        int type;
        BOOL curPathIsDisk = FALSE;
        char curPath[MAX_PATH] = "";
        if (SalamanderGeneral->GetPanelPath(PANEL_SOURCE, curPath, MAX_PATH, &type, NULL))
        {
            if (type != PATH_TYPE_WINDOWS)
                curPath[0] = 0; // bereme jen diskove cesty
            else
                curPathIsDisk = TRUE;
        }

        //TODO: tady by mel byt dotaz uzivateli na cestu a potvrzeni cesty...

        if (curPathIsDisk) // 'path' je cesta k souboru/adresari, provedeme s ni pozadovanou akci
        {
            //TRACE_I("Opening(" << curPath << ")." /*"): " << GetErrorText(error)*/);
            OpenDiskMapWindow(parent, curPath);
        }
        else
        {
            //TRACE_I("Not disk.");
        }

        return FALSE; // neodznacujeme polozky v panelu
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
