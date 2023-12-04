// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <commctrl.h>
#include <limits.h>

#include "lstrfix.h"
#include "trace.h"
#include "messages.h"
#include "handles.h"
#include "array.h"
#include "str.h"
#include "winlib.h"
#include "sheets.h"
#include "tablist.h"
#include "tserver.h"
#include "dialog.h"
#include "registry.h"
#include "config.h"

#include "tserver.rh"
#include "tserver.rh2"

// id male ikony na task bare
#define ICON_ID 1

#define FLUSH_MESSAGES_CACHE_TIMER_ID 1
#define FLUSH_MESSAGES_CACHE_TIMER_TO 100

#define RESET_DATA_ACCEPT_EVENT_TIMER_ID 2
#define RESET_DATA_ACCEPT_EVENT_TIMER_TO 1000

// pokud je TRUE, po WM_CLOSE se zavre program
BOOL QuitProgram = FALSE;

//****************************************************************************
//
// CMainWindow
//

CMainWindow::CMainWindow()
{
    TabList = NULL;
    HasHotKey = FALSE;
    HasHotKeyClear = FALSE;
    TaskbarRestartMsg = 0;
}

void CMainWindow::FlushMessagesCache(BOOL& ErrorMessage)
{
    ErrorMessage = FALSE;
    Data.MessagesCache.BlockArray();
    int newCount = Data.MessagesCache.GetCount();
    int firstNew = Data.Messages.Count;

    // pokud je treba, vycistim prvnich x polozek z pole DataMessages
    BOOL needRepaint = FALSE;
    if (UseMaxMessagesCount)
    {
        if (firstNew + newCount > MaxMessagesCount)
        {
            int needDelete = firstNew + newCount - MaxMessagesCount;
            if (needDelete > firstNew)
                needDelete = firstNew;
            for (int i = 0; i < needDelete; i++)
                free(Data.Messages[i].File);
            if (needDelete < firstNew)
            {
                memmove(&Data.Messages[0], &Data.Messages[needDelete],
                        (firstNew - needDelete) * sizeof(CGlobalDataMessage));
            }
            Data.Messages.Count = firstNew - needDelete;
            firstNew -= needDelete;
            needRepaint = TRUE;
        }
    }

    int i;
    for (i = 0; i < newCount; i++)
    {
        Data.MessagesCache[i].Index = CGlobalDataMessage::StaticIndex++;
        Data.Messages.Add(Data.MessagesCache[i]);
        if (IsErrorMsg(Data.MessagesCache[i].Type))
            ErrorMessage = TRUE;
    }
    Data.MessagesCache.DestroyMembers(); // nevola destruktory
    Data.MessagesCache.UnBlockArray();

    Data.MessagesFlushInProgress = FALSE;
    SetEvent(MessagesFlushDoneEvent);

    // zatridim nove message do pole
    if (newCount > 0)
    {
        if (firstNew == 0 && newCount > 1)
            firstNew = 1;
        if (firstNew != 0)
        {
            while (firstNew < Data.Messages.Count)
            {
                i = firstNew - 1;
                while (i >= 0 && Data.Messages[firstNew] < Data.Messages[i])
                    i--;
                if (++i < firstNew)
                {
                    CGlobalDataMessage tmp = Data.Messages[firstNew];
                    memmove(&Data.Messages[i + 1], &Data.Messages[i],
                            (firstNew - i) * sizeof(CGlobalDataMessage));
                    Data.Messages[i] = tmp;
                }
                firstNew++;
            }
        }
    }
    if (newCount > 0)
        TabList->SetCount(Data.Messages.Count);
}

