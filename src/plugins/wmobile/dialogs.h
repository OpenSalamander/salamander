// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CCommonDialog
//
// Dialog centered relative to the parent
//

class CCommonDialog : public CDialog
{
public:
    CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin = ooStandard);
    CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent, CObjectOrigin origin = ooStandard);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

// ****************************************************************************
// CProgressDlg
//

class CProgressDlg : public CCommonDialog
{
protected:
    CGUIProgressBarAbstract* ProgressBar;
    BOOL WantCancel; // TRUE if the user wants to cancel

    // because the dialog does not run in its own thread, there is no point in using the WM_TIMER
    // method; the same code has to be called to pump the message queue
    DWORD LastTickCount; // detects when it is time to repaint changed data

    char TextCache[MAX_PATH];
    BOOL TextCacheIsDirty;
    DWORD ProgressCache;
    BOOL ProgressCacheIsDirty;
    DWORD ProgressTotalCache;
    BOOL ProgressTotalCacheIsDirty;

    char Title[100], Operation[100];

public:
    CProgressDlg(HWND parent, const char* title, const char* operation, CObjectOrigin origin = ooStandard, int resID = 0);

    void Set(const char* fileName, DWORD progressTotal, BOOL dalayedPaint);
    void SetProgress(DWORD progressTotal, DWORD progress, BOOL dalayedPaint);

    // empties the message queue (call often enough) and allows repainting, pressing Cancel...
    // returns TRUE if the user wants to interrupt the operation
    BOOL GetWantCancel();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableCancel(BOOL enable);

    virtual void FlushDataToControls();
};

class CProgress2Dlg : public CProgressDlg
{
protected:
    CGUIProgressBarAbstract* ProgressBar2;

    char TextCache2[MAX_PATH];
    BOOL TextCache2IsDirty;

    char Operation2[100];

public:
    CProgress2Dlg(HWND parent, const char* title, const char* operation, const char* operation2, CObjectOrigin origin = ooStandard, int resID = 0);

    void Set(const char* fileName, const char* fileName2, BOOL dalayedPaint);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void FlushDataToControls();
};

//****************************************************************************
//
// CChangeAttrDialog
//

class CChangeAttrDialog : public CCommonDialog
{
private:
    // handles for the TimeDate controls
    HWND HModifiedDate;
    HWND HModifiedTime;
    HWND HCreatedDate;
    HWND HCreatedTime;
    HWND HAccessedDate;
    HWND HAccessedTime;

    // state variables used to disable the check boxes
    BOOL SelectionContainsDirectory;

    // if the user clicked the respective check box, the variable is set to TRUE
    BOOL ArchiveDirty;
    BOOL ReadOnlyDirty;
    BOOL HiddenDirty;
    BOOL SystemDirty;

public:
    int Archive,
        ReadOnly,
        Hidden,
        System,
        ChangeTimeModified,
        ChangeTimeCreated,
        ChangeTimeAccessed,
        RecurseSubDirs;

    SYSTEMTIME TimeModified;
    SYSTEMTIME TimeCreated;
    SYSTEMTIME TimeAccessed;

    CChangeAttrDialog(HWND parent, CObjectOrigin origin, DWORD attr, DWORD attrDiff,
                      BOOL selectedDirectory,
                      const SYSTEMTIME* timeModified,
                      const SYSTEMTIME* timeCreated,
                      const SYSTEMTIME* timeAccessed);

    virtual void Transfer(CTransferInfo& ti);

    void EnableWindows();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL GetAndValidateTime(CTransferInfo* ti, int resIDDate, int resIDTime, SYSTEMTIME* time);
};
