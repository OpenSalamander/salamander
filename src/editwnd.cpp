// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "editwnd.h"
#include "stswnd.h"
#include <uxtheme.h>

#include <Shlwapi.h>

//*****************************************************************************
//
// InstallWordBreakProc
//
// Ensure cursor stopping on backslashes in paths
//
// Implementation mess, avoiding stupid behavior of windows.
// Extracted and translated to C from IE 5.5 / BROWSEUI.DLL.
//

BOOL IsCharacterDelimiter(char ch)
{
    return ch == ' ' || ch == '/' || ch == '\\' || ch == ';' || ch == ',' || ch == '.';
}

int CALLBACK
EditWordBreakProc(LPTSTR text, int current, int textLen, int code)
{
    CALL_STACK_MESSAGE5("EditWordBreakProc(%s, %d, %d, %d)", text, current, textLen, code);
    if (textLen == 0)
        return 0;
    static BOOL gRightBreak = FALSE;
    BOOL ebp_8 = FALSE;
    char* ebp_10 = NULL;
    char* esi = text + current;
    switch (code)
    {
    case WB_LEFT:
    {
        do
        {
            esi = CharPrev(text, esi);
            if (esi == text)
                break;
            if (!IsCharacterDelimiter(*esi))
            {
                gRightBreak = FALSE;
                ebp_8 = TRUE;
                continue;
            }
            if (gRightBreak)
                break;
            if (ebp_8)
                break;
        } while (1);
        if (esi - text <= 0)
            return 0;
        if (esi - text >= textLen)
            return (int)(esi - text);
        return (int)(esi - text + 1);
    }

    case WB_RIGHT:
    {
        gRightBreak = FALSE;
        BOOL edi = !IsCharacterDelimiter(*esi);
        ebp_10 = text + textLen;
        if (esi == ebp_10)
            return (int)(esi - text);
        do
        {
            esi = CharNext(esi);
            if (esi == ebp_10)
                return (int)(esi - text);

            if (IsCharacterDelimiter(*esi))
                edi = FALSE;
            else if (!edi)
                return (int)(esi - text);
        } while (1);
        return 0;
    }

    case WB_ISDELIMITER:
    {
        gRightBreak = TRUE;
        return IsCharacterDelimiter(text[current]);
    }
    }
    return textLen;
}

int CALLBACK
EditWordBreakProcUNICODE(LPTSTR text, int current, int textLen, int code)
{
    CALL_STACK_MESSAGE5("EditWordBreakProcUNICODE(%s, %d, %d, %d)", text, current, textLen, code);
    if (textLen == 0)
        return 0;

    char buff[10000];
    // Convert the String to ANSI
    WideCharToMultiByte(CP_ACP, 0, (wchar_t*)text, textLen, buff, 10000, NULL, NULL);
    buff[10000 - 1] = 0;
    text = buff;

    static BOOL gRightBreak = FALSE;
    BOOL ebp_8 = FALSE;
    char* ebp_10 = NULL;
    char* esi = text + current;
    switch (code)
    {
    case WB_LEFT:
    {
        do
        {
            esi = CharPrev(text, esi);
            if (esi == text)
                break;
            if (!IsCharacterDelimiter(*esi))
            {
                gRightBreak = FALSE;
                ebp_8 = TRUE;
                continue;
            }
            if (gRightBreak)
                break;
            if (ebp_8)
                break;
        } while (1);
        if (esi - text <= 0)
            return 0;
        if (esi - text >= textLen)
            return (int)(esi - text);
        return (int)(esi - text + 1);
    }

    case WB_RIGHT:
    {
        gRightBreak = FALSE;
        BOOL edi = !IsCharacterDelimiter(*esi);
        ebp_10 = text + textLen;
        if (esi == ebp_10)
            return (int)(esi - text);
        do
        {
            esi = CharNext(esi);
            if (esi == ebp_10)
                return (int)(esi - text);

            if (IsCharacterDelimiter(*esi))
                edi = FALSE;
            else if (!edi)
                return (int)(esi - text);
        } while (1);
        return 0;
    }

    case WB_ISDELIMITER:
    {
        gRightBreak = TRUE;
        return IsCharacterDelimiter(text[current]);
    }
    }
    return textLen;
}

const char* BACKSPACE_SUBCLASSPROC = "SALBSSubClass";
int CALLBACK EditWordBreakProc(LPTSTR text, int current, int textLen, int code);

LRESULT CALLBACK
BSHandlerSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WNDPROC OldWndProc = (WNDPROC)GetProp(hwnd, BACKSPACE_SUBCLASSPROC);
    if (OldWndProc == NULL)
    {
        TRACE_E("BSHandlerSubclassProc: OldWndProc == NULL");
        return 0;
    }
    switch (message)
    {
    case WM_CHAR:
    {
        if (wParam == 127) // discard the "Ctrl+Backspace" character
            return 0;      // we processed
        break;
    }

    case WM_KEYDOWN:
    {
        // Handles Ctrl+Backspace to delete a word
        if (wParam == VK_BACK)
        {
            BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (controlPressed && !altPressed && !shiftPressed)
            {
                int iStart, iEnd;
                SendMessage(hwnd, EM_GETSEL, (WPARAM)&iStart, (LPARAM)&iEnd);

                // if there is a selection, we will delete it and place the cursor at the end
                if (iStart != iEnd)
                {
                    SendMessage(hwnd, EM_SETSEL, iEnd, iEnd);
                    iStart = iEnd;
                }
                //          if (iStart == iEnd) // nothing should be selected
                //          {
                char buff[10000];
                int len = GetWindowTextLength(hwnd);
                if (len >= 10000 - 1)
                    break;
                SendMessage(hwnd, WM_GETTEXT, 10000, (LPARAM)buff);

                // delete the word
                iStart = EditWordBreakProc(buff, iStart, iStart + 1, WB_LEFT);
                SendMessage(hwnd, EM_SETSEL, iStart, iEnd);
                SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM) "");
                //          }
                return 0; // we processed
            }
        }
        break;
    }

    case WM_DESTROY:
    {
        // we will sweep the stored OldWndProc behind us
        WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)OldWndProc);

        RemoveProp(hwnd, BACKSPACE_SUBCLASSPROC);
        break;
    }
    }
    return CallWindowProc(OldWndProc, hwnd, message, wParam, lParam);
}

// We will not use it for the WinLib subclass to avoid cluttering it
// (some windows we are supposed to connect to are already or will be under WinLib)
BOOL AttachBackspaceHandler(HWND hwndEdit)
{
    WNDPROC oldWndProc = (WNDPROC)GetWindowLongPtr(hwndEdit, GWLP_WNDPROC);
    if (SetProp(hwndEdit, BACKSPACE_SUBCLASSPROC, (HANDLE)oldWndProc))
    {
        SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)BSHandlerSubclassProc);
        return TRUE;
    }
    return FALSE;
}

BOOL InstallWordBreakProc(HWND hWindow)
{
    CALL_STACK_MESSAGE2("InstallWordBreakProc(0x%p)", hWindow);

    if (hWindow == NULL)
    {
        TRACE_E("InstallWordBreakProc: hWindow == NULL");
        return FALSE;
    }

    char className[31];
    className[0] = 0;
    if (GetClassName(hWindow, className, 30) == 0 || StrICmp(className, "edit") != 0)
    {
        // It could be a combobox, let's try to access the internal edit
        hWindow = GetWindow(hWindow, GW_CHILD);
        if (hWindow == NULL || GetClassName(hWindow, className, 30) == 0 || StrICmp(className, "edit") != 0)
        {
            TRACE_E("InstallWordBreakProc: edit window was not found ClassName is " << className);
            return FALSE;
        }
    }
    // Under Windows XP and .NET with common controls 6, EditWordBreakProc works with UNICODE text
    if (CCVerMajor >= 6)
        SendMessage(hWindow, EM_SETWORDBREAKPROC, NULL, (LPARAM)EditWordBreakProcUNICODE);
    else
        SendMessage(hWindow, EM_SETWORDBREAKPROC, NULL, (LPARAM)EditWordBreakProc);

    // Ctrl+Backspace for deleting by words
    if (!AttachBackspaceHandler(hWindow))
        TRACE_E("AttachBackspaceHandler on hWnd=0x" << hWindow);

    return TRUE;
}