void CMainWindow::Activate()
{
    if (!IconControlEnable)
    {
        MessageBeep(MB_ICONEXCLAMATION);
        return;
    }
    if (!ConfigData.UseToolbarCaption)
    {
        // pokud neni hlavni okno viditelne, zobrazim ho
        if (IsIconic(HWindow))
        {
            ShowWindow(HWindow, SW_RESTORE);
            SetForegroundWindow(HWindow);
        }
        else
        {
            if (GetActiveWindow() == HWindow)
            {
                // pokud je akno aktivni, zhasnu ho
                ShowWindow(HWindow, SW_MINIMIZE);
            }
            else
            {
                // jinak ho zaktivuju
                SetForegroundWindow(HWindow);
            }
        }
    }
    else
    {
        // pokud neni hlavni okno viditelne, zobrazim ho
        if (!IsWindowVisible(HWindow))
        {
            ShowWindow(HWindow, SW_SHOW);
            SetForegroundWindow(HWindow);
        }
        else
        {
            if (GetActiveWindow() == HWindow)
            {
                // pokud je akno aktivni, zhasnu ho
                ShowWindow(HWindow, SW_HIDE);
            }
            else
            {
                // jinak ho zaktivuju
                SetForegroundWindow(HWindow);
            }
        }
    }
}

void CMainWindow::OnErrorMessage()
{
    // pokud jsme restored, nebudeme prudit
    if (!IsWindowVisible(HWindow) || IsIconic(HWindow))
    {
        MessageBeep(0);
        if (IsIconic(HWindow))
            ShowWindow(HWindow, SW_RESTORE);
        ShowWindow(HWindow, SW_SHOW);
        SetForegroundWindow(HWindow);
    }
}

void CMainWindow::ClearAllMessages()
{
    Data.MessagesCache.BlockArray();
    for (int i = 0; i < Data.MessagesCache.GetCount(); i++)
    {
        if (Data.MessagesCache[i].File != NULL)
            free(Data.MessagesCache[i].File);
    }
    Data.MessagesCache.DestroyMembers();
    Data.MessagesCache.UnBlockArray();
    for (int i = 0; i < Data.Messages.Count; i++)
    {
        if (Data.Messages[i].File != NULL)
            free(Data.Messages[i].File);
    }
    Data.Messages.DestroyMembers(); // nevola destruktory
    TabList->SetCount(0);
}

