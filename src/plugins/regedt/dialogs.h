// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MNTS_ALL (MNTS_B | MNTS_I | MNTS_A)

#define MAX_HISTORY_ENTRIES 20

// pro okno FTP Searche
// informuje o tom, ze jeden vyhledavaci thread zkoncil
#define WM_USER_SEARCH_FINISHED (WM_APP + 2)

// informuje hlavni okno, ze ma refreshnout list view
#define WM_USER_ADDFILE (WM_APP + 3)

#define WM_USER_BUTTONS (WM_APP + 4)

extern CSalamanderGeneralAbstract* SG;
extern HINSTANCE DLLInstance; //dll instance handle

extern BOOL MinBeepWhenDone;

extern LPWSTR CopyOrMoveHistory[MAX_HISTORY_ENTRIES];

extern int DialogWidth;
extern int DialogHeight;
extern BOOL Maximized;
extern int TypeDateColFW; // LO/HI-WORD: levy/pravy panel: sloupec Type/Date: FixedWidth
extern int TypeDateColW;  // LO/HI-WORD: levy/pravy panel: sloupec Type/Date: Width
extern int DataTimeColFW; // LO/HI-WORD: levy/pravy panel: sloupec Data/Time: FixedWidth
extern int DataTimeColW;  // LO/HI-WORD: levy/pravy panel: sloupec Data/Time: Width
extern int SizeColFW;     // LO/HI-WORD: levy/pravy panel: sloupec Size: FixedWidth
extern int SizeColW;      // LO/HI-WORD: levy/pravy panel: sloupec Size: Width

extern HFONT EnvFont;     // font prostredi (edit, toolbar, header, status)
extern int EnvFontHeight; // vyska fontu

BOOL InitDialogs();
void ReleaseDialogs();

void DialogStackPush(HWND hWindow);
void DialogStackPop();
HWND DialogStackPeek();
inline HWND GetParent()
{
    HWND ret = DialogStackPeek();
    return ret == (HWND)-1 ? SG->GetMsgBoxParent() : ret;
}

class CDialogStackAutoObject
{
public:
    CDialogStackAutoObject(HWND hWindow) { DialogStackPush(hWindow); }
    ~CDialogStackAutoObject() { DialogStackPop(); }
};

#define PARENT(wnd) CDialogStackAutoObject dsao(wnd);

class CTransferInfoEx;

void HistoryComboBox(CTransferInfoEx& ti, int id, LPWSTR text, int textMax,
                     int historySize, LPWSTR* history);

class CTransferInfoEx : public CTransferInfo
{
public:
    CTransferInfoEx(HWND hDialog, CTransferType type) : CTransferInfo(hDialog, type) { ; }

    void EditLineW(int ctrlID, LPWSTR buffer, DWORD bufferSize, BOOL select = TRUE);
};

class CDialogEx : public CDialog
{
protected:
    HWND CenterToHWnd;

public:
    CDialogEx(int resID, HWND parent, HWND centerToHWnd, CObjectOrigin origin = ooStandard) : CDialog(HLanguage, resID, parent, origin = ooStandard) { CenterToHWnd = centerToHWnd; }

    CDialogEx(int resID, int helpID, HWND parent, HWND centerToHWnd, CObjectOrigin origin = ooStandard) : CDialog(HLanguage, resID, helpID, parent, origin = ooStandard) { CenterToHWnd = centerToHWnd; }

    virtual BOOL ValidateData();
    virtual void Validate(CTransferInfoEx& /*ti*/) {}
    virtual BOOL TransferData(CTransferType type);
    virtual void Transfer(CTransferInfoEx& /*ti*/) {}