// returns TRUE if it is the "cd *" command
BOOL IsChangeDirAttempt(const char* text)
{
    while (*text == ' ')
        text++;
    return StrNICmp(text, "cd ", 3) == 0;
}

int GetCmdLineLimit()
{
    /*    Measured limits when running via COMSPEC:
    (4094 + length of the command string)  W2K (the length of COMSPEC does not matter)
    (8190 + length of the command string)  XP (the length of COMSPEC does not matter)
    8156                          Vista + Win7 when COMSPEC=C:\Windows\system32\cmd.exe (depends on the length of COMSPEC: longer COMSPEC = smaller limit)*/

#if SALCMDLINE_MAXLEN != 8192 // maximum value that GetCmdLineLimit() can return
#pragma message(__FILE__ " ERROR: SALCMDLINE_MAXLEN != 8192. SALCMDLINE_MAXLEN and GetCmdLineLimit() must contain the same maximal value!")
#endif

    if (WindowsXP64AndLater) // XP64 + Vista + Win7 + ...
    {
        char cmd[MAX_PATH];
        if (!GetEnvironmentVariable("COMSPEC", cmd, MAX_PATH))
            cmd[0] = 0;
        AddDoubleQuotesIfNeeded(cmd, MAX_PATH); // CreateProcess wants the name with spaces in quotes (otherwise it tries various variants, see help)
        return 8191 - lstrlen(cmd) - 6;         // 6 = strlen(" /K ") + 2 (double quotes around the command itself)
    }
    else
        return 8192; // XP
}

//
// ****************************************************************************
// CEditLine
//

CEditLine::CEditLine()
    : CWindow(ooStatic)
{
    SkipCharacter = FALSE;
    SelChangeDisabled = FALSE;
}

void CEditLine::InsertText(char* s)
{
    SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)s);
}

BOOL SkipNextSysCharacter = FALSE;