void CMainWindow::ShowMessageDetails()
{
    int index = TabList->GetSelectedIndex();
    if (index != -1)
    {
        // docasne potlacime topmost flag, aby se dialog dal hodit pod MSVC
        if (ConfigData.AlwaysOnTop)
            SetWindowPos(MainWindow->HWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        CDetailsDialog dialog(HWindow, &Data.Messages[index]);
        dialog.Execute();

        if (ConfigData.AlwaysOnTop)
            SetWindowPos(MainWindow->HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void AddChars(WCHAR* buffer, int count, WCHAR chr = L' ')
{
    WCHAR* p;
    p = buffer + wcslen(buffer);
    for (int i = 0; i < count; i++)
    {
        *p = chr;
        p++;
    }
    *p = 0;
}

void CMainWindow::ExportAllMessages()
{
    WCHAR fileName[MAX_PATH];
    wcscpy_s(fileName, L"tserver.log");

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = HWindow;
    WCHAR filter[MAX_PATH];
    wcscpy_s(filter, L"Log file (*.log)\0*.log\0");
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = fileName;
    ofn.nMaxFileTitle = MAX_PATH;
    ofn.lpstrDefExt = L"log";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER;

    if (GetSaveFileName(&ofn))
    {
        WCHAR fullName[MAX_PATH];
        if (GetFullPathName(fileName, MAX_PATH, fullName, NULL) != 0)
        {
            HANDLE file;
            WCHAR line[10000];
            WCHAR separator[10000];
            DWORD written;

            file = HANDLES_Q(CreateFile(fullName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_ARCHIVE, NULL));
            if (file == INVALID_HANDLE_VALUE)
                return;

            // napocitam maximalni velikosti sloupcu

            const WCHAR* strPID = L"PID";
            const WCHAR* strUPID = L"UPID";
            const WCHAR* strPName = L"PName";
            const WCHAR* strTID = L"TID";
            const WCHAR* strUTID = L"UTID";
            const WCHAR* strTName = L"TName";
            const WCHAR* strDate = L"Date";
            const WCHAR* strTime = L"Time";
            const WCHAR* strCounter = L"Counter [ms]";
            const WCHAR* strModule = L"Module";
            const WCHAR* strLine = L"Line";
            const WCHAR* strMessage = L"Message";

            DWORD maxPID = wcslen(strPID);
            DWORD maxUPID = wcslen(strUPID);
            DWORD maxPName = wcslen(strPName);
            DWORD maxTID = wcslen(strTID);
            DWORD maxUTID = wcslen(strUTID);
            DWORD maxTName = wcslen(strTName);
            DWORD maxDate = wcslen(strDate);
            DWORD maxTime = wcslen(strTime);
            DWORD maxCounter = wcslen(strCounter);
            DWORD maxModule = wcslen(strModule);
            DWORD maxLine = wcslen(strLine);
            DWORD maxMessage = wcslen(strMessage);

            for (int i = 0; i < Data.Messages.Count; i++)
            {
                WCHAR buff[1000];

                //PID
                swprintf_s(buff, L"%d", Data.Messages[i].ProcessID);
                if (wcslen(buff) > maxPID)
                    maxPID = wcslen(buff);

                //UPID
                swprintf_s(buff, L"%d", Data.Messages[i].UniqueProcessID);
                if (wcslen(buff) > maxUPID)
                    maxUPID = wcslen(buff);

                //PName
                Data.GetProcessName(Data.Messages[i].UniqueProcessID, buff, 1000);
                if (wcslen(buff) > maxPName)
                    maxPName = wcslen(buff);

                //TID
                swprintf_s(buff, L"%d", Data.Messages[i].ThreadID);
                if (wcslen(buff) > maxTID)
                    maxTID = wcslen(buff);

                //UTID
                swprintf_s(buff, L"%d", Data.Messages[i].UniqueThreadID);
                if (wcslen(buff) > maxUTID)
                    maxUTID = wcslen(buff);

                //TName
                Data.GetThreadName(Data.Messages[i].UniqueProcessID,
                                   Data.Messages[i].UniqueThreadID,
                                   buff, 1000);
                if (wcslen(buff) > maxTName)
                    maxTName = wcslen(buff);

                //Date
                GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE,
                              &Data.Messages[i].Time, NULL, buff,
                              1000);
                if (wcslen(buff) > maxDate)
                    maxDate = wcslen(buff);

                //Time
                GetTimeFormat(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT,
                              &Data.Messages[i].Time,
                              L"hh':'mm':'ss", buff, 1000);
                SWPrintFToEnd_s(buff, L".%03d", Data.Messages[i].Time.wMilliseconds);
                if (wcslen(buff) > maxTime)
                    maxTime = wcslen(buff);

                //Counter
                swprintf_s(buff, L"%.3lf", Data.Messages[i].Counter);
                if (wcslen(buff) > maxCounter)
                    maxCounter = wcslen(buff);

                //Module
                swprintf_s(buff, L"%s", Data.Messages[i].File);
                if (wcslen(buff) > maxModule)
                    maxModule = wcslen(buff);

                //Line
                swprintf_s(buff, L"%d", Data.Messages[i].Line);
                if (wcslen(buff) > maxLine)
                    maxLine = wcslen(buff);

                //Message
                swprintf_s(buff, L"%s", Data.Messages[i].Message);
                if (wcslen(buff) > maxMessage)
                    maxMessage = wcslen(buff);
            }

            // vytisknu hlavicku
            swprintf_s(line, L"\xFEFF" /* BOM */ L"Trace Server Log File\r\n\r\n");
            WriteFile(file, line, sizeof(WCHAR) * wcslen(line), &written, NULL);

            // oddelovac

            wcscpy_s(separator, L"-+");

            //PID
            AddChars(separator, maxPID, L'-');
            wcscat_s(separator, L"+");

            //UPID
            AddChars(separator, maxUPID, L'-');
            wcscat_s(separator, L"+");

            //PName
            AddChars(separator, maxPName, L'-');
            wcscat_s(separator, L"+");

            //TID
            AddChars(separator, maxTID, L'-');
            wcscat_s(separator, L"+");

            //UTID
            AddChars(separator, maxUTID, L'-');
            wcscat_s(separator, L"+");

            //TName
            AddChars(separator, maxTName, L'-');
            wcscat_s(separator, L"+");

            //Date
            AddChars(separator, maxDate, L'-');
            wcscat_s(separator, L"+");

            //Time
            AddChars(separator, maxTime, L'-');
            wcscat_s(separator, L"+");

            //Counter
            AddChars(separator, maxCounter, L'-');
            wcscat_s(separator, L"+");

            //Module
            AddChars(separator, maxModule, L'-');
            wcscat_s(separator, L"+");

            //Line
            AddChars(separator, maxLine, L'-');
            wcscat_s(separator, L"+");

            //Message
            AddChars(separator, maxMessage, L'-');

            wcscat_s(separator, L"\r\n");

            WriteFile(file, separator, sizeof(WCHAR) * wcslen(separator), &written, NULL);

            wcscpy_s(line, L" |");

            //PID
            AddChars(line, maxPID - wcslen(strPID));
            SWPrintFToEnd_s(line, L"%s|", strPID);

            //UPID
            AddChars(line, maxUPID - wcslen(strUPID));
            SWPrintFToEnd_s(line, L"%s|", strUPID);

            //PName
            SWPrintFToEnd_s(line, L"%s", strPName);
            AddChars(line, maxPName - wcslen(strPName));
            wcscat_s(line, L"|");

            //TID
            AddChars(line, maxTID - wcslen(strTID));
            SWPrintFToEnd_s(line, L"%s|", strTID);

            //UTID
            AddChars(line, maxUTID - wcslen(strUTID));
            SWPrintFToEnd_s(line, L"%s|", strUTID);

            //TName
            SWPrintFToEnd_s(line, L"%s", strTName);
            AddChars(line, maxTName - wcslen(strTName));
            wcscat_s(line, L"|");

            //Date
            AddChars(line, maxDate - wcslen(strDate));
            SWPrintFToEnd_s(line, L"%s|", strDate);

            //Time
            AddChars(line, maxTime - wcslen(strTime));
            SWPrintFToEnd_s(line, L"%s|", strTime);

            //Counter
            AddChars(line, maxCounter - wcslen(strCounter));
            SWPrintFToEnd_s(line, L"%s|", strCounter);

            //Module
            SWPrintFToEnd_s(line, L"%s", strModule);
            AddChars(line, maxModule - wcslen(strModule));
            wcscat_s(line, L"|");

            //Line
            AddChars(line, maxLine - wcslen(strLine));
            SWPrintFToEnd_s(line, L"%s|", strLine);

            //Message
            SWPrintFToEnd_s(line, L"%s", strMessage);
            AddChars(line, maxMessage - wcslen(strMessage));

            SWPrintFToEnd_s(line, L"\r\n");

            WriteFile(file, line, sizeof(WCHAR) * wcslen(line), &written, NULL);

            WriteFile(file, separator, sizeof(WCHAR) * wcslen(separator), &written, NULL);

            // nahazim tam radky
            for (int i = 0; i < Data.Messages.Count; i++)
            {
                WCHAR buff[1000];

                wcscpy_s(line, IsErrorMsg(Data.Messages[i].Type) ? L"E|" : L"I|");

                //PID
                swprintf_s(buff, L"%d", Data.Messages[i].ProcessID);
                AddChars(line, maxPID - wcslen(buff));
                SWPrintFToEnd_s(line, L"%s|", buff);

                //PID
                swprintf_s(buff, L"%d", Data.Messages[i].UniqueProcessID);
                AddChars(line, maxUPID - wcslen(buff));
                SWPrintFToEnd_s(line, L"%s|", buff);

                //PName
                Data.GetProcessName(Data.Messages[i].UniqueProcessID, buff, 1000);
                wcscat_s(line, buff);
                AddChars(line, maxPName - wcslen(buff));
                wcscat_s(line, L"|");

                //TID
                swprintf_s(buff, L"%d", Data.Messages[i].ThreadID);
                AddChars(line, maxTID - wcslen(buff));
                SWPrintFToEnd_s(line, L"%s|", buff);

                //UTID
                swprintf_s(buff, L"%d", Data.Messages[i].UniqueThreadID);
                AddChars(line, maxUTID - wcslen(buff));
                SWPrintFToEnd_s(line, L"%s|", buff);

                //TName
                Data.GetThreadName(Data.Messages[i].UniqueProcessID,
                                   Data.Messages[i].UniqueThreadID,
                                   buff, 1000);
                wcscat_s(line, buff);
                AddChars(line, maxTName - wcslen(buff));
                wcscat_s(line, L"|");

                //Date
                GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE,
                              &Data.Messages[i].Time, NULL, buff,
                              1000);
                AddChars(line, maxDate - wcslen(buff));
                SWPrintFToEnd_s(line, L"%s|", buff);

                //Time
                GetTimeFormat(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT,
                              &Data.Messages[i].Time,
                              L"hh':'mm':'ss", buff, 1000);
                SWPrintFToEnd_s(buff, L".%03d", Data.Messages[i].Time.wMilliseconds);
                AddChars(line, maxTime - wcslen(buff));
                SWPrintFToEnd_s(line, L"%s|", buff);

                //Counter
                swprintf_s(buff, L"%.3lf", Data.Messages[i].Counter);
                wcscat_s(line, buff);
                AddChars(line, maxCounter - wcslen(buff));
                wcscat_s(line, L"|");

                //Module
                swprintf_s(buff, L"%s", Data.Messages[i].File);
                wcscat_s(line, buff);
                AddChars(line, maxModule - wcslen(buff));
                wcscat_s(line, L"|");

                //Line
                swprintf_s(buff, L"%d", Data.Messages[i].Line);
                AddChars(line, maxLine - wcslen(buff));
                SWPrintFToEnd_s(line, L"%s|", buff);

                //Message
                swprintf_s(buff, L"%s", Data.Messages[i].Message);
                SWPrintFToEnd_s(line, L"%s", buff);

                SWPrintFToEnd_s(line, L"\r\n");
                WriteFile(file, line, sizeof(WCHAR) * wcslen(line), &written, NULL);
            }
            WriteFile(file, separator, sizeof(WCHAR) * wcslen(separator), &written, NULL);
            HANDLES(CloseHandle(file));
        }
    }
}

void CMainWindow::GetWindowPos()
{
    ConfigData.MainWindowPlacement.length = sizeof(ConfigData.MainWindowPlacement);
    GetWindowPlacement(HWindow, &ConfigData.MainWindowPlacement);
    ConfigData.MainWindowHidden = FALSE;
    if (ConfigData.UseToolbarCaption && !IsWindowVisible(HWindow))
        ConfigData.MainWindowHidden = TRUE;
}

LRESULT
CMainWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case CM_EXIT:
        {
            QuitProgram = TRUE;
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            return 0;
        }

        case CM_EXPORT:
        {
            ExportAllMessages();
            return 0;
        }

        case CM_CLEAR:
        {
            ClearAllMessages();
            return 0;
        }

        case CM_ABOUT:
        {
            CAboutDialog dialog(HInstance, IDD_ABOUT, HWindow);
            dialog.Execute();
            return 0;
        }

        case CM_DETAILS:
        {
            ShowMessageDetails();
            return 0;
        }

        case CM_DIFFTIME:
        {
            TabList->SwitchDeltaMode();
            return 0;
        }

        case CM_SHOWINMSVC:
        {
            int index = TabList->GetSelectedIndex();
            if (index != -1)
                Data.GotoEditor(index);
            return 0;
        }

        case CM_CONFIGURATION:
        {
            DoSetupDialog(HWindow);
            return 0;
        }
        }
        break;
    }

    case WM_INITMENUPOPUP:
    {
        CheckMenuItem((HMENU)wParam, CM_DIFFTIME, MF_BYCOMMAND | (TabList->GetDeltaMode() ? MF_CHECKED : MF_UNCHECKED));
        break;
    }

    case WM_HOTKEY:
    {
        if ((int)wParam == HOT_KEY_ID)
            Activate();
        if ((int)wParam == HOT_KEYCLEAR_ID)
            ClearAllMessages();
        break;
    }

    case WM_ACTIVATEAPP:
    {
        if ((BOOL)wParam)
        {
            if (TabList != NULL)
            {
                PostMessage(HWindow, WM_USER_ACTIVATE_APP, 0, 0);
            }
        }
        break;
    }

    case WM_ACTIVATE:
    {
        if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
        {
            PostMessage(HWindow, WM_USER_ACTIVATE_APP, 0, 0);
            return 0;
        }
        break;
    }

    case WM_SETFOCUS:
    {
        if (TabList != NULL && TabList->HWindow != NULL)
        {
            PostMessage(HWindow, WM_USER_ACTIVATE_APP, 0, 0);
            return 0;
        }
        break;
    }

    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_CREATE:
    {
        RECT r;
        GetClientRect(HWindow, &r);
        TabList = new CTabList();
        if (TabList != NULL)
        {
            if (TabList->Create(WC_TABLIST,
                                L"",
                                WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN,
                                0,
                                0,
                                r.right,
                                r.bottom,
                                HWindow,
                                NULL,
                                HInstance,
                                TabList))
            {
                // nahodim timer pro nacitani cache
                SetTimer(HWindow, FLUSH_MESSAGES_CACHE_TIMER_ID, FLUSH_MESSAGES_CACHE_TIMER_TO, NULL);

                if (ConfigData.HotKey != 0)
                    HasHotKey = RegisterHotKey(HWindow, HOT_KEY_ID, HIBYTE(ConfigData.HotKey), LOBYTE(ConfigData.HotKey));
                if (ConfigData.HotKeyClear != 0)
                    HasHotKeyClear = RegisterHotKey(HWindow, HOT_KEYCLEAR_ID, HIBYTE(ConfigData.HotKeyClear), LOBYTE(ConfigData.HotKeyClear));
            }
            else
            {
                TRACE_EW(L"Error while creating TabListu.");
            }
        }
        else
        {
            TRACE_EW(L"Out of memory.");
        }

        // nahodim timer pro reseteni ConnectDataAcceptedEvent, ktery stari klienti nutne musi mit
        // pred pripojenim resetly, jinak connect zkolabuje (od verze 7 si klienti event predem reseti a
        // tohle neni potreba); jak nastava: pokud se klient nedocka odpovedi serveru, ukonci connect
        // timeoutem a v tom pripade az server akci dokonci a pripravi odpoved klientovi, nahodi
        // ConnectDataAcceptedEvent, a event uz nema co resetnout, takze zustava nahozeny pro dalsi
        // connect (ktery tim zkolabuje, pokud nejde o klienta verze 7 a vyssi)
        SetTimer(HWindow, RESET_DATA_ACCEPT_EVENT_TIMER_ID, RESET_DATA_ACCEPT_EVENT_TIMER_TO, NULL);

        TaskbarRestartMsg = RegisterWindowMessage(TEXT("TaskbarCreated"));

        break;
    }

    case WM_QUERYENDSESSION:
    {
        GetWindowPos();
        Registry.Save();
        return TRUE;
    }

    case WM_CLOSE:
    {
        if (QuitProgram)
        {
            GetWindowPos();
            DestroyWindow(HWindow);
        }

        if (!ConfigData.UseToolbarCaption)
        {
            if (!IsIconic(HWindow))
                ShowWindow(HWindow, SW_MINIMIZE);
        }
        else
        {
            if (IsWindowVisible(HWindow))
                ShowWindow(HWindow, SW_HIDE);
        }
        return 0;
    }

    case WM_DESTROY:
    {
        if (HasHotKey)
            UnregisterHotKey(HWindow, HOT_KEY_ID);
        if (HasHotKeyClear)
            UnregisterHotKey(HWindow, HOT_KEYCLEAR_ID);
        KillTimer(HWindow, FLUSH_MESSAGES_CACHE_TIMER_ID);
        KillTimer(HWindow, RESET_DATA_ACCEPT_EVENT_TIMER_ID);
        TaskBarRemoveIcon();
        PostQuitMessage(0);
        return 0;
    }

    case WM_MOVE:
    {
        GetWindowPos();
        break;
    }

    case WM_SIZE:
    {
        // ulozim polohu hlavniho okna do konfigurace
        GetWindowPos();

        // umistim TabList
        if (TabList != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            SetWindowPos(TabList->HWindow, NULL, 0, 0, r.right, r.bottom, SWP_NOMOVE);
        }
        break;
    }

    case WM_TIMER:
    {
        if (wParam == FLUSH_MESSAGES_CACHE_TIMER_ID)
        {
            BOOL errorMessage;
            FlushMessagesCache(errorMessage);
            if (errorMessage && ConfigData.ShowOnErrorMessage)
                OnErrorMessage();
        }
        else
        {
            if (wParam == RESET_DATA_ACCEPT_EVENT_TIMER_ID && ConnectDataAcceptedEventMayBeSignaled)
            {
                if (WaitForSingleObject(OpenConnectionMutex, 0) == WAIT_OBJECT_0) // at nikdo neceka na ConnectDataAcceptedEvent
                {
                    ConnectDataAcceptedEventMayBeSignaled = FALSE; // uz bude resetly, i kdyby ho predchozi klient neresetnul
                    ResetEvent(ConnectDataAcceptedEvent);
                    ReleaseMutex(OpenConnectionMutex); // opet zpristupnim connect klientum
                }
            }
        }
        return 0;
    }

    case WM_USER_ACTIVATE_APP:
    {
        SetForegroundWindow(TabList->GetListViewHWND());
        SetFocus(TabList->GetListViewHWND());
        return 0;
    }

    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_ESCAPE:
        {
            Activate();
            break;
        }
        }
        break;
    }

    case WM_USER_PROCESS_CONNECTED:
    {
        if (ConfigData.AutoClear)
            ClearAllMessages();
        return 0;
    }

    case WM_USER_CT_OPENCONNECTION:
    {
        ReleaseMutex(OpenConnectionMutex); // zacina fungovat connection thread
        return 0;
    }

    case WM_USER_CT_TERMINATED:
    {
        WaitForSingleObject(ConnectingThread, 5000); // pockame nez skonci

        DWORD exitCode;
        if (GetExitCodeThread(ConnectingThread, &exitCode))
        {
            switch (exitCode)
            {
            case CT_SUCCESS:
                break;

            case CT_UNABLE_TO_CREATE_FILE_MAPPING:
            {
                MESSAGE_EW(NULL, L"Unable to create file mapping in connecting thread.", MB_OK);
                break;
            }

            case CT_UNABLE_TO_MAP_VIEW_OF_FILE:
            {
                MESSAGE_EW(NULL, L"Unable to map view of file in connecting thread.", MB_OK);
                break;
            }

            default: // STILL_ACTIVE a jine ...
            {
                MESSAGE_EW(NULL, L"Unexpected exit code of connecting thread.", MB_OK);
                break;
            }
            }
        }
        else
            MESSAGE_EW(NULL, L"Unable to get exit code of terminated connecting thread.", MB_OK);
        return 0;
    }

    case WM_USER_SHOWERROR:
    {
        switch (wParam)
        {
        case EC_CANNOT_CREATE_READ_PIPE_THREAD:
        {
            MESSAGE_EW(NULL, L"Unable to create read pipe thread, connection failed.",
                       MB_OK);
            break;
        }

        case EC_LOW_MEMORY:
        {
            MESSAGE_EW(NULL, L"Low memory.", MB_OK);
            QuitProgram = TRUE;
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            break;
        }

        case EC_UNKNOWN_MESSAGE_TYPE:
        {
            MESSAGE_EW(NULL, L"Unknown message type.\nConnection failed.", MB_OK);
            break;
        }

        default:
        {
            MESSAGE_EW(NULL, L"Unexpected error code.", MB_OK);
            break;
        }
        }
        return 0;
    }

    case WM_USER_PROCESSES_CHANGE:
    {
        if (TabList != NULL)
            TabList->RedrawListView();
        return 0;
    }

    case WM_USER_THREADS_CHANGE:
    {
        if (TabList != NULL)
            TabList->RedrawListView();
        return 0;
    }

    case WM_USER_PROCESS_DISCONNECTED:
    {
        TRACE_I("Odpojil se client - PID: " << wParam);
        return 0;
    }

    case WM_USER_INCORRECT_VERSION:
    {
        MESSAGE_EW(NULL, L"Incorrect version of client, connection refused." << L"\nClient PID: " << lParam << L"\nClient version: " << wParam << L"\nServer version: " << TRACE_SERVER_VERSION, MB_OK);
        return 0;
    }

    case WM_USER_SHOWSYSTEMERROR:
    {
        MESSAGE_EW(NULL, L"Error during reading client->server pipe:\n"
                             << errW(wParam),
                   MB_OK);
        return 0;
    }

    case WM_USER_FLUSH_MESSAGES_CACHE:
    {
        BOOL errorMessage;
        FlushMessagesCache(errorMessage);
        if (errorMessage && ConfigData.ShowOnErrorMessage)
            OnErrorMessage();
        return 0;
    }

    case WM_USER_ICON_NOTIFY:
    {
        switch (lParam)
        {
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        {
            Activate();
            break;
        }

        case WM_RBUTTONDOWN:
        {
            if (!IconControlEnable)
            {
                MessageBeep(MB_ICONEXCLAMATION);
                break;
            }
            HMENU hMenu = LoadMenu(HInstance, MAKEINTRESOURCE(IDM_ICON_POPUP));
            HMENU hPopupMenu = GetSubMenu(hMenu, 0);
            POINT p;
            GetCursorPos(&p);

            SetForegroundWindow(HWindow);
            TrackPopupMenu(hPopupMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON, p.x,
                           p.y, 0, HWindow, NULL);
            PostMessage(HWindow, WM_USER, 0, 0);
            DestroyMenu(hMenu);
            break;
        }
        }
        return 0;
    }

    default:
    {
        if (uMsg == TaskbarRestartMsg)
            TaskBarAddIcon();
        break;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

BOOL CMainWindow::TaskBarAddIcon()
{
    BOOL res;
    NOTIFYICONDATA tnid;

    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = HWindow;
    tnid.uID = ICON_ID;
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tnid.uCallbackMessage = WM_USER_ICON_NOTIFY;
    tnid.hIcon = HANDLES(LoadIcon(HInstance, MAKEINTRESOURCE(IC_TSERVER_1)));

    lstrcpyn(tnid.szTip, MAINWINDOW_NAME, sizeof(tnid.szTip));

    res = Shell_NotifyIcon(NIM_ADD, &tnid);

    if (tnid.hIcon != NULL)
        HANDLES(DestroyIcon(tnid.hIcon));

    return res;
}

BOOL CMainWindow::TaskBarRemoveIcon()
{
    NOTIFYICONDATA tnid;

    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = HWindow;
    tnid.uID = ICON_ID;

    return Shell_NotifyIcon(NIM_DELETE, &tnid);
}
