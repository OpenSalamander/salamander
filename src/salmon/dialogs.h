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
    int UploadingIndex; // index do pole BugReports, ktery prave uploadime
    CCompressParams CompressParams;
    CUploadParams UploadParams;
    CMinidumpParams MinidumpParams;
    char CurrentProgressText[200];
    BOOL MinidumpOnOpen; // ma se po otevreni okna zacit generovat minidump?

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
