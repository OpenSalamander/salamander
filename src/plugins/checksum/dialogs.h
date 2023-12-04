// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CCRCMD5Thread;
class CCalculateThread;
class CVerifyThread;

#define WM_USER_STARTWORK WM_APP + 555 // start work when dialog is visible (instead of in WM_INITDIALOG)
#define WM_USER_ENDWORK WM_APP + 556   // end work when worker thread ends

class FILELISTITEM
{
public:
    FILELISTITEM()
    {
        IconIndex = 0;
        Name = NULL;
        Size = CQuadWord(0, 0);
        FileExist = FALSE;
        for (int i = 0; i < HT_COUNT; i++)
            Hashes[i] = NULL;
    }

    ~FILELISTITEM()
    {
        if (Name != NULL)
            free(Name);
        for (int i = 0; i < HT_COUNT; i++)
        {
            if (Hashes[i] != NULL)
                free(Hashes[i]);
        }
    }

public:
    int IconIndex;  // synchronizace neni potreba, viz CSFVMD5Dialog::SetItemTextAndIcon()
    char* Name;     // po pridani do pole uz se nemeni = pristup k ni nesynchronizujeme
    CQuadWord Size; // po pridani do pole uz se nemeni = pristup k ni nesynchronizujeme
    BOOL FileExist; // po pridani do pole uz se nemeni = pristup k ni nesynchronizujeme

    // poradi v tomto poli odpovida sloupcum v listview (zavisle na konfiguraci)
    // synchronizace neni potreba, viz CSFVMD5Dialog::SetItemTextAndIcon()
    char* Hashes[HT_COUNT];
};

class CSFVMD5Dialog : public CDialog
{
public:
    CSFVMD5Dialog(int id, HWND parent, BOOL alwaysOnTop);
    ~CSFVMD5Dialog();

protected:
    void InitList(int columns[], int widths[], int numcols, SHashInfo* pHashInfo = NULL);
    void InitLayout(int id[], int n, int m);
    void RecalcLayout(int cx, int cy, int id[], int n, int m);
    void SaveSizes(int sizes[], int numcols, SHashInfo* pHashInfo = NULL);
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void ConvertPath(char* str, char from, char to);
    virtual void OnThreadEnd();
    void SetItemTextAndIcon(int row, int col, const char* text = NULL, int icon = -1);
    void GetItemText(int row, int col, char* text, int textMax);
    void IncreaseProgress(const CQuadWord& delta);
    virtual void DeleteItem(int index);
    void ScrollToItem(int i);
    void AddFileListItem(const char* name, CQuadWord size, BOOL fileExist);
    void SetRowsDirty(int firstRow, int lastRow);

    void EnterDataCS() { HANDLES(EnterCriticalSection(&DataCS)); }
    void LeaveDataCS() { HANDLES(LeaveCriticalSection(&DataCS)); }

    HWND hParent;
    HWND hList;
    HIMAGELIST hImg;
    int listX, listY;
    int ctrlX[7], ctrlY[7];
    int minX, minY;

    CRITICAL_SECTION DataCS;  // kriticka sekce pro pristup k sdilenym datum dialogu a vypocetniho threadu
    int ScheduledScrollIndex; // synchronizujeme pres DataCS
    int ScrollIndex;
    int DirtyRowMin;
    int DirtyRowMax;
    CQuadWord totalSize;
    CQuadWord CurrentSize;
    CQuadWord ScheduledCurrentSize; // synchronizujeme pres DataCS
    BOOL bScrollToItem;
    BOOL bAlwaysOnTop;
    HANDLE hThread;  // handle lze pouzit jen v thread-queue (zavira se automaticky po dokonceni threadu)
    DWORD iThreadID; // TID threadu 'hThread'
    BOOL bThreadRunning;
    BOOL bTerminateThread;
    BOOL ReadingDirectories; // probiha enumerace adresaru
    BOOL StopReadingDirectories;
    BOOL bDisableNotification;
    TIndirectArray<FILELISTITEM> FileList;