LRESULT
CEditLine::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CEditLine::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CHAR:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        if (MainWindow->EditWindow->Dropped())
            break;

        if (SkipCharacter)
            return 0;
        switch ((TCHAR)wParam)
        {
        case '\t': // change panel
        {
            MainWindow->ChangePanel();
            return 0;
        }

        case '\r':
        {
            if (SendMessage(HWindow, WM_GETTEXTLENGTH, 0, 0) == 0)
                MainWindow->GetActivePanel()->CtrlPageDnOrEnter(VK_RETURN);
            else
            {
                char cmdLine[SALCMDLINE_MAXLEN + 1];
                SendMessage(HWindow, WM_GETTEXT, SALCMDLINE_MAXLEN + 1, (LPARAM)cmdLine);

                MainWindow->SetDefaultDirectories();

                char command[SALCMDLINE_MAXLEN + 1];
                command[0] = 0;
                int selFrom = 0;
                int selTo = 0;

                BOOL executed = FALSE;
                CFilesWindow* panel = MainWindow->GetActivePanel();
                if (panel->Is(ptDisk)) // running commands on disk -> running in DOS Prompt
                {
                    // People are used to changing the path in the panel using the command line from TC and other file managers
                    // we will try to unlearn it
                    if (IsChangeDirAttempt(cmdLine))
                    {
                        if (Configuration.CnfrmChangeDirTC)
                        {
                            BOOL dontShow = !Configuration.CnfrmChangeDirTC;

                            MSGBOXEX_PARAMS params;
                            memset(&params, 0, sizeof(params));
                            params.HParent = HWindow;
                            params.Flags = MB_OK | MB_ICONINFORMATION;
                            params.Caption = LoadStr(IDS_INFOTITLE);
                            params.Text = LoadStr(IDS_CHANGEDIR_TC_HINT);
                            params.CheckBoxText = LoadStr(IDS_DONTSHOWAGAIN2);
                            params.CheckBoxValue = &dontShow;
                            SalMessageBoxEx(&params);
                            Configuration.CnfrmChangeDirTC = !dontShow;
                        }
                        // Let the command fall through to the shell, so we don't complicate the text of the message box
                    }

                    char cmd[SALCMDLINE_MAXLEN + MAX_PATH]; // COMSPEC will probably be only a few characters long, MAX_PATH is a large reserve (we do not add more for parameters /K, etc.)
                    if (!GetEnvironmentVariable("COMSPEC", cmd, SALCMDLINE_MAXLEN + MAX_PATH))
                        cmd[0] = 0;
                    AddDoubleQuotesIfNeeded(cmd, SALCMDLINE_MAXLEN + MAX_PATH); // CreateProcess wants the name with spaces in quotes (otherwise it tries various variants, see help)

                    if (SystemPolicies.GetMyRunRestricted() &&
                        (!SystemPolicies.GetMyCanRun(cmd) || !SystemPolicies.GetMyCanRun(cmdLine)))
                    {
                        MSGBOXEX_PARAMS params;
                        memset(&params, 0, sizeof(params));
                        params.HParent = HWindow;
                        params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
                        params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
                        params.Text = LoadStr(IDS_POLICIESRESTRICTION);
                        params.ContextHelpId = IDH_GROUPPOLICY;
                        params.HelpCallback = MessageBoxHelpCallback;
                        SalMessageBoxEx(&params);
                        return 0;
                    }

                    panel->UserWorkedOnThisPath = TRUE;

                    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
                    HCURSOR oldCur;
                    if (setWait)
                        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    BOOL cmdTooLong = FALSE;
                    if (strlen(cmd) + 4 < SALCMDLINE_MAXLEN + MAX_PATH)
                    {
                        if ((Configuration.CloseShell != 0) ^ ((GetKeyState(VK_MENU) & 0x8000) != 0))
                            strcat(cmd, " /C "); // to close the shell
                        else
                            strcat(cmd, " /K "); // to keep the shell active
                    }
                    else
                        cmdTooLong = TRUE;
                    if (strlen(cmd) + strlen(cmdLine) + 2 < SALCMDLINE_MAXLEN + MAX_PATH)
                    {
                        strcat(cmd, "\"");
                        strcat(cmd, cmdLine); // We need to surround the user input from the command line with quotation marks, otherwise commands containing quotation marks will not work (for example, >>"C:\APPS\WinRAR\UnRAR.exe" e "test.rar"<< will write >>'C:\APPS\WinRAR\UnRAR.exe" e "test.rar' is not recognized<<).
                        strcat(cmd, "\"");
                    }
                    else
                        cmdTooLong = TRUE;

                    STARTUPINFO si;
                    memset(&si, 0, sizeof(STARTUPINFO));
                    si.cb = sizeof(STARTUPINFO);
                    si.lpTitle = LoadStr(IDS_COMMANDSHELL);
                    si.dwFlags = STARTF_USESHOWWINDOW;
                    POINT p;
                    if (MultiMonGetDefaultWindowPos(MainWindow->HWindow, &p))
                    {
                        // if the main window is on a different monitor, we should also open it there
                        // window being created and preferably at the default position (same as on primary)
                        si.dwFlags |= STARTF_USEPOSITION;
                        si.dwX = p.x;
                        si.dwY = p.y;
                    }
                    si.wShowWindow = SW_SHOWNORMAL;

                    PROCESS_INFORMATION pi;

                    BOOL proc_ret = FALSE;
                    DWORD err = 0;
                    if (!cmdTooLong)
                    {
                        CALL_STACK_MESSAGE3("CEditLine::WindowProc::CreateProcess(, %s, , , , , , %s, ,)",
                                            strlen(cmd) > 300 ? "(very long cmd)" : cmd,
                                            MainWindow->GetActivePanel()->GetPath());
                        proc_ret = HANDLES(CreateProcess(NULL, cmd, NULL, NULL, FALSE,
                                                         CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                                                         NULL, MainWindow->GetActivePanel()->GetPath(), &si, &pi));
                        err = GetLastError();
                    }
                    if (cmdTooLong || !proc_ret)
                    {
                        SalMessageBox(HWindow, cmdTooLong ? LoadStr(IDS_TOOLONGPATH) : GetErrorText(err),
                                      LoadStr(IDS_ERROREXECCMDLINE), MB_OK | MB_ICONEXCLAMATION);
                    }
                    else
                    {
                        HANDLES(CloseHandle(pi.hProcess));
                        HANDLES(CloseHandle(pi.hThread));
                        executed = TRUE;
                    }
                    if (setWait)
                        SetCursor(oldCur);
                }
                else
                {
                    if (panel->Is(ptPluginFS) && panel->GetPluginFS()->NotEmpty() &&
                        panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_COMMANDLINE))
                    { // executing commands from FS
                        lstrcpyn(command, cmdLine, SALCMDLINE_MAXLEN + 1);

                        panel->UserWorkedOnThisPath = TRUE;

                        // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                        if (panel->GetPluginFS()->ExecuteCommandLine(HWindow, command, selFrom, selTo))
                        {
                            executed = TRUE;
                        }

                        // increase the thread priority again, operation completed
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                    }
                }

                // we will add a command to the history
                if (executed)
                {
                    if (Configuration.EnableCmdLineHistory)
                    {
                        char** history = Configuration.EditHistory;
                        int from = EDIT_HISTORY_SIZE - 1;
                        int i;
                        for (i = 0; i < EDIT_HISTORY_SIZE; i++)
                            if (history[i] != NULL)
                                if (strcmp(history[i], cmdLine) == 0)
                                {
                                    from = i;
                                    break;
                                }
                        if (from > 0)
                        {
                            char* text = (char*)malloc(strlen(cmdLine) + 1);
                            if (text != NULL)
                            {
                                free(history[from]);
                                for (i = from - 1; i >= 0; i--)
                                    history[i + 1] = history[i];
                                history[0] = text;
                                strcpy(history[0], cmdLine);
                            }
                        }
                    }
                    MainWindow->EditWindow->FillHistory();

                    int l = (int)strlen(command);
                    if (selFrom < 0)
                        selFrom = 0;
                    if (selFrom > l)
                        selFrom = l;
                    if (selTo < 0)
                        selTo = 0;
                    if (selTo > l)
                        selTo = l;
                    SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)command);
                    SendMessage(HWindow, EM_SETSEL, selFrom, selTo);
                }
            }
            return 0;
        }
        }
        break;
    }

    case WM_COPY:
    case WM_CUT:
    case WM_DESTROYCLIPBOARD:
    {
        // change clipboard -> start calculation of clipboard function enablers
        IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
        IdleCheckClipboard = TRUE; // we will also check the clipboard
        break;
    }

    case WM_KILLFOCUS:
    {
        SelChangeDisabled = TRUE;
        LRESULT res = CWindow::WindowProc(uMsg, wParam, lParam);
        SelChangeDisabled = FALSE;
        return res;
    }

    case WM_SETFOCUS:
    {
        SelChangeDisabled = TRUE;
        LRESULT res = CWindow::WindowProc(uMsg, wParam, lParam);
        SelChangeDisabled = FALSE;
        return res;
    }

    case EM_SETSEL:
    {
        if (SelChangeDisabled)
            return 0;
        break;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_SETCURSOR:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        break;
    }

    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        // 7.10.2009 - AS253_B1_IB34: Manison reported to us that the horizontal scroll does not work for him under Windows Vista.
        // It worked for me (this way). After installing Intellipoint drivers v7 (previously I was on Vista x64
        // no special drivers) WM_MOUSEHWHEEL messages stopped passing through the hook and leaked directly
        // to the focused window; I disabled the hook and now we have to catch messages in windows that may have focus in order to
        // Forwarding occurred.
        // 30.11.2012 - a person appeared on the forum who does not receive WM_MOUSEWHEEL through a message hook (same as before)
        // at Manison in case of WM_MOUSEHWHEEL): https://forum.altap.cz/viewtopic.php?f=24&t=6039
        // So now we will also catch the new message in individual windows, where it can potentially go (according to focus)
        // and then route it so that it is delivered to the window under the cursor, as we always did

        // if the message "recently" came from another channel, we will ignore this channel
        if (MouseWheelMSGThroughHook && MouseWheelMSGTime != 0 && (GetTickCount() - MouseWheelMSGTime < MOUSEWHEELMSG_VALID))
            return 0;
        MouseWheelMSGThroughHook = FALSE;
        MouseWheelMSGTime = GetTickCount();

        MSG msg;
        DWORD pos = GetMessagePos();
        msg.pt.x = GET_X_LPARAM(pos);
        msg.pt.y = GET_Y_LPARAM(pos);
        msg.lParam = lParam;
        msg.wParam = wParam;
        msg.hwnd = HWindow;
        msg.message = uMsg;
        PostMouseWheelMessage(&msg);
        return 0;
    }

    case WM_USER_MOUSEWHEEL:
    {
        // Disable default processing
        return 0;
    }

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        if (MainWindow->EditWindow->Dropped())
            break;
        SkipCharacter = FALSE;
        break;
    }

    case WM_SYSCOMMAND:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        if (MainWindow->EditWindow->Dropped())
            break;
        if (SkipCharacter)
            return 0;
        break;
    }

    case WM_SYSCHAR:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        if (SkipNextSysCharacter)
        {
            SkipNextSysCharacter = FALSE;
            return FALSE;
        }
        return TRUE;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (MainWindow->HasLockedUI())
            return 0;
        SkipCharacter = FALSE;
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (!IsWindowEnabled(MainWindow->HWindow))
            return 0;

        // we will set the SelectedItems variable of the panel to enable selection
        // press Shift+arrows if the focus is here in the edit line
        CFilesWindow* panel = MainWindow->GetActivePanel();
        BOOL firstPress = (lParam & 0x40000000) == 0;
        // j.r.: Dusek found a problem when UP did not reflect to paru to DOWN
        // therefore I will trigger a test on the first press of the SHIFT key
        if (wParam == VK_SHIFT && firstPress && panel->Dirs->Count + panel->Files->Count > 0)
        {
            panel->SelectItems = !panel->GetSel(panel->FocusedIndex);
        }

        if (MainWindow->EditWindow->Dropped())
            break;
        else
        {
            if (wParam == VK_UP || wParam == VK_DOWN)
            {
                // Alt - let the listbox unpack
                if (!controlPressed && altPressed && !shiftPressed)
                    break;
                // Control - allow scrolling without expanding the listbox
                if (controlPressed && !altPressed && !shiftPressed)
                    break;
            }
        }

        if (altPressed && !controlPressed && !shiftPressed)
        {
            // change panel mode
            if (wParam >= '0' && wParam <= '9')
            {
                int index = (int)(wParam - '0');
                if (index == 0)
                    index = 9;
                else
                    index--;
                if (MainWindow->GetActivePanel()->IsViewTemplateValid(index))
                    MainWindow->GetActivePanel()->SelectViewTemplate(index, TRUE, FALSE);
                SkipNextSysCharacter = TRUE; // prevent beeping
                return TRUE;
            }
        }

        if (controlPressed && !shiftPressed && !altPressed)
        {
            // Since Windows Vista, SelectAll works by default, so we will leave select all on them
            if (!WindowsVistaAndLater)
            {
                if (wParam == 'A')
                {
                    SendMessage(HWindow, EM_SETSEL, 0, -1);
                    SkipCharacter = TRUE; // prevent beeping
                    return TRUE;
                }
            }
        }

        /*        if (shiftPressed && controlPressed && !altPressed)
      {
        if (wParam >= 'A' && wParam <= 'Z')
        {
          SkipCharacter = TRUE;
          MainWindow->HandleCtrlLetter((char)wParam);
          return 0;
        }
      }*/

        if (wParam >= '0' && wParam <= '9')
        {
            BOOL exit = FALSE;
            // define hot path
            if (shiftPressed && controlPressed && !altPressed)
            {
                MainWindow->GetActivePanel()->SetUnescapedHotPath((char)wParam == '0' ? 9 : (char)wParam - '1');
                if (!Configuration.HotPathAutoConfig)
                    MainWindow->GetActivePanel()->DirectoryLine->FlashText();
                exit = TRUE;
            }

            // go to hot path

            // I cannot type the characters @, L, $, {, [, ], } on the command line in
            // Open Salamander. With my Danish keyboard these characters all require me
            // to press AltGr+<a digit> (= Ctrl+Alt+<digit>), but to Salamander this has a
            // special meaning, which seems to be: "go to hot path in the non-focused
            // panel". I am not interested in this special Alt-Ctrl-functionality in
            // Salamander, I am definitely more interested in being able to type the
            // mentioned characters on the command line.
            if ((controlPressed && !shiftPressed && !altPressed) /* || // Shift+numbers from edit-line won't work (you need to type '*' and more)
            (Configuration.ShiftForHotPaths && !controlPressed && shiftPressed)*/
            )
            {
                MainWindow->GetActivePanel()->GotoHotPath((char)wParam == '0' ? 9 : (char)wParam - '1');
                /*            if (altPressed)
            MainWindow->GetNonActivePanel()->GotoHotPath((char)wParam == '0' ? 9 : (char)wParam - '1');
          else
            MainWindow->GetActivePanel()->GotoHotPath((char)wParam == '0' ? 9 : (char)wParam - '1');
          */
                exit = TRUE;
            }

            if (exit)
            {
                SkipCharacter = TRUE;
                return 0;
            }
        }

        if (wParam == VK_BACK && (!controlPressed && !altPressed && shiftPressed))
        { // Shift+backspace
            SkipCharacter = TRUE;
            MainWindow->GetActivePanel()->GotoRoot();
            return 0;
        }

        if (controlPressed && !altPressed)
        {
            if (!shiftPressed)
            {
                if (wParam == 0xBF) // Ctrl+'/'
                {
                    SkipCharacter = TRUE;
                    PostMessage(MainWindow->HWindow, WM_COMMAND, CM_DOSSHELL, 0);
                    return 0;
                }

                if (wParam == 0xBB) // Ctrl+'+'
                {
                    SkipCharacter = TRUE;
                    PostMessage(MainWindow->HWindow, WM_COMMAND, CM_ACTIVESELECT, 0);
                    return 0;
                }

                if (wParam == 0xBD) // Ctrl+'-'
                {
                    SkipCharacter = TRUE;
                    PostMessage(MainWindow->HWindow, WM_COMMAND, CM_ACTIVEUNSELECT, 0);
                    return 0;
                }

                if (wParam == VK_BACKSLASH) // Ctrl+'\\'
                {
                    SkipCharacter = TRUE;
                    MainWindow->GetActivePanel()->GotoRoot();
                    return 0;
                }
            }
        }

        switch (wParam)
        {
        case VK_RETURN:
        {
            if (controlPressed && !altPressed) // filename of the selected file to the command line
            {
                SkipCharacter = TRUE;
                char path[MAX_PATH + 1];
                const char* s;
                int l;
                CFilesWindow* p = MainWindow->GetActivePanel();
                if (p->FocusedIndex >= 0 &&
                    p->FocusedIndex < p->Files->Count + p->Dirs->Count)
                {
                    CFileData* file = (p->FocusedIndex < p->Dirs->Count) ? &p->Dirs->At(p->FocusedIndex) : &p->Files->At(p->FocusedIndex - p->Dirs->Count);
                    if (shiftPressed) // user-name
                    {
                        s = (file->DosName == NULL) ? file->Name : file->DosName;
                    }
                    else
                    {
                        s = file->Name;
                    }
                }
                else
                    return 0;
                l = (int)strlen(s);
                memmove(path, s, l);
                path[l++] = ' ';
                path[l] = 0;
                InsertText(path);
                return 0;
            }
            else
            {
                if (shiftPressed)
                {
                    SkipCharacter = TRUE;
                    MainWindow->GetActivePanel()->CtrlPageDnOrEnter(wParam);
                    return 0;
                }
                else
                {
                    if (altPressed)
                    {
                        SendMessage(HWindow, WM_CHAR, '\r', 0);
                        SkipCharacter = TRUE;
                        return 0;
                    }
                }
            }
            break;
        }

        case VK_INSERT:
        {
            if (!shiftPressed && !controlPressed && altPressed ||
                shiftPressed && !controlPressed && altPressed)
            { // clipboard: (full) name of focused item
                CCopyFocusedNameModeEnum mode;
                if (!shiftPressed && !controlPressed && altPressed)
                    mode = cfnmFull;
                else
                    mode = cfnmShort;
                MainWindow->GetActivePanel()->CopyFocusedNameToClipboard(mode);
                return 0;
            }
            if (!shiftPressed && controlPressed && altPressed)
            { // clipboard: current full path
                MainWindow->GetActivePanel()->CopyCurrentPathToClipboard();
                return 0;
            }
            if (shiftPressed && controlPressed && !altPressed)
            { // clipboard: (full) UNC name of focused item
                MainWindow->GetActivePanel()->CopyFocusedNameToClipboard(cfnmUNC);
                return 0;
            }
            if (controlPressed || shiftPressed)
                break;
            MainWindow->GetActivePanel()->SelectFocusedIndex();
            return 0;
        }

        case VK_ESCAPE:
        case VK_TAB:
        {
            if (wParam == VK_ESCAPE || controlPressed)
            {
                if (wParam == VK_ESCAPE)
                {
                    // People want compatibility with WinCmd, NC, FAR -- deleting content on Escape
                    SetWindowText(HWindow, "");
                }
                SkipCharacter = TRUE;
                MainWindow->FocusPanel(MainWindow->GetActivePanel());
                return 0;
            }
            break;
        }

        case VK_LBRACKET:
        case VK_RBRACKET:
        case VK_SPACE:
        {
            if (controlPressed && !altPressed)
            {
                SkipCharacter = TRUE;
                char path[MAX_PATH];
                const char* s;
                switch (wParam)
                {
                case VK_LBRACKET:
                    s = MainWindow->LeftPanel->Is(ptDisk) ? MainWindow->LeftPanel->GetPath() : NULL;
                    break;
                case VK_RBRACKET:
                    s = MainWindow->RightPanel->Is(ptDisk) ? MainWindow->RightPanel->GetPath() : NULL;
                    break;
                default:
                    s = MainWindow->GetActivePanel()->Is(ptDisk) ? MainWindow->GetActivePanel()->GetPath() : NULL;
                    break;
                }
                if (s != NULL)
                {
                    if (shiftPressed) // dos-path
                    {
                        if (!GetShortPathName(s, path, MAX_PATH))
                        {
                            strcpy(path, s);
                        }
                    }
                    else
                    {
                        strcpy(path, s);
                    }
                    SalPathAddBackslash(path, MAX_PATH);
                    InsertText(path);
                }
                return 0;
            }
            break;
        }

        case VK_UP:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        {
            CFilesWindow* activePanel = MainWindow->GetActivePanel();
            LRESULT dummy;
            activePanel->OnSysKeyDown(WM_SYSKEYDOWN, wParam, lParam, &dummy);
            return 0;
        }
        }

        // we give the opportunity to plugins (we do the same in panels)
        if (Plugins.HandleKeyDown(wParam, lParam, MainWindow->GetActivePanel(), MainWindow->HWindow))
        {
            return 0;
        }

        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

