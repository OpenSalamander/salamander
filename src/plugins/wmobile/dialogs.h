// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CCommonDialog
//
// Dialog centrovany k parentu
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
    BOOL WantCancel; // TRUE pokud uzivatel chce Cancel

    // protoze dialog nebezi ve vlastnim threadu, je zbytecne pouzivat WM_TIMER
    // metodu; stejna nas musi zavolat pro vypumpovani message queue
    DWORD LastTickCount; // pro detekci ze uz je treba prekreslit zmenena data

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

    // vyprazdni message queue (volat dostatecne casto) a umozni prekreslit, stisknout Cancel...
    // vraci TRUE, pokud uzivatel chce prerusit operaci
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
    // handly pro TimeDate controly
    HWND HModifiedDate;
    HWND HModifiedTime;
    HWND HCreatedDate;
    HWND HCreatedTime;
    HWND HAccessedDate;
    HWND HAccessedTime;

    // stavove promenne pro disableni checkboxu
    BOOL SelectionContainsDirectory;

    // pokud user kliknul na prislusny checkbox, bude promenna nastavena na TRUE
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