    friend class CCalculateThread;
    friend class CVerifyThread;
    friend class CSFVMD5ListView;
};

class CSFVMD5ListView : public CWindow
{
public:
    CSFVMD5ListView(CSFVMD5Dialog* parent) : CWindow(ooAllocated) { Parent = parent; }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_VSCROLL)
            Parent->bScrollToItem = FALSE; // uzivatel rucne roluje obsahem -> zakazeme autoscroll
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }

    CSFVMD5Dialog* Parent;
};

class CCRCMD5Thread : public CThread
{
public:
    CCRCMD5Thread(BOOL* terminate) : CThread("Hash Worker Thread")
    {
        Terminate = terminate;
    };
    virtual unsigned Body() = 0;
    BOOL* Terminate; // samotny BOOL lezi v dialogu, aby sel z dialogu nahodit (objekt threadu se automaticky dealokuje)
};

typedef struct _SEEDFILEINFO
{
    CQuadWord Size;
    bool bDir;
    DWORD Attr;
    char Name[1];
} SEEDFILEINFO;

typedef TIndirectArray<SEEDFILEINFO> TSeedFileList;

class CCalculateDialog : public CSFVMD5Dialog
{
public:
    CCalculateDialog(HWND parent, BOOL alwaysOnTop, TSeedFileList* pFileList, const char* sourcePath);

protected:
    BOOL AddDir(char (&path)[MAX_PATH + 50], size_t root, BOOL* ignoreAll);
    BOOL GetFileList();
    virtual void OnThreadEnd();
    void EnableButtons(BOOL bEnable);
    virtual void DeleteItem(int index);
    BOOL GetSaveFileName(LPTSTR buffer, LPCTSTR title = NULL);
    void SaveHashes();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnContextMenu(int x, int y, eHASH_TYPE forceCopyHash = HT_COUNT);
    void RefreshUI();

    TSeedFileList* pSeedFileList;
    const char* SourcePath;
    SHashInfo HashInfo[HT_COUNT];
    DWORD RefreshCounter;

    friend class CCalculateThread;
};

#define DIGEST_MAX_SIZE 64
#define HASH_MAX_SIZE (2 * DIGEST_MAX_SIZE + 1)

struct FILEINFO
{
    // nic z teto struktury se po pridani do pole uz se nemeni = pristup k datum nesynchronizujeme
    char fileName[MAX_PATH];
    char digest[DIGEST_MAX_SIZE];
    CQuadWord size;
    BOOL bFileExist;
};

class CVerifyDialog : public CSFVMD5Dialog
{
public:
    CVerifyDialog(HWND parent, BOOL alwaysOnTop, char* path, char* file);

protected:
    void LTrimStr(char* str);
    //char* GetLine(FILE* f, char* buffer, int max);
    char* LoadFile(char* name);
    BOOL AnalyzeSourceFile();
    BOOL LoadSourceFile();
    virtual void OnThreadEnd();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    TIndirectArray<FILEINFO> fileList;
    char* sourcePath;
    char* sourceFile;
    SHashInfo* pHashInfo;
    BOOL bCanceled;
    int nCorrupt, nMissing, nSkipped;

    friend class CVerifyThread;
};

BOOL OpenCalculateDialog(HWND parent);
BOOL OpenVerifyDialog(HWND parent);

extern CWindowQueue ModelessQueue; // seznam vsech nemodalnich oken
extern CThreadQueue ThreadQueue;   // seznam vsech threadu oken a vypoctu

//****************************************************************************
//
// CCommonDialog
//
// Dialog centered to parent
//

class CCommonDialog : public CDialog
{
public:
    CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin = ooStandard);
    CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent, CObjectOrigin origin = ooStandard);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CConfigurationDialog
//

class CConfigurationDialog : public CCommonDialog
{
private:
public:
    CConfigurationDialog(HWND hParent);

    virtual void Transfer(CTransferInfo& ti);

    /*  protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);*/
};