class CEditDropTarget : public IDropTarget
{
private:
    long RefCount;                    // object lifespan
    IDataObject* DataObject;          // IDataObject that entered the drag
    IDataObject* ForbiddenDataObject; // IDataObject, which we do not take (we are its source)
    BOOL UseUnicode;                  // is the DataObject unicode text? (otherwise we will try ANSI text)
    CEditLine* EditLine;              // editline that we are working on
    int EditWidth;
    int EditHeight;
    char* TextBuff;
    int TextLen;
    int OldIsertMarkX;

public:
    CEditDropTarget(CEditLine* editLine)
    {
        RefCount = 1;
        DataObject = NULL;
        ForbiddenDataObject = NULL;
        EditLine = editLine;
        OldIsertMarkX = -1;
        TextBuff = NULL;
        UseUnicode = TRUE;
    }

    virtual ~CEditDropTarget()
    {
        if (TextBuff != NULL)
        {
            TRACE_E("~CEditDropTarget(): unexpected situation: TextBuff != NULL");
            free(TextBuff);
            TextBuff = NULL;
        }
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of object!");
    }

    void DrawInsertMark(int x)
    {
        HDC hDC = HANDLES(GetDC(EditLine->HWindow));
        int oldRop = SetROP2(hDC, R2_NOT);
        MoveToEx(hDC, x, 0, NULL);
        LineTo(hDC, x, EditHeight);
        MoveToEx(hDC, x - 2, 0, NULL);
        LineTo(hDC, x + 3, 0);
        MoveToEx(hDC, x - 2, EditHeight - 1, NULL);
        LineTo(hDC, x + 3, EditHeight - 1);
        SetROP2(hDC, oldRop);
        HANDLES(ReleaseDC(EditLine->HWindow, hDC));
    }

