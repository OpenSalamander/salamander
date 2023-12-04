// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <Sddl.h>

#include "resource.h"
#include "salbreak.h"
#include "tasklist.h"
#include "md5.h"

#define MAX_LOADSTRING 100

#define NOHANDLES(function) function // obrana proti zanaseni maker HANDLES do zdrojaku pomoci CheckHnd

// aby nedochazelo k problemum se stredniky v nize nadefinovanych makrech
inline void __TraceEmptyFunction() {}

#define TRACE_E(str) __TraceEmptyFunction()

// Global Variables:
HINSTANCE hInst;                     // current instance
TCHAR szTitle[MAX_LOADSTRING];       // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING]; // The title bar text

// Foward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK About(HWND, UINT, WPARAM, LPARAM);

HWND HMainWindow = NULL;

void BreakAllSalamanders()
{
    CProcessListItem items[MAX_TL_ITEMS];
    int c = TaskList.GetItems(items, NULL);
    for (int i = 0; i < c; i++)
    {
        if (!TaskList.FireEvent(TASKLIST_TODO_BREAK, items[i].PID))
        {
            TRACE_E("Unable to deliver break-message.");
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE /*hPrevInstance*/,
                   LPSTR /*lpCmdLine*/,
                   int /*nCmdShow*/)
{
    if (!TaskList.Init())
        return 666;

    // TODO: Place code here.
    MSG msg;
    HACCEL hAccelTable;

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_SALBREAK, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, 0))
    {
        return FALSE;
    }

    RegisterHotKey(HMainWindow, 0x0001, MOD_ALT | MOD_CONTROL | MOD_SHIFT, VK_F12);

    hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_SALBREAK);

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_HOTKEY && msg.wParam == 0x0001)
        {
            BreakAllSalamanders();
            continue;
        }
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnregisterHotKey(HMainWindow, 0x0001);

    return msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = (WNDPROC)WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_SALBREAK);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = (LPCSTR)IDC_SALBREAK;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

    return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;

    hInst = hInstance; // Store instance handle in our global variable

    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

    HMainWindow = hWnd;

    //   ShowWindow(hWnd, nCmdShow);
    //   UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;
    TCHAR szHello[MAX_LOADSTRING];
    LoadString(hInst, IDS_HELLO, szHello, MAX_LOADSTRING);

    switch (message)
    {
    case WM_COMMAND:
        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code here...
        RECT rt;
        GetClientRect(hWnd, &rt);
        DrawText(hdc, szHello, lstrlen(szHello), &rt, DT_CENTER);
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL GetStringSid(LPTSTR* stringSid)
{
    *stringSid = NULL;

    HANDLE hToken = NULL;
    DWORD dwBufferSize = 0;
    PTOKEN_USER pTokenUser = NULL;

    // Open the access token associated with the calling process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TRACE_E("OpenProcessToken failed.");
        return FALSE;
    }

    // get the size of the memory buffer needed for the SID
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize);

    pTokenUser = (PTOKEN_USER)malloc(dwBufferSize);
    memset(pTokenUser, 0, dwBufferSize);

    // Retrieve the token information in a TOKEN_USER structure.
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize))
    {
        TRACE_E("GetTokenInformation failed.");
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);

    if (!IsValidSid(pTokenUser->User.Sid))
    {
        TRACE_E("The owner SID is invalid.\n");
        free(pTokenUser);
        return FALSE;
    }

    // volajici musi uvolnit vracenou pamet pomoci LocalFree, viz MSDN
    ConvertSidToStringSid(pTokenUser->User.Sid, stringSid);

    free(pTokenUser);

    return TRUE;
}

BOOL GetSidMD5(BYTE* sidMD5)
{
    ZeroMemory(sidMD5, 16);

    HANDLE hToken = NULL;
    DWORD dwBufferSize = 0;
    PTOKEN_USER pTokenUser = NULL;

    // Open the access token associated with the calling process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TRACE_E("OpenProcessToken failed.");
        return FALSE;
    }

    // get the size of the memory buffer needed for the SID
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize);

    pTokenUser = (PTOKEN_USER)malloc(dwBufferSize);
    memset(pTokenUser, 0, dwBufferSize);

    // Retrieve the token information in a TOKEN_USER structure.
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize))
    {
        TRACE_E("GetTokenInformation failed.");
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);

    if (!IsValidSid(pTokenUser->User.Sid))
    {
        TRACE_E("The owner SID is invalid.\n");
        free(pTokenUser);
        return FALSE;
    }

    MD5 context;
    context.update((BYTE*)pTokenUser->User.Sid, GetLengthSid(pTokenUser->User.Sid));
    context.finalize();
    memcpy(sidMD5, context.digest, 16);

    free(pTokenUser);

    return TRUE;
}

