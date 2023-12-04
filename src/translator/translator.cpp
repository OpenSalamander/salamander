// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndframe.h"
#include "translator.h"
#include "wndrh.h"
#include "wndtext.h"
#include "wndtree.h"
#include "wndout.h"
#include "wndprev.h"
#include "config.h"
#include "wndlayt.h"
#include "dialogs.h"
#include "allochan.h"

const char* FRAMEWINDOW_CLASSNAME = "TranslatorFrame";
const char* MDICHILD_CLASSNAME = "TranslatorMDIChild";

int QuietValidate = 0;
int QuietTranslate = 0;
int QuietMarkChAsTrl = 0;
char QuietImport[MAX_PATH] = {0};
int QuietImportOnlyDlgLayout = 0;
char QuietImportTrlProp[MAX_PATH] = {0};
char QuietExportSLT[MAX_PATH] = {0};
char QuietExportSDC[MAX_PATH] = {0};
BOOL QuietExportSLTForDiff = FALSE;
char QuietImportSLT[MAX_PATH] = {0};
char QuietExportSpellChecker[MAX_PATH] = {0};
char OpenLayoutEditorDialogID[MAX_PATH] = {0};
BYTE* SharedMemoryCopy = NULL;

BOOL Windows7AndLater = FALSE;

// ****************************************************************************

class C__StrCriticalSection
{
public:
    CRITICAL_SECTION cs;

    C__StrCriticalSection() { InitializeCriticalSection(&cs); }
    ~C__StrCriticalSection() { DeleteCriticalSection(&cs); }
};

// zajistime vcasnou konstrukci kriticke sekce
#pragma warning(disable : 4073)
#pragma init_seg(lib)
C__StrCriticalSection __StrCriticalSection;

// ****************************************************************************

char* LoadStr(int resID)
{
    static char buffer[10000]; // buffer pro mnoho stringu
    static char* act = buffer;

    HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

    if (10000 - (act - buffer) < 200)
        act = buffer;

    HINSTANCE hInstance = HInstance;

RELOAD:
    int size = LoadString(hInstance, resID, act, 10000 - (act - buffer));
    // size obsahuje pocet nakopirovanych znaku bez terminatoru
    //  DWORD error = GetLastError();
    char* ret;
    if (size != 0 /* || error == NO_ERROR*/) // error je NO_ERROR, i kdyz string neexistuje - nepouzitelne
    {
        if ((10000 - (act - buffer) == size + 1) && (act > buffer))
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
        TRACE_E("Error in LoadStr(" << resID << ")." /*"): " << GetErrorText(error)*/);
        ret = (char*)"ERROR LOADING STRING";
    }

    HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

//*****************************************************************************
//
// GetErrorText
//
// ma problemy se synchronizaci, zatim to necham byt, protoze jde "jen" o vypis
// chyby, problem muze nastat, jakmile se dva thready rozhodnou soucasne vypsat
// chybu, jeden z nich vypise spatnou chybu

char* GetErrorText(DWORD error)
{
    static char tempErrorText[MAX_PATH + 20];
    int l = sprintf_s(tempErrorText, "(%d) ", error);
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      error,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      tempErrorText + l,
                      MAX_PATH + 20 - l,
                      NULL) == 0 ||
        *(tempErrorText + l) == 0)
    {
        sprintf_s(tempErrorText, "System error %d, text description is not available.", error);
    }
    return tempErrorText;
}

// ****************************************************************************

int GetRootPath(char* root, const char* path)
{
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        int len = s - path;
        memcpy(root, path, len);
        root[len] = '\\';
        root[len + 1] = 0;
        return len + 1;
    }
    else
    {
        root[0] = path[0];
        root[1] = ':';
        root[2] = '\\';
        root[3] = 0;
        return 3;
    }
}

// ****************************************************************************

