// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CDialogEx
//

INT_PTR
CDialogEx::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE_NONE
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        DialogStack.Push(HWindow);
        SG->MultiMonCenterWindow(HWindow, CenterToHWnd, FALSE);
        break;
    }

    case WM_DESTROY:
    {
        DialogStack.Pop();
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CDialogEx::NotifDlgJustCreated()
{
    SalGUI->ArrangeHorizontalLines(HWindow);
}

// ****************************************************************************

void HistoryComboBox(CTransferInfo& ti, int id, char* text, int textMax,
                     int historySize, char** history)
{
    CALL_STACK_MESSAGE4("HistoryComboBox(, %d, , %d, %d, )", id, textMax,
                        historySize);
    HWND combo;

    if (!ti.GetControl(combo, id))
        return;

    if (ti.Type == ttDataFromWindow)
    {
        ti.EditLine(id, text, textMax);

        int toMove = historySize - 1;

        // podivame jestli uz stejna polozka neni v historii
        int i;
        for (i = 0; i < historySize; i++)
        {
            if (history[i] == NULL)
                break;
            if (SG->StrICmp(history[i], text) == 0)
            {
                toMove = i;
                break;
            }
        }
        // alokujeme si pamet pro novou polozku
        char* ptr = new char[strlen(text) + 1];
        if (ptr)
        {
            // uvolnime pamet vymazavane polozky
            if (history[toMove])
                delete[] history[toMove];
            // vytvorime misto pro cestu kterou budeme ukladat
            for (i = toMove; i > 0; i--)
                history[i] = history[i - 1];
            // ulozime cestu
            strcpy(ptr, text);
            history[0] = ptr;
        }
    }
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < historySize; i++)
    {
        if (history[i] == NULL)
            break;
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)history[i]);
    }
    if (ti.Type == ttDataFromWindow)
        SendMessage(combo, CB_SETCURSEL, 0, 0);
    else
    {
        SendMessage(combo, CB_LIMITTEXT, textMax - 1, 0);
        SendMessage(combo, WM_SETTEXT, 0, (LPARAM)text);
        SendMessage(combo, CB_SETEDITSEL, 0, -1);
    }
}

void LoadHistory(HKEY regKey, const char* keyPattern, char** history,
                 char* buffer, int bufferSize, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE3("LoadHistory(, %s, , , %d, )", keyPattern, bufferSize);
    char buf[32];
    for (int i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        SalPrintf(buf, 32, keyPattern, i);
        if (!registry->GetValue(regKey, buf, REG_SZ, buffer, bufferSize))
            break;
        char* ptr = new char[strlen(buffer) + 1];
        if (!ptr)
            break;
        strcpy(ptr, buffer);
        history[i] = ptr;
    }
}

void SaveHistory(HKEY regKey, const char* keyPattern, char** history,
                 CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE2("SaveHistory(, %s, , )", keyPattern);
    char buf[32];
    for (int i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        if (history[i] == NULL)
            break;
        SalPrintf(buf, 32, keyPattern, i);
        registry->SetValue(regKey, buf, REG_SZ, history[i], -1);
    }
}