// Vice informaci viz:
//   http://www.codeguru.com/cpp/w-p/win32/tutorials/print.php/c4545 (A NotQuiteNullDacl Class)
//   http://forums.microsoft.com/msdn/ShowPost.aspx?PostID=748596&SiteID=1 (uz resi vistu)
//   Programming Server-Side Applications for Microsoft Windows 2000, Richter/Clark, Microsoft Press 2000, Chapter 10, pp 458-460
//   Ask Dr. Gui #49 (nemuzu ho poradne najit online, tak radeji vkladam sem):
// Mutex Madness
// Dear Dr. GUI:
// I'm a French engineer and my English isn't perfect so I hope that you understand my question.
// I have a DLL that creates a mutex. I have some problems synchronizing all processes that use my DLL. I create the mutex with code that looks like the following:
// ghMutexExe = CreateMutex(NULL, FALSE , "APPLICOM_IO_MUTEX");
// When I use my DLL with an application that is running in Real time priority, I can't run another application that uses the same DLL. When I use the function:
// ghMutexExe = CreateMutex(NULL, FALSE , "APPLICOM_IO_MUTEX");
// It returns NULL (ghMutexExe = NULL), and GetLastError returns 5 (Access is denied. ERROR_ACCESS_DENIED).
// Can you help me?
// Bertrand Lauret
//
// Dr. GUI replies:
// Your English is jus fine. (Dr, GUI is just glad you didn't ask to get my reply in French.) Most people do not know this, but the good doctor is bilingual. He speaks English and C++.
// It is certainly possible to have problems with thread synchronization due to differing priorities of threads. However, it is very unlikely that you would receive an ERROR_ACCESS_DENIED when trying to obtain a handle to an existing mutex because of the priority class of the creating process.
// Typically, ERROR_ACCESS_DENIED is returned as a result of failure because of the security implications of the function being called. Let me describe a scenario where this could happen with Microsoft Windows NT? or Windows 2000:
// Process A is running as a service (perhaps in real time, perhaps not) and creates a named mutex passing NULL as the first parameter indicating default security for the object.
// Process B is launched by the interactive user and attempts to obtain a handle to the named mutex through a similar call to CreateMutex. This call fails with ERROR_ACCESS_DENIED because the default security of the service excludes all but the local system for ALL_ACCESS security to the object. This process does not have access even if it is running as an administrator of the system.
// This type of failure is the result of a combination of points:
// Most services are installed in the local system account and run with special security rights as a result.
// Processes running in the local system account grant GENERIC_ALL access to other processes running in the local system, and READ_CONTROL, GENERIC_EXECUTE, and GENERIC_READ access to members of the Administrators group. All other access to the object by any other users or groups is denied.
// All calls to CreateMutex implicitly request MUTEX_ALL_ACCESS for the object in question. An interactive user does not have the rights required to obtain a handle to an object created from the local system security context as a result.
// There are several solutions to this problem:
// You can set the security descriptor in your call to CreateMutex to contain a "NULL DACL." An object with NULL-DACL security grants all access to everyone, regardless of security context. One downside of this approach is that the object is now completely unsecured. In fact, a malicious application could obtain a handle to a named object created with a NULL DACL and change its security access such that other processes are unable to use the object, effectively ruining the object. PLEASE NOTE: The doctor seriously advises against this "solution."
// A second option would be to create a security descriptor that explicitly grants the necessary rights to the built-in group: Everyone. In the case of a mutex, this would be MUTEX_ALL_ACCESS. This is preferable because it will not allow malicious (or buggy) software to affect other software's access to the object.
// You can also choose to explicitly allow access to the user accounts that will need to use the object. This type of explicit security can be good but comes with the disadvantage of needing to know who will need access at the time the object is created.
// My preference in cases of creating objects for general availability to users of the system is the second. Regardless of which approach you choose, you have the additional choice of whether to apply the security descriptor to each object as it is created, or to change the default security for your process by changing the token's default DACL. In this case, you would continue to pass NULL for the security parameter when creating an object.
// Dr. GUI thinks that in your case you should only set the security for specific objects because you are developing a DLL, which may not want to change the security for every object in the process.
// The following function wraps CreateMutex with the additional functionality of creating security that is more relaxed than is the default for a service:
/*
    // pridelime mutexu vsechna mozna prava (aby napriklad fungovalo otevirani mezi AsAdmin a User ucty)
    // cistejsi by bylo zavolani funkce ObtainAccessableMutex(), viz jeji obsahly komentar
    SECURITY_ATTRIBUTES secAttr;
    char secDesc[ SECURITY_DESCRIPTOR_MIN_LENGTH ];
    secAttr.nLength = sizeof(secAttr);
    secAttr.bInheritHandle = FALSE;
    secAttr.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(secAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    // give the security descriptor a NULL DACL, done using the  "TRUE, (PACL)NULL" here
    SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, 0, FALSE);
*/

