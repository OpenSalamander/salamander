// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern HIMAGELIST HSymbolsImageList;
extern char DirText[100];

// indexy ikon v image listu
#define ILS_DIRECTORY 0
#define ILS_FILE 1
#define ILS_WARNING 2

// indexy sloupcu
#define CI_OLDNAME 0
#define CI_NEWNAME 1
#define CI_SIZE 2
#define CI_DATE 3
#define CI_TIME 4
#define CI_PATH 5

class CRenamerDialog;

class CPreviewWindow : public CWindow
{
protected:
    CRenamerDialog* RenamerDialog;
    CRenamerOptions& RenamerOptions; // options
    CRenamer Renamer;

    // data for rename operation
    char (&Root)[MAX_PATH];
    int& RootLen;
    TIndirectArray<CSourceFile>& SourceFiles;
    BOOL& SourceFilesValid;

    BOOL TransferError;
    BOOL Dirty;
    char TextBuffer[MAX_PATH];
    int CachedItem;
    char NewNameCache[MAX_PATH];
    BOOL NewNameValid;

    HWND Static;
    int State;

public:
    CPreviewWindow(CRenamerDialog* renamerDlg);
    virtual ~CPreviewWindow();
    BOOL InitColumns();

    void SetDirty() { Dirty = TRUE; }
    void Update(BOOL force = FALSE);
    void GetDispInfo(LV_DISPINFO* info);
    char* GetItemText(int index, int subItem);
    BOOL CustomDraw(LPNMLVCUSTOMDRAW cd, LRESULT& result);

    int CompareFunc(CSourceFile* f1, CSourceFile* f2, int sortBy);
    void QuickSort(int left, int right, int sortBy);
    void SortItems(int sortBy);
    int GetSortCommand(int column);
    void SetItemCount(int count, DWORD flags, int state);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