DWORD AddUnicodeToClipboard(const char* str, int textLen)
{
    DWORD err = ERROR_SUCCESS;
    HGLOBAL unicode = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, 2 * (textLen + 1)));
    if (unicode != NULL)
    {
        wchar_t* unicodeStr = (wchar_t*)HANDLES(GlobalLock(unicode));
        if (unicodeStr != NULL)
        {
            if (textLen > 0 && MultiByteToWideChar(CP_ACP, 0, str, textLen, unicodeStr, textLen + 1) == 0)
                err = GetLastError();
            unicodeStr[textLen] = 0; // koncova nula
            HANDLES(GlobalUnlock(unicode));
            if (err == ERROR_SUCCESS && SetClipboardData(CF_UNICODETEXT, unicode) == NULL)
                err = GetLastError();
        }
        else
            err = GetLastError();
        if (err != ERROR_SUCCESS)
            NOHANDLES(GlobalFree(unicode));
    }
    else
        err = GetLastError();
    if (err != ERROR_SUCCESS)
        TRACE_E("Nepovedl se SetClipboardData pro Unicode text.");
    return err;
}

BOOL CopyHTextToClipboard(HGLOBAL hGlobalText, int textLen)
{
    if (hGlobalText == NULL)
    {
        TRACE_E("hGlobalText == NULL");
        return FALSE;
    }

    DWORD err = ERROR_SUCCESS;

    if (OpenClipboard(NULL))
    {
        if (EmptyClipboard())
        {
            //if (WindowsNTAndLater) -- fungujeme pouze pod NT
            {
                char* text = (char*)HANDLES(GlobalLock(hGlobalText));
                if (text != NULL)
                {
                    if (textLen == -1)
                        textLen = lstrlen(text);
                    err = AddUnicodeToClipboard(text, textLen); // ulozime text nejprve v Unicode (jen NT)
                    HANDLES(GlobalUnlock(hGlobalText));
                }
                else
                    err = GetLastError();
            }

            if (SetClipboardData(CF_TEXT, hGlobalText) == NULL) // pak ulozime text multibyte
                err = GetLastError();
        }
        else
            err = GetLastError();
        CloseClipboard();
    }
    else
        err = GetLastError();

    return err == ERROR_SUCCESS;
}

BOOL CopyTextToClipboard(const char* text, int textLen)
{
    if (text == NULL)
    {
        TRACE_E("text == NULL");
        return FALSE;
    }

    DWORD err = ERROR_SUCCESS;

    if (textLen == -1)
        textLen = lstrlen(text);

    HGLOBAL hglbCopy = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, textLen + 1));
    if (hglbCopy != NULL)
    {
        char* lptstrCopy = (char*)HANDLES(GlobalLock(hglbCopy));
        if (lptstrCopy != NULL)
        {
            memcpy(lptstrCopy, text, textLen);
            lptstrCopy[textLen] = 0;
            HANDLES(GlobalUnlock(hglbCopy));
            if (!CopyHTextToClipboard(hglbCopy, textLen))
                return FALSE;
        }
        else
            err = GetLastError();
    }
    else
        err = GetLastError();

    return err == ERROR_SUCCESS;
}

//*****************************************************************************
//
// GetTargetDirectory
//
//  parent  - okno vlastnika dialogu
//  title   - titul dialogu
//  comment - text zobrazeny nad tree-view
//  path    - buffer pro vybranou cestu (delka minimalne MAX_PATH)
//
//  vraci TRUE pokud je path platna nova cesta

struct CBrowseData
{
    const char* Title;
    const char* InitDir;
    HWND HCenterWindow;
};

int CALLBACK DirectoryBrowse(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED)
    {
        // pokusim se vycentrovat dialog k centrovacimu oknu
        RECT centerRect;
        GetWindowRect(((CBrowseData*)lpData)->HCenterWindow, &centerRect);
        int centerW = centerRect.right - centerRect.left;
        int centerH = centerRect.bottom - centerRect.top;
        int centerX = centerRect.left;
        int centerY = centerRect.top;
        RECT r;
        GetWindowRect(hwnd, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        int x = centerX + (centerW - w) / 2;
        int y = centerY + (centerH - h) / 2;

        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, FALSE);
        // osetrim uplnou viditelnost dialogu
        if (x < workArea.left)
            x = workArea.left;
        if (y < workArea.top)
            y = workArea.top;
        if (x + w > workArea.right)
            x = workArea.right - w;
        if (y + h > workArea.bottom)
            y = workArea.bottom - h;

        // umistim ho
        SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        // nastavim header
        SetWindowText(hwnd, ((CBrowseData*)lpData)->Title);
        if (((CBrowseData*)lpData)->InitDir != NULL)
        {
            char path[MAX_PATH];
            GetRootPath(path, ((CBrowseData*)lpData)->InitDir);
            if (strlen(path) < strlen(((CBrowseData*)lpData)->InitDir)) // neni to root-dir
            {
                strcpy_s(path, ((CBrowseData*)lpData)->InitDir);
                char& ch = path[strlen(path) - 1];
                if (ch == '\\')
                    ch = 0;
            }
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)path);
        }
    }
    if (uMsg == BFFM_SELCHANGED)
    {
        if ((ITEMIDLIST*)lParam != NULL)
        {
            char path[MAX_PATH];
            BOOL ret = SHGetPathFromIDList((ITEMIDLIST*)lParam, path);
            SendMessage(hwnd, BFFM_ENABLEOK, 0, ret);
        }
    }
    return 0;
}