/*
// podle http://forums.microsoft.com/msdn/ShowPost.aspx?PostID=748596&SiteID=1
// by se pod vistou mel nastavit integrity level, ale nedokazal jsem problem na Viste/Serveru2008 navodit
// takze zatim nechavam pouze v komentari, dokud na to nenarazime
//
// Windows Integrity Mechanism Design
// http://msdn.microsoft.com/en-us/library/bb625963.aspx
if (windowsVistaAndLater) // FIXME: nenarazil jsem na viste na to, ze bych musel integrity level resit (mezi AdAdmin / normalni aplikaci)
{
  PSECURITY_DESCRIPTOR pSD;
  ConvertStringSecurityDescriptorToSecurityDescriptor(
      "S:(ML;;NW;;;LW)", // this means "low integrity"
      SDDL_REVISION_1,
      &pSD,
      NULL);
  PACL pSacl = NULL;                  // not allocated
  BOOL fSaclPresent = FALSE;
  BOOL fSaclDefaulted = FALSE;
  GetSecurityDescriptorSacl(
      pSD,
      &fSaclPresent,
      &pSacl,
      &fSaclDefaulted);
  SetSecurityDescriptorSacl(secAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
}
*/

SECURITY_ATTRIBUTES* CreateAccessableSecurityAttributes(SECURITY_ATTRIBUTES* sa, SECURITY_DESCRIPTOR* sd,
                                                        DWORD allowedAccessMask, PSID* psidEveryone, PACL* paclNewDacl)
{
    SID_IDENTIFIER_AUTHORITY siaWorld = SECURITY_WORLD_SID_AUTHORITY;
    int nAclSize;

    *psidEveryone = NULL;
    *paclNewDacl = NULL;

    // Create the everyone sid
    if (!AllocateAndInitializeSid(&siaWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AllocateAndInitializeSid() failed!");
        goto ErrorExit;
    }

    nAclSize = GetLengthSid(psidEveryone) * 2 + sizeof(ACCESS_ALLOWED_ACE) + sizeof(ACCESS_DENIED_ACE) + sizeof(ACL);
    *paclNewDacl = (PACL)LocalAlloc(LPTR, nAclSize);
    if (*paclNewDacl == NULL)
    {
        TRACE_E("CreateAccessableSecurityAttributes(): LocalAlloc() failed!");
        goto ErrorExit;
    }
    if (!InitializeAcl(*paclNewDacl, nAclSize, ACL_REVISION))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): InitializeAcl() failed!");
        goto ErrorExit;
    }
    if (!AddAccessDeniedAce(*paclNewDacl, ACL_REVISION, WRITE_DAC | WRITE_OWNER, *psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AddAccessDeniedAce() failed!");
        goto ErrorExit;
    }
    if (!AddAccessAllowedAce(*paclNewDacl, ACL_REVISION, allowedAccessMask, *psidEveryone))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): AddAccessAllowedAce() failed!");
        goto ErrorExit;
    }
    if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): InitializeSecurityDescriptor() failed!");
        goto ErrorExit;
    }
    if (!SetSecurityDescriptorDacl(sd, TRUE, *paclNewDacl, FALSE))
    {
        TRACE_E("CreateAccessableSecurityAttributes(): SetSecurityDescriptorDacl() failed!");
        goto ErrorExit;
    }
    sa->nLength = sizeof(SECURITY_ATTRIBUTES);
    sa->bInheritHandle = FALSE;
    sa->lpSecurityDescriptor = sd;
    return sa;