    void SetInsertMark(int x)
    {
        if (x != OldIsertMarkX)
        {
            if (ImageDragging)
                ImageDragShow(FALSE);
            if (OldIsertMarkX != -1)
                DrawInsertMark(OldIsertMarkX); // turn off
            if (x != -1)
                DrawInsertMark(x); // turn on
            OldIsertMarkX = x;
            if (ImageDragging)
                ImageDragShow(TRUE);
        }
    }

    BOOL HitTest(POINTL pt, BOOL showInsertMark, int* xPos)
    {
        POINT p;
        p.x = pt.x;
        p.y = pt.y;
        ScreenToClient(EditLine->HWindow, &p);
        if (TextBuff != NULL && p.x >= 0 && p.x <= EditWidth && p.y >= 0 && p.y <= EditHeight)
        {
            // skipped message EM_POSFROMCHAR - I have to bypass it
            LRESULT pos = SendMessage(EditLine->HWindow, EM_CHARFROMPOS, 0, MAKELPARAM(p.x, p.y));
            int myPos = *xPos = LOWORD(pos);
            BOOL byPass = FALSE;
            if (TextLen > 0 && myPos == TextLen)
            {
                myPos--;
                byPass = TRUE;
            }
            pos = SendMessage(EditLine->HWindow, EM_POSFROMCHAR, myPos, 0);
            short x = (short)LOWORD(pos);
            if (x < 0)
                x = 0;
            if (x > EditWidth)
                x = EditWidth;
            if (byPass)
            {
                HFONT hFont = (HFONT)SendMessage(EditLine->HWindow, WM_GETFONT, 0, 0);
                HDC hDC = HANDLES(GetDC(EditLine->HWindow));
                HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
                SIZE sz;
                GetTextExtentPoint32(hDC, TextBuff + TextLen - 1, 1, &sz);
                SelectObject(hDC, hOldFont);
                HANDLES(ReleaseDC(EditLine->HWindow, hDC));
                x += (short)sz.cx;
            }
            if (showInsertMark)
                SetInsertMark(x);
            return TRUE;
        }
        return FALSE;
    }

    BOOL InsertText(POINTL pt, const char* text)
    {
        int xPos;
        if (HitTest(pt, FALSE, &xPos))
        {
            SetInsertMark(-1);
            char buff[10000];
            lstrcpyn(buff, text, 10000);
            char* start = buff;
            if ((GetKeyState(VK_MENU) & 0x8000) != 0)
            {
                // we don't want the whole way - I'll handle it
                int len = lstrlen(buff);
                if (len > 2)
                {
                    if (buff[len - 1] == '\\')
                    {
                        buff[len - 1] = 0;
                        len--;
                    }
                    char* p = buff + len - 1;
                    while (p >= buff && *p != '\\')
                        p--;
                    if (p >= buff && *p == '\\')
                        start = p + 1;
                }
            }
            if (ImageDragging)
                ImageDragShow(FALSE);
            SendMessage(EditLine->HWindow, EM_SETSEL, xPos, xPos);
            SendMessage(EditLine->HWindow, EM_REPLACESEL, TRUE, (LPARAM)start);
            UpdateWindow(EditLine->HWindow);
            if (ImageDragging)
                ImageDragShow(TRUE);
            return TRUE;
        }
        return FALSE;
    }

    void SetForbiddenDataObject(IDataObject* forbiddenDataObject)
    {
        ForbiddenDataObject = forbiddenDataObject;
    }

    // returns the directory/file (must be exactly one)
    BOOL GetNameFromDataObject(IDataObject* pDataObject, char* path)
    {
        FORMATETC formatEtc;
        formatEtc.cfFormat = RegisterClipboardFormat(SALCF_FAKE_REALPATH);
        formatEtc.ptd = NULL;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        STGMEDIUM stgMedium;
        stgMedium.tymed = TYMED_HGLOBAL;
        stgMedium.hGlobal = NULL;
        stgMedium.pUnkForRelease = NULL;

        if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
        {
            BOOL havePath = FALSE;
            if (path != NULL)
                path[0] = 0;
            if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
            {
                char* data = (char*)HANDLES(GlobalLock(stgMedium.hGlobal));
                if (data != NULL)
                {
                    havePath = data[0] != 0 && data[1] != 0;
                    if (data[0] != 0 && path != NULL)
                        lstrcpyn(path, data + 1, MAX_PATH);
                    HANDLES(GlobalUnlock(stgMedium.hGlobal));
                }
            }
            ReleaseStgMedium(&stgMedium);
            return havePath;
        }

        formatEtc.cfFormat = CF_HDROP;
        formatEtc.ptd = NULL;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        stgMedium.tymed = TYMED_HGLOBAL;
        stgMedium.hGlobal = NULL;
        stgMedium.pUnkForRelease = NULL;

        BOOL ret = FALSE;
        if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
        {
            if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
            {
                DROPFILES* data = (DROPFILES*)HANDLES(GlobalLock(stgMedium.hGlobal));
                if (data != NULL)
                {
                    if (data->fWide)
                    {
                        const wchar_t* fileW = (wchar_t*)(((char*)data) + data->pFiles);
                        int l = lstrlenW(fileW);
                        if (l < MAX_PATH && *(fileW + l + 1) == 0)
                        {
                            if (path != NULL)
                            {
                                WideCharToMultiByte(CP_ACP, 0, fileW, l + 1, path, MAX_PATH, NULL, NULL);
                                path[MAX_PATH - 1] = 0;
                            }
                            ret = TRUE;
                        }
                    }
                    else
                    {
                        const char* fileA = ((char*)data) + data->pFiles;
                        int l = (int)strlen(fileA);
                        if (l < MAX_PATH && *(fileA + l + 1) == 0)
                        {
                            if (path != NULL)
                                lstrcpyn(path, fileA, MAX_PATH);
                            ret = TRUE;
                        }
                    }

                    HANDLES(GlobalUnlock(stgMedium.hGlobal));
                }
            }
            ReleaseStgMedium(&stgMedium);
        }
        /* unnecessarily strict check removed - unable to drop pagefile.sys
      if (ret && path != NULL)
      {
        DWORD attrs = SalGetFileAttributes(path);
        if (attrs == 0xFFFFFFFF)
          ret = FALSE;
      }*/
        return ret;
    }