BOOL GetTargetDirectoryAux(HWND parent, HWND hCenterWindow,
                           const char* title, const char* comment,
                           char* path, BOOL onlyNet, const char* initDir)
{
    __try
    {
        ITEMIDLIST* pidl; // vyber root-folderu
        if (onlyNet)
            SHGetSpecialFolderLocation(parent, CSIDL_NETWORK, &pidl);
        else
            pidl = NULL;

        // otevreni dialogu
        char display[MAX_PATH];
        BROWSEINFO bi;
        bi.hwndOwner = parent;
        bi.pidlRoot = pidl;
        bi.pszDisplayName = display;
        bi.lpszTitle = comment;
        bi.ulFlags = BIF_RETURNONLYFSDIRS;
        bi.lpfn = DirectoryBrowse;
        CBrowseData bd;
        bd.Title = title;
        bd.InitDir = initDir;
        bd.HCenterWindow = hCenterWindow;
        bi.lParam = (LPARAM)&bd;
        ITEMIDLIST* res = SHBrowseForFolder(&bi);
        BOOL ret = FALSE; // navratova hodnota
        if (res != NULL)
        {
            SHGetPathFromIDList(res, path);
            ret = TRUE;
        }
        // uvolneni item-id-listu
        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->DidAlloc(pidl) == 1)
                alloc->Free(pidl);
            if (alloc->DidAlloc(res) == 1)
                alloc->Free(res);
            alloc->Release();
        }
        return ret;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE; // chyba
    }
}

BOOL GetTargetDirectory(HWND parent, const char* title, const char* comment,
                        char* path, const char* initDir)
{
    return GetTargetDirectoryAux(parent, parent, title, comment, path, FALSE, initDir);
}

HWND GetMsgParent()
{
    return FrameWindow.HWindow;
}

// ****************************************************************************
//
// GetCmdLine - ziskani parametru z prikazove radky
//
// buf + size - buffer pro parametry
// argv - pole ukazatelu, ktere se naplni parametry
// argCount - na vstupu je to pocet prvku v argv, na vystupu obsahuje pocet parametru
// cmdLine - parametry prikazove radky (bez jmena .exe souboru - z WinMain)

BOOL GetCmdLine(char* buf, int size, char* argv[], int& argCount, char* cmdLine)
{
    int space = argCount;
    argCount = 0;
    char* c = buf;
    char* end = buf + size;

    char* s = cmdLine;
    char term;
    while (*s != 0)
    {
        if (*s == '"') // pocatecni '"'
        {
            if (*++s == 0)
                break;
            term = '"';
        }
        else
            term = ' ';

        if (argCount < space && c < end)
            argv[argCount++] = c;
        else
            return c < end; // chyba jen pokud je maly buffer

        while (1)
        {
            if (*s == term || *s == 0)
            {
                if (*s == 0 || term != '"' || *++s != '"') // neni-li to nahrada "" -> "
                {
                    if (*s != 0)
                        s++;
                    while (*s != 0 && *s == ' ')
                        s++;
                    if (c < end)
                    {
                        *c++ = 0;
                        break;
                    }
                    else
                        return FALSE;
                }
            }
            if (c < end)
                *c++ = *s++;
            else
                return FALSE;
        }
    }
    return TRUE;
}