ErrorExit:
    if (*paclNewDacl != NULL)
    {
        LocalFree(*paclNewDacl);
        *paclNewDacl = NULL;
    }
    if (*psidEveryone != NULL)
    {
        FreeSid(*psidEveryone);
        *psidEveryone = NULL;
    }
    return NULL;
}

//****************************************************************************
//
// GetProcessIntegrityLevel (vytazeno z MSDN)
// V pripade uspechu vrati TRUE a naplni DWORD na ktery odkazuje 'integrityLevel'
// jinak (pri selhani nebo pod OS strasima nez Vista) vrati FALSE
//

#define SECURITY_MANDATORY_UNTRUSTED_RID (0x00000000L)
#define SECURITY_MANDATORY_LOW_RID (0x00001000L)               // Low Integrity
#define SECURITY_MANDATORY_MEDIUM_RID (0x00002000L)            // Medium Integrity
#define SECURITY_MANDATORY_HIGH_RID (0x00003000L)              // High Integrity
#define SECURITY_MANDATORY_SYSTEM_RID (0x00004000L)            // System Integrity
#define SECURITY_MANDATORY_PROTECTED_PROCESS_RID (0x00005000L) //

BOOL SalIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // nahrada za memset (nevyzaduje RTLko)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

BOOL GetProcessIntegrityLevel(DWORD* integrityLevel)
{
    HANDLE hToken;
    HANDLE hProcess;

    DWORD dwLengthNeeded;
    DWORD dwError = ERROR_SUCCESS;

    PTOKEN_MANDATORY_LABEL pTIL = NULL;
    DWORD dwIntegrityLevel;

    BOOL ret = FALSE;

    BOOL WindowsVistaAndLater = SalIsWindowsVersionOrGreater(6, 0, 0);

    if (WindowsVistaAndLater) // integrity levels byly zavedeny od Windows Vista
    {
        hProcess = GetCurrentProcess();
        if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
        {
            // Get the Integrity level.
            if (!GetTokenInformation(hToken, (_TOKEN_INFORMATION_CLASS)25 /*TokenIntegrityLevel*/, NULL, 0, &dwLengthNeeded))
            {
                dwError = GetLastError();
                if (dwError == ERROR_INSUFFICIENT_BUFFER)
                {
                    pTIL = (PTOKEN_MANDATORY_LABEL)LocalAlloc(0, dwLengthNeeded);
                    if (pTIL != NULL)
                    {
                        if (GetTokenInformation(hToken, (_TOKEN_INFORMATION_CLASS)25 /*TokenIntegrityLevel*/, pTIL, dwLengthNeeded, &dwLengthNeeded))
                        {
                            dwIntegrityLevel = *GetSidSubAuthority(pTIL->Label.Sid, (DWORD)(UCHAR)(*GetSidSubAuthorityCount(pTIL->Label.Sid) - 1));
                            ret = TRUE;

                            /*
              if (dwIntegrityLevel == SECURITY_MANDATORY_LOW_RID)
              {
               // Low Integrity
               wprintf(L"Low Process");
              }
              else if (dwIntegrityLevel >= SECURITY_MANDATORY_MEDIUM_RID && 
                   dwIntegrityLevel < SECURITY_MANDATORY_HIGH_RID)
              {
               // Medium Integrity
               wprintf(L"Medium Process");
              }
              else if (dwIntegrityLevel >= SECURITY_MANDATORY_HIGH_RID)
              {
               // High Integrity
               wprintf(L"High Integrity Process");
              }
              else if (dwIntegrityLevel >= SECURITY_MANDATORY_SYSTEM_RID)
              {
               // System Integrity
               wprintf(L"System Integrity Process");
              }
              */
                        }
                        LocalFree(pTIL);
                    }
                }
            }
            CloseHandle(hToken);
        }
    }
    if (ret)
        *integrityLevel = dwIntegrityLevel;
    else
        *integrityLevel = 0;
    return ret;
}
