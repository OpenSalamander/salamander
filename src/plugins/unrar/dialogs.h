// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CDlgRoot
{
public:
    HWND Parent;
    HWND Dlg;

    CDlgRoot(HWND parent)
    {
        Parent = parent;
        Dlg = NULL;
    }

    void CenterDlgToParent();
    void SubClassStatic(DWORD wID, BOOL subclass);
};

class CNextVolumeDialog : public CDlgRoot
{
    LPTSTR VolumeName;
    TCHAR CurrentPath[MAX_PATH];
    LPCSTR Message;

public:
    CNextVolumeDialog(HWND parent, LPTSTR volumeName, LPCTSTR message = NULL) : CDlgRoot(parent)
    {
        VolumeName = volumeName;
        lstrcpyn(CurrentPath, VolumeName, MAX_PATH);
        SalamanderGeneral->CutDirectory(CurrentPath);
        Message = message;
    }
    INT_PTR Proceed();

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL OnInit(WPARAM wParam, LPARAM lParam);
    BOOL OnBrowse(WORD wNotifyCode, WORD wID, HWND hwndCtl);
    BOOL OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl);
};

INT_PTR NextVolumeDialog(HWND parent, LPTSTR volumeName, LPCTSTR message = NULL);

class CContinuedFileDialog : public CDlgRoot
{
    const char* File;

public:
    CContinuedFileDialog(HWND parent, const char* file) : CDlgRoot(parent)
    {
        File = file;
    }
    INT_PTR Proceed();

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL OnInit(WPARAM wParam, LPARAM lParam);
    BOOL OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl);
};

INT_PTR ContinuedFileDialog(HWND parent, const char* file);

class CConfigDialog : public CDlgRoot
{

public:
    CConfigDialog(HWND parent) : CDlgRoot(parent) { ; }

    INT_PTR Proceed();

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL OnInit(WPARAM wParam, LPARAM lParam);
    BOOL OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl);
};

INT_PTR ConfigDialog(HWND parent);

#define PD_NOSKIP 0x01
#define PD_NOSKIPALL 0x02
#define PD_NOALL 0x04

class CPasswordDialog : public CDlgRoot
{
    char* Password;
    const char* FileName;
    DWORD Flags;

public:
    CPasswordDialog(HWND parent, const char* fileName, char* password, DWORD flags) : CDlgRoot(parent)
    {
        Password = password;
        FileName = fileName;
        Flags = flags;
    }

    INT_PTR Proceed();

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL OnInit(WPARAM wParam, LPARAM lParam);
};

INT_PTR PasswordDialog(HWND parent, const char* fileName, char* password, DWORD flags);

class CAttentionDialog : public CDlgRoot
{
public:
    CAttentionDialog(HWND parent) : CDlgRoot(parent) { ; }

    INT_PTR Proceed();

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl);
};

INT_PTR AttentionDialog(HWND parent);
