// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

enum CDialogTaskEnum
{
    dteCompress,
    dteUpload,
    dteMinidump,
    dteDialog
};

class CMainDialog : public CDialog
{
protected:
    HFONT HBoldFont;
    BOOL Compressing;
    BOOL Uploading;
    BOOL Minidumping;
    int UploadingIndex; // index into the BugReports array we are currently uploading
    CCompressParams CompressParams;
    CUploadParams UploadParams;
    CMinidumpParams MinidumpParams;
    char CurrentProgressText[200];
    BOOL MinidumpOnOpen; // should minidump generation start after opening the window?

public:
    CMainDialog(HINSTANCE modul, int resID, BOOL minidumpOnOpen);
    ~CMainDialog();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void ShowChilds(CDialogTaskEnum task, BOOL enable);
    void CenterControl(int resID);

    BOOL StartUploadIndex(int index);
};