//*****************************************************************************
//
// GetSystemDPI
//

int GetSystemDPI(HDC hDC = NULL)
{
    static int systemDPI = -1;
    if (systemDPI == -1)
    {
        HDC hTmpDC;
        if (hDC == NULL)
            hTmpDC = GetDC(NULL);
        else
            hTmpDC = hDC;
        systemDPI = GetDeviceCaps(hTmpDC, LOGPIXELSX);
#ifdef _DEBUG
        if (systemDPI != GetDeviceCaps(hTmpDC, LOGPIXELSY))
            TRACE_E("Unexpected situation: LOGPIXELSX != LOGPIXELSY.");
#endif
        if (hDC == NULL)
            ReleaseDC(NULL, hTmpDC);
    }
    return systemDPI;
}

//*****************************************************************************
//
// GetScaleForSystemDPI
//

int GetScaleForSystemDPI()
{
    int dpi = GetSystemDPI();
    int scale;
    if (dpi <= 96)
        scale = 100;
    else if (dpi <= 120)
        scale = 125;
    else if (dpi <= 144)
        scale = 150;
    else if (dpi <= 192)
        scale = 200;
    else if (dpi <= 240)
        scale = 250;
    else if (dpi <= 288)
        scale = 300;
    else if (dpi <= 384)
        scale = 400;
    else
        scale = 500;

    return scale;
}

//*****************************************************************************
//
// GetFixedFont
//

void GetFixedLogFont(LOGFONT* lf)
{
    const int VIEWER_FONT_PTS = 9;
    memset(lf, 0, sizeof(*lf));
    lf->lfHeight = -(VIEWER_FONT_PTS * GetSystemDPI()) / 72;
    lf->lfWeight = FW_NORMAL;
    lf->lfCharSet = DEFAULT_CHARSET;
    lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf->lfQuality = DEFAULT_QUALITY;
    lf->lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    lstrcpy(lf->lfFaceName, "Consolas");
}

//*****************************************************************************
//
// SetWinPlacement
//

void SetWinPlacement(WINDOWPLACEMENT* wp, int l, int t, int r, int b)
{
    wp->length = sizeof(WINDOWPLACEMENT);
    wp->rcNormalPosition.left = l;
    wp->rcNormalPosition.right = r;
    wp->rcNormalPosition.top = t;
    wp->rcNormalPosition.bottom = b;
    wp->showCmd = 1;
}

