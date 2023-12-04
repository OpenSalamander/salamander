// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CSnapshotProgressDlg : public CDialog
{
protected:
    CGUIProgressBarAbstract* ProgressBar;
    BOOL WantCancel; // TRUE when user wants Cancel

public:
    CSnapshotProgressDlg(HWND parent, CObjectOrigin origin = ooStandard);
    void SetProgressText(int resID);
    void SetProgressText(int resID, int number);
    void SetProgress(DWORD progress);
    BOOL GetWantCancel();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CCopyProgressDlg : public CDialog
{
protected:
    CGUIProgressBarAbstract *ProgressBar1, *ProgressBar2;
    CGUIStaticTextAbstract *Label1, *Label2;
    BOOL WantCancel;
    char SrcName[MAX_PATH], DestName[MAX_PATH];
    DWORD LastTick, FileProgress, TotalProgress;
    BOOL Changed[4];

public:
    CCopyProgressDlg(HWND parent, CObjectOrigin origin = ooStandard);
    void SetSourceFileName(const char* fileName);
    void SetDestFileName(const char* fileName);
    void SetFileProgress(DWORD progress);
    void SetTotalProgress(DWORD progress);
    BOOL GetWantCancel();
    void UpdateControls(BOOL now = FALSE);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CConnectDialog : public CDialog
{
public:
    CConnectDialog(HWND parent, int panel);
    char Volume[MAX_PATH];
    int Panel;

protected:
    HWND hList;
    HIMAGELIST hDrivesImg;

    void AddVolumeDetails(const char* root, const char* volumeID, const char* volumeFS,
                          const CQuadWord& bytesTotal, const CQuadWord& bytesFree,
                          const char* volumeName, int serial, BOOL selected);
    void InitDrives();
    BOOL OnDialogOK();
    void OnImageBrowse();

public:
    virtual void Transfer(CTransferInfo& ti);
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CFileNameDialog : public CDialog
{
public:
    CFileNameDialog(HWND parent, char* filename);
    BOOL AllPressed;

protected:
    char* FileName;

    virtual void Transfer(CTransferInfo& ti);
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CConfigDialog : public CDialog
{
public:
    CConfigDialog(HWND parent);

protected:
    virtual void Transfer(CTransferInfo& ti);
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CRestoreDialog : public CDialog
{
public:
    CRestoreDialog(HWND parent);

    char TargetPath[MAX_PATH];

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CRestoreProgressDlg : public CCopyProgressDlg
{
public:
    CRestoreProgressDlg(HWND parent, CObjectOrigin origin = ooStandard)
        : CCopyProgressDlg(parent, origin) {}

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