    STDMETHOD(QueryInterface)
    (REFIID refiid, void FAR* FAR* ppv)
    {
        if (refiid == IID_IUnknown || refiid == IID_IDropTarget)
        {
            *ppv = this;
            AddRef();
            return NOERROR;
        }
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
    }

    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // We must not access the object, it no longer exists
        }
        return RefCount;
    }

    STDMETHOD(DragEnter)
    (IDataObject* pDataObject, DWORD grfKeyState,
     POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragEnter(pt.x, pt.y);
        if (DataObject != NULL)
            DataObject->Release();
        DataObject = pDataObject;
        DataObject->AddRef();

        SetInsertMark(-1);

        // if our panel is also a source, disable paste
        if (DataObject == ForbiddenDataObject)
        {
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        RECT r;
        GetClientRect(EditLine->HWindow, &r);
        EditWidth = r.right;
        EditHeight = r.bottom;

        if (TextBuff != NULL)
        {
            TRACE_E("CEditDropTarget::DragEnter: Unexpected situation: TextBuff != NULL");
            free(TextBuff);
        }
        TextLen = GetWindowTextLength(EditLine->HWindow);
        TextBuff = (char*)malloc(TextLen + 1);
        if (TextBuff != NULL)
            TextLen = GetWindowText(EditLine->HWindow, TextBuff, TextLen + 1);
        else
            TextLen = 0;

        // check if there is text on the clipboard
        FORMATETC formatEtc;
        ZeroMemory(&formatEtc, sizeof(formatEtc));
        formatEtc.cfFormat = CF_UNICODETEXT;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;
        UseUnicode = TRUE;
        HRESULT textRes;
        if ((textRes = pDataObject->QueryGetData(&formatEtc)) != S_OK)
        {
            formatEtc.cfFormat = CF_TEXT;
            UseUnicode = FALSE;
            textRes = pDataObject->QueryGetData(&formatEtc);
        }
        if (textRes == S_OK || GetNameFromDataObject(DataObject, NULL))
        {
            *pdwEffect = DROPEFFECT_COPY;
            return S_OK;
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHOD(DragOver)
    (DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragMove(pt.x, pt.y);
        if (DataObject != NULL)
        {
            // if our panel is also a source, disable paste
            if (DataObject == ForbiddenDataObject)
            {
                *pdwEffect = DROPEFFECT_NONE;
                return S_OK;
            }
            // check if there is text on the clipboard
            FORMATETC formatEtc;
            ZeroMemory(&formatEtc, sizeof(formatEtc));
            formatEtc.cfFormat = UseUnicode ? CF_UNICODETEXT : CF_TEXT;
            formatEtc.dwAspect = DVASPECT_CONTENT;
            formatEtc.lindex = -1;
            formatEtc.tymed = TYMED_HGLOBAL;
            if (DataObject->QueryGetData(&formatEtc) == S_OK || GetNameFromDataObject(DataObject, NULL))
            {
                int xPosDummy;
                if (HitTest(pt, TRUE, &xPosDummy))
                {
                    *pdwEffect = DROPEFFECT_COPY;
                    return S_OK;
                }
            }
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHOD(DragLeave)
    ()
    {
        if (ImageDragging)
            ImageDragLeave();
        if (DataObject != NULL)
        {
            DataObject->Release();
            DataObject = NULL;
        }
        SetInsertMark(-1);
        if (TextBuff != NULL)
        {
            free(TextBuff);
            TextBuff = NULL;
        }
        return S_OK;
    }

    STDMETHOD(Drop)
    (IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
     DWORD* pdwEffect)
    {
        // trying to extract text from DataObject
        FORMATETC formatEtc;
        ZeroMemory(&formatEtc, sizeof(formatEtc));
        formatEtc.cfFormat = UseUnicode ? CF_UNICODETEXT : CF_TEXT;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        STGMEDIUM stgMedium;
        ZeroMemory(&stgMedium, sizeof(stgMedium));
        stgMedium.tymed = TYMED_HGLOBAL;

        if (ImageDragging)
            ImageDragLeave();
        SetInsertMark(-1);

        if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
        {
            char* path = (char*)HANDLES(GlobalLock(stgMedium.hGlobal));
            if (path != NULL)
            {
                // change path
                if (UseUnicode)
                    path = ConvertAllocU2A((const WCHAR*)path, -1);
                if (path != NULL)
                {
                    InsertText(pt, path);
                    if (UseUnicode)
                        free(path);
                }
                HANDLES(GlobalUnlock(stgMedium.hGlobal));
            }
        }
        else
        {
            char path[2 * MAX_PATH];
            if (GetNameFromDataObject(pDataObject, path)) // Here, up to 'MAX_PATH' characters are placed into 'path'
            {
                // change path
                if (!IsPluginFSPath(path))
                {
                    int l = (int)strlen(path);
                    if (l > 0 && path[l - 1] == '\\')
                        path[--l] = 0;             // '\\' at the end is not welcome
                    if (l == 2 && path[0] != '\\') // There must be a '\\' at the end of the UNC root path
                    {
                        path[l++] = '\\';
                        path[l] = 0;
                    }
                }
                else
                    PluginFSConvertPathToExternal(path); // here it is necessary for there to be MAX_PATH characters in 'path' after fs-name (that's why it's 2 * MAX_PATH)

                InsertText(pt, path);
            }
        }

        if (DataObject != NULL)
        {
            DataObject->Release();
            DataObject = NULL;
        }
        *pdwEffect = DROPEFFECT_NONE;
        if (TextBuff != NULL)
        {
            free(TextBuff);
            TextBuff = NULL;
        }
        return S_OK;
    }
};

void CEditLine::RegisterDragDrop()
{
    CALL_STACK_MESSAGE1("CEditLine::RegisterDragDrop()");
    CEditDropTarget* dropTarget = new CEditDropTarget(this);
    if (dropTarget != NULL)
    {
        if (HANDLES(RegisterDragDrop(HWindow, dropTarget)) != S_OK)
        {
            TRACE_E("RegisterDragDrop error.");
        }
        //    else
        //      IDropTargetPtr = dropTarget;
        dropTarget->Release(); // RegisterDragDrop called AddRef()
    }
}

void CEditLine::RevokeDragDrop()
{
    HANDLES(RevokeDragDrop(HWindow));
}

//
// ****************************************************************************
// CInnerText
//

CInnerText::CInnerText(CEditWindow* editWindow)
    : CWindow(ooStatic)
{
    Allocated = 0;
    Message = NULL;
    Width = 0;
    Height = 0;
    EditWindow = editWindow;
}

CInnerText::~CInnerText()
{
    if (Message != NULL)
        free(Message);
}

LRESULT
CInnerText::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CInnerText::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_SIZE:
    {
        Width = LOWORD(lParam);
        Height = HIWORD(lParam);
        ItemBitmap.Enlarge(Width, Height); // Allocation of bitmap in ItemBitmap.HMemDC
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));

        HDC dc = ItemBitmap.HMemDC;

        if (Message != NULL)
        {
            COLORREF sysBkColor = EditWindow->Enabled ? COLOR_WINDOW : COLOR_BTNFACE;
            COLORREF sysTxColor = EditWindow->Enabled ? COLOR_WINDOWTEXT : COLOR_BTNSHADOW;
            int oldColor = SetTextColor(dc, GetSysColor(sysTxColor));
            HFONT oldFont = (HFONT)SelectObject(dc, EnvFont);
            RECT r;
            GetClientRect(HWindow, &r);
            FillRect(dc, &r, (HBRUSH)(UINT_PTR)(sysBkColor + 1));
            int oldBkMode = SetBkMode(dc, TRANSPARENT);
            r.right -= TXEL_SPACE - 1; // if the font is bold, the resulting text is not cheerful - hence this correction

            // PathCompactPath() works better than the combination of DT_PATH_ELLIPSIS with DT_END_ELLIPSIS (due to the misbehaving last character)
            char buff[2 * MAX_PATH];
            strncpy_s(buff, _countof(buff), Message, _TRUNCATE);
            PathCompactPath(dc, buff, r.right - r.left);

            DrawText(dc, buff, -1, &r,
                     /*DT_END_ELLIPSIS | DT_PATH_ELLIPSIS |*/ DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SetBkMode(dc, oldBkMode);
            SetTextColor(dc, oldColor);
            SelectObject(dc, oldFont);
            BitBlt(ps.hdc, 0, 0, Width, Height, dc, 0, 0, SRCCOPY);
        }

        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

void CInnerText::UpdateControl()
{
    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
}

BOOL CInnerText::SetText(const char* txt)
{
    CALL_STACK_MESSAGE2("CInnerText::SetText(%s)", txt);
    if (txt == NULL)
        txt = ">";
    int l = (int)strlen(txt);
    int lm = (Message == NULL) ? 0 : ((int)strlen(Message) - 1);
    if (Allocated < l + 2)
    {
        if (Message != NULL)
            free(Message);
        Message = (char*)malloc(l + 2);
        if (Message == NULL)
        {
            TRACE_E(LOW_MEMORY);
            Allocated = 0;
            return FALSE;
        }
        Allocated = l + 2;
    }
    if (lm == l && strncmp(Message, txt, l) == 0)
        return FALSE;
    memmove(Message, txt, l);
    Message[l] = '>';
    Message[l + 1] = 0;
    return TRUE;
}

int CInnerText::GetNeededWidth()
{
    if (Message == NULL)
        return 0;

    HDC dc = HANDLES(GetDC(NULL));
    if (dc != NULL)
    {
        HFONT old = (HFONT)SelectObject(dc, EnvFont);
        SIZE s;
        GetTextExtentPoint32(dc, Message, (int)strlen(Message), &s);
        SelectObject(dc, old);
        HANDLES(ReleaseDC(NULL, dc));
        return s.cx + TXEL_SPACE;
    }
    return 0;
}

//
// ****************************************************************************
// CEditWindow
//

CEditWindow::CEditWindow()
    : CWindow(ooStatic)
{
    EditLine = new CEditLine();
    Text = new CInnerText(this);
    LastText = NULL;
    Enabled = TRUE;
    Tracking = FALSE;
}

CEditWindow::~CEditWindow()
{
    if (EditLine != NULL)
        delete EditLine;
    if (Text != NULL)
        delete Text;
    ResetStoredContent();
}

BOOL CEditWindow::Create(HWND hParent, int childID)
{
    CALL_STACK_MESSAGE2("CEditWindow::Create(, %d)", childID);
    HWND hWnd = CreateEx(0,
                         "ComboBox",
                         "",
                         WS_CHILD | WS_VSCROLL | WS_CLIPSIBLINGS |
                             CBS_AUTOHSCROLL | CBS_HASSTRINGS | CBS_DROPDOWN,
                         0, 0, 0, 0,
                         hParent,
                         (HMENU)(UINT_PTR)childID,
                         HInstance,
                         this);
    if (hWnd != NULL)
    {
        if (EditLine != NULL)
        {
            EditLine->AttachToWindow(GetWindow(HWindow, GW_CHILD));
            EditLine->RegisterDragDrop();
            InstallWordBreakProc(EditLine->HWindow);
        }
        if (Text != NULL)
        {
            if (!Text->Create("STATIC",
                              "",
                              WS_VISIBLE | WS_CHILD | SS_RIGHT | SS_NOPREFIX,
                              0, 0, 0, 0,
                              HWindow,
                              (HMENU)IDC_DIRTEXT,
                              HInstance,
                              Text))
            {
                TRACE_E("Unable to create text control.");
            }
        }
    }
    SetFont();
    SendMessage(HWindow, CB_LIMITTEXT, GetCmdLineLimit(), 0);
    FillHistory();
    EnableWindow(HWindow, Enabled);
    return hWnd != NULL;
}

void CEditWindow::FillHistory()
{
    SendMessage(HWindow, CB_RESETCONTENT, 0, 0);
    if (Configuration.EnableCmdLineHistory)
    {
        int i;
        for (i = 0; i < EDIT_HISTORY_SIZE; i++)
            if (Configuration.EditHistory[i] != NULL)
                SendMessage(HWindow, CB_ADDSTRING, 0, (LPARAM)Configuration.EditHistory[i]);
    }
}

BOOL CEditWindow::Dropped()
{
    return (BOOL)SendMessage(HWindow, CB_GETDROPPEDSTATE, 0, 0);
}

void CEditWindow::SetFont()
{
    if (EditLine != NULL && EditLine->HWindow != NULL &&
        Text != NULL && Text->HWindow != NULL)
    {
        SendMessage(HWindow, WM_SETFONT, (WPARAM)EnvFont, 0);
        SendMessage(EditLine->HWindow, EM_SETMARGINS, (WPARAM)EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);

        RECT r;
        GetClientRect(HWindow, &r);
        ResizeChilds(r.right - r.left, r.bottom - r.top, FALSE);
        Text->UpdateControl();
    }
}

void CEditWindow::SetDirectory(const char* dir)
{
    if (Text != NULL && !Text->SetText(dir))
        return;
    if (HWindow != NULL)
    {
        RECT r;
        GetClientRect(HWindow, &r);
        ResizeChilds(r.right - r.left, r.bottom - r.top, FALSE);
        if (Text != NULL)
            Text->UpdateControl();
        UpdateWindow(HWindow);
    }
}

#define EL_XBORDER 4
#define EL_YBORDER 1

int CEditWindow::GetNeededHeight()
{
    return 2 * (EL_YBORDER + 3) + EnvFontCharHeight;
}

void CEditWindow::ResizeChilds(int cx, int cy, BOOL repaint)
{
    int textWidth;
    if (Text != NULL && Text->HWindow != NULL)
    {
        textWidth = Text->GetNeededWidth();
        int pulka = (cx - 2 * EL_XBORDER) / 2;
        if (textWidth > pulka)
            textWidth = pulka;
        MoveWindow(Text->HWindow, EL_XBORDER, 4, textWidth,
                   EnvFontCharHeight, repaint);
    }
    else
        textWidth = 0;

    if (EditLine != NULL && EditLine->HWindow != NULL)
        MoveWindow(EditLine->HWindow, EL_XBORDER + textWidth, 4,
                   cx - 2 * EL_XBORDER - textWidth + 1 - GetSystemMetrics(SM_CXVSCROLL),
                   EnvFontCharHeight, repaint);
}

LRESULT
CEditWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CEditWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_SIZE:
    {
        LRESULT result = CWindow::WindowProc(uMsg, wParam, lParam);
        ResizeChilds(LOWORD(lParam), HIWORD(lParam), TRUE);
        return result;
    }

    case WM_DESTROY:
    {
        if (EditLine != NULL)
        {
            EditLine->RevokeDragDrop();
            EditLine->DetachWindow();
        }
        if (Text != NULL)
            Text->DetachWindow();
        break;
    }

    case WM_LBUTTONDOWN:
    {
        int sbWidth = GetSystemMetrics(SM_CXVSCROLL);
        RECT cr;
        GetClientRect(HWindow, &cr);
        if (!SendMessage(HWindow, CB_GETDROPPEDSTATE, 0, 0) &&
            LOWORD(lParam) > cr.right - 3 - sbWidth &&
            LOWORD(lParam) < cr.right - 2 &&
            HIWORD(lParam) > cr.top + 1 &&
            HIWORD(lParam) < cr.bottom - 2)
        {
            Tracking = TRUE;
        }
        break;
    }

    case WM_CAPTURECHANGED:
    case WM_LBUTTONUP:
    {
        Tracking = FALSE;
        break;
    }

    case WM_PAINT:
    {
        RECT cr;
        GetClientRect(HWindow, &cr);
        // Ensure that the two-point frame around the combo is not swept away during painting
        RECT r;
        r.left = 0;
        r.top = 0;
        r.right = cr.right;
        r.bottom = 2;
        ValidateRect(HWindow, &r);
        r.left = 0;
        r.top = cr.bottom - 2;
        r.right = cr.right;
        r.bottom = cr.bottom;
        ValidateRect(HWindow, &r);
        r.left = 0;
        r.top = 2;
        r.right = 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 2;
        r.top = 2;
        r.right = cr.right;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);

        // let's draw our own (one outer gray and inner dimpled)
        HDC hDC = HANDLES(GetDC(HWindow));
        HPEN hOldPen = (HPEN)SelectObject(hDC, BtnFacePen);
        SelectObject(hDC, HANDLES(GetStockObject(NULL_BRUSH)));
        Rectangle(hDC, cr.left, cr.top, cr.right, cr.bottom);
        SelectObject(hDC, BtnShadowPen);
        MoveToEx(hDC, cr.left + 1, cr.bottom - 2, NULL);
        LineTo(hDC, cr.left + 1, cr.top + 1);
        LineTo(hDC, cr.right - 2, cr.top + 1);
        SelectObject(hDC, BtnHilightPen);
        MoveToEx(hDC, cr.right - 2, cr.top + 1, NULL);
        LineTo(hDC, cr.right - 2, cr.bottom - 2);
        LineTo(hDC, cr.left, cr.bottom - 2);

        if (WindowsVistaAndLater)
        {
            // They finally fixed the combobox in Vista so it doesn't flicker during resize
            // we therefore need to "manually" adjust the space between child windows and the edge
            // combobox, otherwise garbage remains there
            (HPEN) SelectObject(hDC, WndPen);
            Rectangle(hDC, cr.left + EL_XBORDER - 1, cr.top + 4 - 1,
                      cr.right - GetSystemMetrics(SM_CXVSCROLL) - 1, cr.bottom - 4 + 1);
        }

        // if any visual style is active, we will not take care of the button
        if (IsAppThemed())
        {
            SelectObject(hDC, hOldPen);
            HANDLES(ReleaseDC(HWindow, hDC));
            break;
        }

        // Ensure that the two-point frame around the button is not swept away during painting
        int sbWidth = GetSystemMetrics(SM_CXVSCROLL);
        r.left = cr.right - 2 - sbWidth;
        r.top = 2;
        r.right = cr.right - 2;
        r.bottom = 4;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 2 - sbWidth;
        r.top = cr.bottom - 4;
        r.right = cr.right - 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 2 - sbWidth;
        r.top = 2;
        r.right = cr.right - 2 - sbWidth + 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);
        r.left = cr.right - 4;
        r.top = 2;
        r.right = cr.right - 2;
        r.bottom = cr.bottom - 2;
        ValidateRect(HWindow, &r);

        // draw our own frame around the button
        HPEN leftTopPen;
        HPEN bottomRightPen;
        POINT pos;
        GetCursorPos(&pos);
        ScreenToClient(HWindow, &pos);
        if (Tracking && pos.x > cr.right - 3 - sbWidth &&
            pos.x < cr.right - 2 &&
            pos.y > cr.top + 1 &&
            pos.y < cr.bottom - 2)
        {
            // draw a depressed frame
            leftTopPen = BtnShadowPen;
            //bottomRightPen = BtnHilightPen;
            bottomRightPen = BtnShadowPen;
        }
        else
        {
            // draw a normal frame
            leftTopPen = BtnHilightPen;
            bottomRightPen = BtnShadowPen;
        }
        SelectObject(hDC, BtnFacePen);
        SelectObject(hDC, HANDLES(GetStockObject(NULL_BRUSH)));
        Rectangle(hDC, cr.right - 2 - sbWidth + 1, 3, cr.right - 3, cr.bottom - 3);
        SelectObject(hDC, leftTopPen);
        MoveToEx(hDC, cr.right - 2 - sbWidth, cr.bottom - 4, NULL);
        LineTo(hDC, cr.right - 2 - sbWidth, cr.top + 2);
        LineTo(hDC, cr.right - 3, cr.top + 2);
        SelectObject(hDC, bottomRightPen);
        MoveToEx(hDC, cr.right - 3, cr.top + 2, NULL);
        LineTo(hDC, cr.right - 3, cr.bottom - 3);
        LineTo(hDC, cr.right - 2 - sbWidth - 1, cr.bottom - 3);

        SelectObject(hDC, hOldPen);
        HANDLES(ReleaseDC(HWindow, hDC));
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

BOOL CEditWindow::HideCaret()
{
    if (EditLine->HWindow == NULL)
        return FALSE;
    if (GetFocus() != EditLine->HWindow)
        return FALSE;
    return ::HideCaret(EditLine->HWindow);
}

void CEditWindow::ShowCaret()
{
    if (EditLine->HWindow != NULL)
        ::ShowCaret(EditLine->HWindow);
}

void CEditWindow::Enable(BOOL enable)
{
    if (Enabled != enable)
    {
        Enabled = enable;
        EnableWindow(HWindow, Enabled);
    }
}

void CEditWindow::StoreContent()
{
    if (HWindow == NULL)
        return;
    ResetStoredContent();
    int textLen = GetWindowTextLength(EditLine->HWindow);
    if (textLen > 0)
    {
        LastText = (char*)malloc(textLen + 1);
        if (LastText != NULL)
        {
            GetWindowText(EditLine->HWindow, LastText, textLen + 1);
            SendMessage(EditLine->HWindow, EM_GETSEL, (WPARAM)&LastSelStart, (LPARAM)&LastSelEnd);
        }
    }
}

void CEditWindow::RestoreContent()
{
    if (Enabled && LastText != NULL)
    {
        // if we have saved the old state of the window (content and selection), we restore it
        SetWindowText(HWindow, LastText);
        SendMessage(EditLine->HWindow, EM_SETSEL, (WPARAM)LastSelStart, (LPARAM)LastSelEnd);
    }
}

void CEditWindow::ResetStoredContent()
{
    if (LastText != NULL)
    {
        free(LastText);
        LastText = NULL;
    }
}