//*****************************************************************************
//
// WinMain
//

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR cmdLine, int cmdShow)
{
    SetTraceProcessName("TRANSLATOR");
    SetTraceThreadName("Main thread");
    TRACE_I("Begin.");
    HInstance = hInstance;

    // nastavime lokalizovane hlasky do modulu ALLOCHAN (zajistuje pri nedostatku pameti hlaseni
    // uzivateli + Retry button + kdyz vse selze tak i Cancel pro terminate softu)
    SetAllocHandlerMessage(NULL, FRAMEWINDOW_NAME, NULL, NULL);

    SetWinLibStrings("Invalid number!", FRAMEWINDOW_NAME);

    INITCOMMONCONTROLSEX initCtrls;
    initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    initCtrls.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_COOL_CLASSES | ICC_DATE_CLASSES |
                      ICC_ANIMATE_CLASS | ICC_HOTKEY_CLASS | ICC_INTERNET_CLASSES | ICC_LINK_CLASS |
                      ICC_NATIVEFNTCTL_CLASS | ICC_PAGESCROLLER_CLASS | ICC_PROGRESS_CLASS |
                      ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES;
    if (!InitCommonControlsEx(&initCtrls))
    {
        TRACE_E("InitCommonControlsEx failed");
        return 1;
    }

    Windows7AndLater = SalIsWindowsVersionOrGreater(6, 1, 0);
    if (!Windows7AndLater)
    {
        MessageBox(NULL, "Translator needs Windows 7 or later.",
                   FRAMEWINDOW_NAME, MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    if (!InitializeWinLib())
        return 1; // musime inicializovat WinLib pred prvnim zobrazenim

    CFrameWindow::RegisterUniversalClass(CS_DBLCLKS | CS_SAVEBITS,
                                         0,
                                         0,
                                         LoadIcon(HInstance, MAKEINTRESOURCE(IDI_MAIN)),
                                         LoadCursor(NULL, IDC_ARROW),
                                         (HBRUSH)(COLOR_APPWORKSPACE + 1),
                                         NULL,
                                         FRAMEWINDOW_CLASSNAME,
                                         NULL);
    CFrameWindow::RegisterUniversalClass(CS_DBLCLKS | CS_SAVEBITS,
                                         0,
                                         0,
                                         NULL,
                                         LoadCursor(NULL, IDC_ARROW),
                                         (HBRUSH)(COLOR_BTNFACE + 1),
                                         NULL,
                                         MDICHILD_CLASSNAME,
                                         NULL);

    if (FrameWindow.Create(FRAMEWINDOW_CLASSNAME,
                           FRAMEWINDOW_NAME,
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                           CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                           NULL,
                           LoadMenu(HInstance, MAKEINTRESOURCE(IDM_MAINMENU)),
                           HInstance,
                           &FrameWindow))
    {
        Config.Load();

        HACCEL hAccelTable = HANDLES(LoadAccelerators(HInstance,
                                                      MAKEINTRESOURCE(IDA_MAINACCELS)));
        HACCEL hAccelTableLayout = HANDLES(LoadAccelerators(HInstance,
                                                            MAKEINTRESOURCE(IDA_LAYOUTACCELS)));

        char buf[MAX_PATH];
        char* argv[20];
        int p = 20; // pocet prvku pole argv
        BOOL cmdLineParamsUsed = FALSE;
        if (GetCmdLine(buf, MAX_PATH, argv, p, cmdLine))
        {
            if (p > 0)
            {
                if (p == 2 && _stricmp(argv[0], "-quiet-validate-all") != 0 &&
                        _stricmp(argv[0], "-quiet-validate-layout") != 0 &&
                        _stricmp(argv[0], "-quiet-translate") != 0 &&
                        _stricmp(argv[0], "-quiet-mark-changed-as-translated") != 0 ||
                    p == 3 && _stricmp(argv[0], "-quiet-import") != 0 &&
                        _stricmp(argv[0], "-quiet-import-only-dialog-layout") != 0 &&
                        _stricmp(argv[0], "-quiet-import-trlprop") != 0 &&
                        _stricmp(argv[0], "-quiet-export-slt") != 0 &&
                        _stricmp(argv[0], "-quiet-export-sizes") != 0 &&
                        _stricmp(argv[0], "-quiet-export-slt-for-diff") != 0 &&
                        _stricmp(argv[0], "-quiet-import-slt") != 0 &&
                        _stricmp(argv[0], "-quiet-export-spellcheck") != 0 &&
                        _stricmp(argv[0], "-open-layout-editor") != 0 ||
                    p > 3)
                {
                    MessageBox(FrameWindow.HWindow, "Unexpected command line arguments.\n\n"
                                                    "Usage: translator.exe [switch] [switch_param] project.atp \n\n"
                                                    "Switches:\n"
                                                    "-quiet-validate-all: validates translation (using all tests) "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-validate-layout: validates dialog layout "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-import: imports old translation from [importdir] directory "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-import-only-dialog-layout: imports only dialog layouts from old translation "
                                                    "(useful e.g. to import dialog layouts from Czech version to Slovak version) "
                                                    "from [importdir] directory and if all is OK, closes Translator "
                                                    "automatically.\n\n"
                                                    "-quiet-import-trlprop: imports translation properties from another [project]"
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-export-slt: exports translation to SLT text archive "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-import-slt: imports translation from SLT text archive "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-export-slt-for-diff: exports translation to special SLT "
                                                    "text archive which does not contain version info (allows to "
                                                    "compare data between different versions) "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-export-spellcheck: exports texts for spell checker to text file "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-quiet-translate: searches for untranslated strings "
                                                    "and if nothing is found or module is completely untranslated, "
                                                    "closes Translator automatically.\n\n"
                                                    "-quiet-mark-changed-as-translated: marks all untranslated but changed strings "
                                                    "as translated.\n\n"
                                                    "-quiet-export-sizes: exports sizes of dialogs and controls to SDC text file "
                                                    "and if all is OK, closes Translator automatically.\n\n"
                                                    "-open-layout-editor: opens layout editor for [dialogid] dialog,"
                                                    "on layout editor exit closes Translator.\n\n"
                                                    "Project file (project.atp) must exist.\n\n",
                               FRAMEWINDOW_NAME, MB_OK | MB_ICONINFORMATION);
                }
                else
                {
                    FrameWindow.ProcessCmdLineParams(argv, p);
                    cmdLineParamsUsed = TRUE;
                }
            }
        }

        if (Config.FrameWindowPlacement.length == 0) // pri prvnim spusteni okno maximalizujeme a rozvrhneme child okna
        {
            ShowWindow(FrameWindow.HWindow, SW_SHOWMAXIMIZED);
            RECT r;
            GetClientRect(FrameWindow.HWindow, &r);
            // odecteme experimentalne zjistenou sirku/vysku vnitrnich ramecku
            r.right -= 4;
            r.bottom -= 4;
            int width = r.right - r.left;
            int height = r.bottom - r.top;

            const int TREEWIN_WIDTH = max(width / 10, (300 * GetScaleForSystemDPI()) / 100);
            const int OUTWIN_HEIGHT = max(height / 5, (200 * GetScaleForSystemDPI()) / 100);
            const int TEXTWIN_HEIGHT = max(height / 4, (283 * GetScaleForSystemDPI()) / 100);
            SetWinPlacement(&Config.TreeWindowPlacement, 0, 0, TREEWIN_WIDTH, r.bottom - OUTWIN_HEIGHT);
            SetWinPlacement(&Config.RHWindowPlacement, 0, r.bottom - OUTWIN_HEIGHT, r.right / 2, r.bottom);
            SetWinPlacement(&Config.TextWindowPlacement, TREEWIN_WIDTH,
                            r.bottom - OUTWIN_HEIGHT - TEXTWIN_HEIGHT,
                            r.right, r.bottom - OUTWIN_HEIGHT);
            SetWinPlacement(&Config.OutWindowPlacement, r.right / 2, r.bottom - OUTWIN_HEIGHT,
                            r.right, r.bottom);
            SetWinPlacement(&Config.PreviewWindowPlacement, TREEWIN_WIDTH, 0, r.right,
                            r.bottom - OUTWIN_HEIGHT - TEXTWIN_HEIGHT);

            SetWindowPlacement(TreeWindow.HWindow, &Config.TreeWindowPlacement);
            SetWindowPlacement(RHWindow.HWindow, &Config.RHWindowPlacement);
            SetWindowPlacement(TextWindow.HWindow, &Config.TextWindowPlacement);
            SetWindowPlacement(OutWindow.HWindow, &Config.OutWindowPlacement);
            SetWindowPlacement(PreviewWindow.HWindow, &Config.PreviewWindowPlacement);

            TextWindow.SetColumnsWidth();
        }
        else
        {
            SetWindowPlacement(FrameWindow.HWindow, &Config.FrameWindowPlacement);
            ShowWindow(FrameWindow.HWindow, SW_SHOW);
        }

        CAgreementDialog agreementDlg(FrameWindow.HWindow);
        if (Config.AgreementConfirmed ||
            agreementDlg.HasAgreement() && agreementDlg.Execute() == IDOK ||
            !agreementDlg.HasAgreement())
        {
            if (!Config.AgreementConfirmed && agreementDlg.HasAgreement())
                Config.AgreementConfirmed = TRUE;

            if (!cmdLineParamsUsed)
                PostMessage(FrameWindow.HWindow, WM_AUTORELOAD, 0, 0);

            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0))
            {
                if (!TranslateMDISysAccel(FrameWindow.HMDIClient, &msg) &&
                    !TranslateAccelerator(FrameWindow.HWindow, (LayoutWindow == NULL ? hAccelTable : hAccelTableLayout), &msg))
                {
                    BOOL ret = (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN);
                    if (ret || TextWindow.HWindow == NULL || !IsDialogMessage(TextWindow.HWindow, &msg))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
        }
        else
            DestroyWindow(FrameWindow.HWindow);
    }

    TRACE_I("End.");
    return 0;
}
