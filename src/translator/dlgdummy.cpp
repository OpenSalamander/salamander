// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

const wchar_t* DUMMY_TEXT = L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum "
                            L"Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum ";

const wchar_t* DUMMY_TEXT_SHORT = L"Lorem Ipsum Lorem Ipsum Lorem";

void SetWindowTextWithDummyValue(HWND hWnd, BOOL shortText)
{
    SetWindowTextW(hWnd, shortText ? DUMMY_TEXT_SHORT : DUMMY_TEXT);
}

void SetComboBoxWithDummyValue(HWND hWnd)
{
    for (int i = 0; i < 10; i++)
        SendMessageW(hWnd, CB_ADDSTRING, 0, (LPARAM)DUMMY_TEXT);
    SendMessageW(hWnd, CB_SETCURSEL, 0, 0);
}

void SetIconWithDummyValue(HWND hWnd)
{
    HICON hIcon = LoadIcon(HInstance, MAKEINTRESOURCE(IDI_DUMMY));
    SendMessageW(hWnd, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);
}

void FillControlWithDummyValue(HWND hWnd)
{
    // WORD id = (WORD)GetMenu(hWnd);
    DWORD wStyle = GetWindowLong(hWnd, GWL_STYLE);
    wchar_t wClass[200];
    RealGetWindowClassW(hWnd, wClass, 200);
    wchar_t wText[2000];
    wText[0] = 0;
    GetWindowTextW(hWnd, wText, 2000);
    BOOL isEmpty = (wText[0] == 0);
    if (lstrcmpW(wClass, L"Edit") == 0 && isEmpty)
        SetWindowTextWithDummyValue(hWnd, FALSE);
    if (lstrcmpW(wClass, L"Button") == 0 && IsRadioOrCheckBox(wStyle) && isEmpty)
        SetWindowTextWithDummyValue(hWnd, FALSE);
    if (lstrcmpW(wClass, L"ComboBox") == 0)
        SetComboBoxWithDummyValue(hWnd);
    if (lstrcmpW(wClass, L"Static") == 0)
    {
        if (IsStyleStaticText(wStyle, FALSE, FALSE) && isEmpty)
            SetWindowTextWithDummyValue(hWnd, FALSE);
        if ((wStyle & SS_TYPEMASK) == SS_ICON)
            SetIconWithDummyValue(hWnd);
    }
}

void FillControlsWithDummyValues(HWND hDlg)
{
    char buff[1000] = {0};
    GetWindowText(hDlg, buff, _countof(buff));
    if (buff[0] == 0)
        SetWindowTextWithDummyValue(hDlg, TRUE);
    HWND hIter = GetWindow(hDlg, GW_CHILD);
    do
    {
        FillControlWithDummyValue(hIter);
        hIter = GetNextWindow(hIter, GW_HWNDNEXT);
    } while (hIter != NULL);
}