    INT_PTR Execute(); // modalni dialog
    HWND Create();     // nemodalni dialog

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

class CNewKeyDialog : public CDialogEx
{
protected:
    WCHAR* KeyName;
    WCHAR* Text;
    WCHAR* Title;
    BOOL* Direct;

public:
    CNewKeyDialog(HWND parent, WCHAR* keyName, BOOL* direct = NULL, WCHAR* text = NULL, WCHAR* title = NULL,
                  int resID = IDD_CREATEKEY)
        : CDialogEx(resID, parent, SG->GetMainWindowHWND())
    {
        KeyName = keyName;
        Text = text;
        Title = title;
        Direct = direct;
    }

    virtual void Validate(CTransferInfoEx& ti);
    virtual void Transfer(CTransferInfoEx& ti);
};

enum CValDlgType
{
    vdtNewValDialog,
    vdtEditValDialog
};

class CNewValDialog : public CNewKeyDialog
{
protected:
    CValDlgType DlgType;
    DWORD* Type;

public:
    CNewValDialog(HWND parent, WCHAR* keyName, DWORD* type, BOOL* direct, int resID = IDD_NEWVALUE)
        : CNewKeyDialog(parent, keyName, direct, NULL, NULL, resID)
    {
        DlgType = vdtNewValDialog;
        Type = type;
    }

    virtual void Validate(CTransferInfoEx& ti);
    virtual void Transfer(CTransferInfoEx& ti);
};

class CEditValDialog : public CNewValDialog
{
protected:
    LPBYTE& Data;
    DWORD& Size;
    BOOL DefaultValue;
    DWORD Allocated;
    BOOL Hex;
    int EditWidth;
    int EditHeight;

public:
    CEditValDialog(HWND parent, WCHAR* keyName, DWORD* type, LPBYTE& data,
                   DWORD& size, BOOL defaultValue)
        : CNewValDialog(parent, keyName, type, NULL, IDD_EDITVAL), Data(data), Size(size)
    {
        DefaultValue = defaultValue;
        Allocated = Size;
        DlgType = vdtEditValDialog;
        Hex = TRUE;
    }

    virtual void Validate(CTransferInfoEx& ti);
    virtual void Transfer(CTransferInfoEx& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CRawEditValDialog : public CNewValDialog
{
protected:
    LPBYTE& Data;
    DWORD& Size;
    BOOL DefaultValue;
    DWORD Allocated;
    char TempFile[MAX_PATH];
    char TempDir[MAX_PATH];
    BOOL Edit;

public:
    CRawEditValDialog(HWND parent, WCHAR* keyName, DWORD* type, LPBYTE& data,
                      DWORD& size, BOOL defaultValue)
        : CNewValDialog(parent, keyName, type, NULL, IDD_RAWEDITVAL), Data(data), Size(size)
    {
        DefaultValue = defaultValue;
        Allocated = Size;
        *TempDir = *TempFile = 0;
        Edit = FALSE;
    }

    BOOL ExportToTempFile();
    BOOL ImportFromTempFile();

    virtual void Transfer(CTransferInfoEx& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CCopyOrMoveDialog : public CNewKeyDialog
{
protected:
public:
    CCopyOrMoveDialog(HWND parent, WCHAR* keyName, BOOL* direct, WCHAR* text, WCHAR* title)
        : CNewKeyDialog(parent, keyName, direct, text, title, IDD_COPY)
    {
        ;
    }

    virtual void Transfer(CTransferInfoEx& ti);
};

class CConfigDialog : public CDialogEx
{
protected:
public:
    CConfigDialog(HWND parent) : CDialogEx(IDD_CONFIG, parent, parent)
    {
        ;
    }

    virtual void Validate(CTransferInfoEx& ti);
    virtual void Transfer(CTransferInfoEx& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CExportDialog : public CDialogEx
{
protected:
    LPWSTR Path;
    char* File;
    BOOL* Direct;

public:
    CExportDialog(HWND parent, LPWSTR path, char* file, BOOL* direct)
        : CDialogEx(IDD_EXPORT, parent, SG->GetMainWindowHWND())
    {
        Path = path;
        File = file;
        Direct = direct;
    }

    virtual void Validate(CTransferInfoEx& ti);
    virtual void Transfer(CTransferInfoEx& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
