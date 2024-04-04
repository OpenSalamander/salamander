// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "viewer.h"
#include "codetbl.h"

#include "cfgdlg.h"
#include "dialogs.h"

// ****************************************************************************

struct CTVData
{
    int Left, Top, Width, Height;
    CViewerWindow* View;
    const char* Name;
    UINT ShowCmd;
    BOOL Success;
    const char* Caption;
    BOOL WholeCaption;
};

HANDLE ViewerContinue = NULL;

void ThreadViewerMessageLoopBodyAux()
{
    __try
    {
        OleUninitialize();
        //    CoUninitialize();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        OCUExceptionHasOccured++;
    }
}

unsigned ThreadViewerMessageLoopBody(void* parameter)
{
    CALL_STACK_MESSAGE1("ThreadViewerMessageLoopBody(): (text/hex viewer)");
    SetThreadNameInVCAndTrace("Viewer");
    TRACE_I("Begin");
    //  TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 1");
    CTVData* data = (CTVData*)parameter;
    CViewerWindow* view = data->View;
    char name[MAX_PATH];
    strcpy(name, data->Name);
    char captionBuf[MAX_PATH];
    const char* caption = NULL;
    BOOL wholeCaption = FALSE;
    if (data->Caption != NULL)
    {
        lstrcpyn(captionBuf, data->Caption, MAX_PATH);
        caption = captionBuf;
        wholeCaption = data->WholeCaption;
    }
    UINT showCmd = data->ShowCmd;

    //  TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 2");
    //  CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 2");
    data->Success = /*CoInitialize(NULL) == S_OK &&*/ OleInitialize(NULL) == S_OK;
    //  TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 3 succes="<<data->Success);
    //  CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 3");

    if (data->Success &&
        view->CreateEx(Configuration.AlwaysOnTop ? WS_EX_TOPMOST : 0,
                       CVIEWERWINDOW_CLASSNAME,
                       LoadStr(IDS_VIEWERTITLE),
                       WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
                       data->Left,
                       data->Top,
                       data->Width,
                       data->Height,
                       NULL,
                       ViewerMenu,
                       HInstance,
                       view) != NULL)
    {
        //    TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 4");
        //    CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 4");
        view->SetObjectOrigin(ooAllocated); // Change from ooStatic, window successfully created
        data->Success = TRUE;
        // we will show the window, otherwise it might "pop up" unexpectedly later
        //    TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 5");
        //    CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 5");
        ShowWindow(view->HWindow, showCmd);
        //    TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 6");
        //    CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 6");
        SetForegroundWindow(view->HWindow); // error from 1.6 beta 1
                                            //    TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 7");
                                            //    CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 7");
        UpdateWindow(view->HWindow);
        //    TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 8");
        //    CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 8");
    }
    else
        data->Success = FALSE;

    //  TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 9");
    //  CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 9");
    BOOL ok = data->Success;
    data = NULL;              // no longer valid
    SetEvent(ViewerContinue); // Let's start the main thread
                              //  TRACE_I("MoresStanislav: ThreadViewerMessageLoopBody 10");
                              //  CALL_STACK_MESSAGE1("MoresStanislav: ThreadViewerMessageLoopBody 10");

    if (ok) // if the window has been created, we start the application loop
    {
        CALL_STACK_MESSAGE1("ThreadViewerMessageLoopBody::message_loop");
        if (SalGetFullName(name))
            view->OpenFile(name, caption, wholeCaption);

        MSG msg;
        HWND viewHWindow = view->HWindow; // due to wm_quit, after which the window will no longer be allocated
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!TranslateAccelerator(viewHWindow, ViewerTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    ThreadViewerMessageLoopBodyAux();

    TRACE_I("End");
    return ok ? 0 : 1;
}

unsigned ThreadViewerMessageLoopEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ThreadViewerMessageLoopBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread ViewerMessageLoop: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ThreadViewerMessageLoop(void* param)
{
    CCallStack stack;
    return ThreadViewerMessageLoopEH(param);
}

BOOL OpenViewer(const char* name, CViewType mode, int left, int top, int width, int height,
                UINT showCmd, BOOL returnLock, HANDLE* lock, BOOL* lockOwner,
                CSalamanderPluginViewerData* viewerData, int enumFileNamesSourceUID,
                int enumFileNamesLastFileIndex)
{
    CALL_STACK_MESSAGE11("OpenViewer(%s, %d, %d, %d, %d, %d, %u, %d, , , , %d, %d)",
                         name, mode, left, top, width, height, showCmd, returnLock,
                         enumFileNamesSourceUID, enumFileNamesLastFileIndex);
    CSalamanderPluginInternalViewerData* intViewerData = NULL;
    if (viewerData != NULL && viewerData->Size == sizeof(CSalamanderPluginInternalViewerData))
    {
        intViewerData = (CSalamanderPluginInternalViewerData*)viewerData;
        mode = (intViewerData->Mode == 0) ? vtText : vtHex;
    }
    CViewerWindow* view = new CViewerWindow(NULL, mode, NULL, FALSE, ooStatic,
                                            enumFileNamesSourceUID, enumFileNamesLastFileIndex);
    if (view != NULL)
    {
        view->InitFindDialog(GlobalFindDialog);
        if (returnLock)
        {
            *lock = view->GetLockObject();
            *lockOwner = TRUE;
        }
    }
    if (view != NULL && view->IsGood() && (!returnLock || *lock != NULL))
    {
        CTVData data;
        data.View = view;
        data.Left = left;
        data.Top = top;
        data.Width = width;
        data.Height = height;
        data.Name = name;
        data.ShowCmd = showCmd;
        data.Caption = intViewerData != NULL ? intViewerData->Caption : NULL;

        DWORD ThreadID;
        HANDLE loop = HANDLES(CreateThread(NULL, 0, ThreadViewerMessageLoop, &data, 0, &ThreadID));
        if (loop == NULL)
        {
            TRACE_E("Unable to start ViewerMessageLoop thread.");
            goto ERROR_TV_CREATE;
        }
        else
        {
            //      SetThreadPriority(loop, THREAD_PRIORITY_HIGHEST);
        }
        AddAuxThread(loop);                            // add a thread between existing viewers (kill on exit)
        WaitForSingleObject(ViewerContinue, INFINITE); // Wait until the thread is started
        if (!data.Success)
            goto ERROR_TV_CREATE;
        return TRUE;
    }
    else
    {
        TRACE_E("Insufficient memory for viewer or unable to get font for viewer.");

    ERROR_TV_CREATE:

        if (view != NULL && returnLock && *lock != NULL)
            view->CloseLockObject();
        if (view != NULL)
            delete view;
        return FALSE;
    }
}

//*****************************************************************************
//
// RegExpErrorText
//
// error messages from regexp.cpp
//

const char* RegExpErrorText(CRegExpErrors err)
{
    switch (err)
    {
    case reeNoError:
        return LoadStr(IDS_REGEXPERROR1);
    case reeLowMemory:
        return LoadStr(IDS_REGEXPERROR2);
    case reeEmpty:
        return LoadStr(IDS_REGEXPERROR3);
    case reeTooBig:
        return LoadStr(IDS_REGEXPERROR4);
    case reeTooManyParenthesises:
        return LoadStr(IDS_REGEXPERROR5);
    case reeUnmatchedParenthesis:
        return LoadStr(IDS_REGEXPERROR6);
    case reeOperandCouldBeEmpty:
        return LoadStr(IDS_REGEXPERROR7);
    case reeNested:
        return LoadStr(IDS_REGEXPERROR8);
    case reeInvalidRange:
        return LoadStr(IDS_REGEXPERROR9);
    case reeUnmatchedBracket:
        return LoadStr(IDS_REGEXPERROR10);
    case reeFollowsNothing:
        return LoadStr(IDS_REGEXPERROR11);
    case reeTrailingBackslash:
        return LoadStr(IDS_REGEXPERROR12);
    case reeInternalDisaster:
        return LoadStr(IDS_REGEXPERROR13);
    default:
        return "";
    }
}

//
//*****************************************************************************
// CViewerWindow
//

void CViewerWindow::ConfigHasChanged()
{
    CALL_STACK_MESSAGE1("CViewerWindow::ConfigHasChanged()");
    BOOL fatalErr = FALSE;
    FileChanged(NULL, FALSE, fatalErr, FALSE); // restart viewer
    if (fatalErr)
        FatalFileErrorOccured();
    if (fatalErr || ExitTextMode)
        return;
    InvalidateRect(HWindow, NULL, FALSE);
}

__int64
CViewerWindow::Prepare(HANDLE* hFile, __int64 offset, __int64 bytes, BOOL& fatalErr)
{
    fatalErr = FALSE;
    if (Seek <= offset)
        if (Seek + Loaded >= offset + bytes)
            return bytes; // o.k.
        else
        {
            if (Seek + Loaded == FileSize) // read data until the end of the file
                if (Seek + Loaded > offset)
                    return Seek + Loaded - offset;
                else
                    return 0; // end of file

            if (offset + bytes - (Seek + Loaded) < VIEW_BUFFER_SIZE / 2 &&
                (Loaded <= VIEW_BUFFER_SIZE / 2 ||
                 Seek + VIEW_BUFFER_SIZE / 2 <= offset))
            {
                if (!LoadBehind(hFile))
                    fatalErr = TRUE;
            }
            else
            {
                Seek = offset;
                Loaded = 0;
                if (!LoadBehind(hFile))
                    fatalErr = TRUE;
            }
        }
    else // offset < Seek
    {
        if (Seek - offset < VIEW_BUFFER_SIZE / 2)
        {
            if (!LoadBefore(hFile))
                fatalErr = TRUE;
        }
        else
        {
            Seek = offset;
            Loaded = 0;
            if (!LoadBehind(hFile))
                fatalErr = TRUE;
        }
    }
    if (Seek <= offset)
        if (Seek + Loaded >= offset + bytes)
            return bytes; // o.k.
        else
            return Seek + Loaded > offset ? Seek + Loaded - offset : 0; // shortened
    else
        return 0; // nothing is usable (the beginning did not load)
}

void CViewerWindow::CodeCharacters(unsigned char* start, unsigned char* end)
{
    if (UseCodeTable)
    {
        unsigned char* s = start - 1;
        while (++s < end)
            *s = CodeTable[*s];
    }
}

BOOL CViewerWindow::LoadBefore(HANDLE* hFile)
{
    CALL_STACK_MESSAGE1("CViewerWindow::LoadBefore()");
    if (FileName == NULL)
        return FALSE;

    HANDLE file;
    if (hFile == NULL || *hFile == NULL)
    {
        file = HANDLES_Q(CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                    OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
        if (hFile != NULL && file != INVALID_HANDLE_VALUE)
            *hFile = file;
    }
    else
        file = *hFile;

    if (file != INVALID_HANDLE_VALUE)
    {
        CQuadWord size;
        DWORD err;
        BOOL haveSize = SalGetFileSize(file, size, err);
        if (!haveSize || size.Value != (unsigned __int64)FileSize) // error or file change
        {
            TRACE_I("The size of the viewed file has changed or some error occured.");
            // PostMessage(HWindow, WM_COMMAND, CM_REREADFILE, 0);  // prezitek, zbytecne: vznikne "fatal error" a dojde k prekresleni
            Seek = Loaded = 0;
            if (hFile == NULL) // if the caller does not take care of closing the trade, it is up to us
                HANDLES(CloseHandle(file));
            return FALSE;
        }
        int read;
        if (Seek >= VIEW_BUFFER_SIZE / 2)
            read = VIEW_BUFFER_SIZE / 2;
        else
            read = (int)Seek;
        if (read == 0)
        {
            TRACE_E("Incorrect call to LoadBefore.");
            if (hFile == NULL) // if the caller does not take care of closing the trade, it is up to us
                HANDLES(CloseHandle(file));
            return FALSE;
        }
        if (Loaded > 0)
        {
            int space = VIEW_BUFFER_SIZE - read;
            memmove(Buffer + read, Buffer, (int)((space < Loaded) ? (Loaded = space) : Loaded));
            Seek -= read;
        }
        DWORD readed;
        BOOL ret;
        BOOL kill = FALSE; // TRUE means that in case of an error, FileName will be set to NULL
        CQuadWord resSeek;
        resSeek.SetUI64(Seek); // Attention, the seek for SetFilePointer is a signed value
        resSeek.LoDWord = SetFilePointer(file, resSeek.LoDWord, (PLONG)&resSeek.HiDWord, FILE_BEGIN);
        err = GetLastError();

#define INVALID_SET_FILE_POINTER ((DWORD)-1) // MS seems to have forgotten about this constant ;-)

        if ((resSeek.LoDWord != INVALID_SET_FILE_POINTER || err == NO_ERROR) && // no error
            resSeek.Value == (unsigned __int64)Seek)                            // current file offset is set
        {
            if (ReadFile(file, Buffer, read, &readed, NULL))
            {
                if (readed != (DWORD)read)
                {
                    InvalidateRect(HWindow, NULL, FALSE);
                    Seek = Loaded = 0; // data in the buffer may be corrupted, we invalidate them so that nothing uses them during the display of the message box
                    kill = SalMessageBoxViewerPaintBlocked(HWindow, LoadStr(IDS_VIEWER_UNKNOWNERR),
                                                           LoadStr(IDS_ERRORREADINGFILE),
                                                           MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL;
                    ret = FALSE;
                    Seek = Loaded = 0; // During the display of the message box, some data could have been loaded, we invalidate the buffer again
                }
                else
                {
                    CodeCharacters(Buffer, Buffer + read);
                    Loaded += readed;
                    ret = TRUE;
                }
            }
            else
            {
                DWORD err2 = GetLastError();
                InvalidateRect(HWindow, NULL, FALSE);
                Seek = Loaded = 0; // data in the buffer may be corrupted, we invalidate them so that nothing uses them during the display of the message box
                kill = SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(err2), LoadStr(IDS_ERRORREADINGFILE),
                                                       MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL;
                ret = FALSE;
                Seek = Loaded = 0; // During the display of the message box, some data could have been loaded, we invalidate the buffer again
            }
        }
        else
        {
            InvalidateRect(HWindow, NULL, FALSE);
            Seek = Loaded = 0; // data in the buffer may be corrupted, we invalidate them so that nothing uses them during the display of the message box
            kill = SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(err), LoadStr(IDS_ERRORREADINGFILE),
                                                   MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL;
            ret = FALSE;
            Seek = Loaded = 0; // During the display of the message box, some data could have been loaded, we invalidate the buffer again
        }
        if (hFile == NULL) // if the caller does not take care of closing the trade, it is up to us
            HANDLES(CloseHandle(file));

        if (!ret && kill) // Alternatively, we can terminate working with this file
        {
            free(FileName);
            FileName = NULL;
            if (Caption != NULL)
            {
                free(Caption);
                Caption = NULL;
            }
            if (Lock != NULL)
            {
                SetEvent(Lock);
                Lock = NULL; // now it's just on the disk cache
            }
            SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
            InvalidateRect(HWindow, NULL, FALSE);
        }

        return ret;
    }
    else
    {
        DWORD err = GetLastError();
        Seek = Loaded = 0;
        free(FileName);
        FileName = NULL;
        if (Caption != NULL)
        {
            free(Caption);
            Caption = NULL;
        }
        if (Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL; // now it's just on the disk cache
        }
        SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
        InvalidateRect(HWindow, NULL, FALSE);
        SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(err), LoadStr(IDS_ERRORREADINGFILE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

BOOL CViewerWindow::LoadBehind(HANDLE* hFile)
{
    CALL_STACK_MESSAGE1("CViewerWindow::LoadBehind()");
    if (FileName == NULL)
        return FALSE;

    HANDLE file;
    if (hFile == NULL || *hFile == NULL)
    {
        file = HANDLES_Q(CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN, NULL));
        if (hFile != NULL && file != INVALID_HANDLE_VALUE)
            *hFile = file;
    }
    else
        file = *hFile;

    if (file != INVALID_HANDLE_VALUE)
    {
        CQuadWord size;
        DWORD err;
        BOOL haveSize = SalGetFileSize(file, size, err);
        if (!haveSize || size.Value != (unsigned __int64)FileSize) // error or file change
        {
            TRACE_I("The size of the viewed file has changed or some error occured.");
            // PostMessage(HWindow, WM_COMMAND, CM_REREADFILE, 0);  // prezitek, zbytecne: vznikne "fatal error" a dojde k prekresleni
            Seek = Loaded = 0;
            if (hFile == NULL) // if the caller does not take care of closing the trade, it is up to us
                HANDLES(CloseHandle(file));
            return FALSE;
        }
        int read;
        if (FileSize - (Seek + Loaded) >= VIEW_BUFFER_SIZE / 2)
            read = VIEW_BUFFER_SIZE / 2;
        else
            read = (int)(FileSize - (Seek + Loaded));
        if (read == 0)
        {
            if (hFile == NULL) // if the caller does not take care of closing the trade, it is up to us
                HANDLES(CloseHandle(file));
            return FALSE;
        }
        DWORD readed; // first offset into the buffer, then the number of bytes read
        __int64 seekEnd = Seek + Loaded;
        if (Loaded > 0)
        {
            int space = VIEW_BUFFER_SIZE - read;
            if (space < Loaded)
            {
                memmove(Buffer, Buffer + (Loaded - space), space);
                Loaded = (readed = space);
                Seek = seekEnd - Loaded;
            }
            else
                readed = (int)Loaded;
        }
        else
            readed = 0;
        BOOL ret;
        BOOL kill = FALSE; // TRUE means that in case of an error, FileName will be set to NULL

        CQuadWord resSeek;
        resSeek.SetUI64(seekEnd); // Attention, the seek for SetFilePointer is a signed value
        resSeek.LoDWord = SetFilePointer(file, resSeek.LoDWord, (PLONG)&resSeek.HiDWord, FILE_BEGIN);
        err = GetLastError();
        if ((resSeek.LoDWord != INVALID_SET_FILE_POINTER || err == NO_ERROR) && // no error
            resSeek.Value == (unsigned __int64)seekEnd)                         // current file offset is set
        {
            if (ReadFile(file, Buffer + readed, read, &readed, NULL))
            {
                if (readed != (DWORD)read)
                {
                    TRACE_I("CViewerWindow::LoadBehind(): ReadFile returned " << (DWORD)readed << " instead of " << (DWORD)read);
                    InvalidateRect(HWindow, NULL, FALSE);
                    Seek = Loaded = 0; // data in the buffer may be corrupted, we invalidate them so that nothing uses them during the display of the message box
                    kill = SalMessageBoxViewerPaintBlocked(HWindow, LoadStr(IDS_VIEWER_UNKNOWNERR), LoadStr(IDS_ERRORREADINGFILE),
                                                           MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL;
                    ret = FALSE;
                    Seek = Loaded = 0; // During the display of the message box, some data could have been loaded, we invalidate the buffer again
                }
                else
                {
                    CodeCharacters(Buffer + Loaded, Buffer + Loaded + read);
                    Loaded += readed;
                    ret = TRUE;
                }
            }
            else
            {
                DWORD err2 = GetLastError();
                InvalidateRect(HWindow, NULL, FALSE);
                Seek = Loaded = 0; // data in the buffer may be corrupted, we invalidate them so that nothing uses them during the display of the message box
                kill = SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(err2), LoadStr(IDS_ERRORREADINGFILE),
                                                       MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL;
                ret = FALSE;
                Seek = Loaded = 0; // During the display of the message box, some data could have been loaded, we invalidate the buffer again
            }
        }
        else
        {
            DWORD err2 = GetLastError();
            InvalidateRect(HWindow, NULL, FALSE);
            Seek = Loaded = 0; // data in the buffer may be corrupted, we invalidate them so that nothing uses them during the display of the message box
            kill = SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(err2), LoadStr(IDS_ERRORREADINGFILE),
                                                   MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL;
            ret = FALSE;
            Seek = Loaded = 0; // During the display of the message box, some data could have been loaded, we invalidate the buffer again
        }
        if (hFile == NULL) // if the caller does not take care of closing the trade, it is up to us
            HANDLES(CloseHandle(file));

        if (!ret && kill) // Alternatively, we can terminate working with this file
        {
            free(FileName);
            FileName = NULL;
            if (Caption != NULL)
            {
                free(Caption);
                Caption = NULL;
            }
            if (Lock != NULL)
            {
                SetEvent(Lock);
                Lock = NULL; // now it's just on the disk cache
            }
            SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
            InvalidateRect(HWindow, NULL, FALSE);
        }

        return ret;
    }
    else
    {
        DWORD err = GetLastError();
        Seek = Loaded = 0;
        free(FileName);
        FileName = NULL;
        if (Caption != NULL)
        {
            free(Caption);
            Caption = NULL;
        }
        if (Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL; // now it's just on the disk cache
        }
        SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
        InvalidateRect(HWindow, NULL, FALSE);
        SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(err), LoadStr(IDS_ERRORREADINGFILE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

void CViewerWindow::HeightChanged(BOOL& fatalErr)
{
    CALL_STACK_MESSAGE1("CViewerWindow::HeightChanged()");
    fatalErr = FALSE;
    switch (Type)
    {
    case vtHex:
    {
        MaxSeekY = max(0, max(0, FileSize - 1) / 16 + 1 - max(1, Height / CharHeight)) * 16;
        break;
    }

    case vtText:
    {
        MaxSeekY = FindSeekBefore(FileSize, max(Height / CharHeight, 1), fatalErr);
        break;
    }
    }
}

void CViewerWindow::OpenFile(const char* file, const char* caption, BOOL wholeCaption)
{
    CALL_STACK_MESSAGE3("CViewerWindow::OpenFile(%s, %s)", file, caption);
    char fileName[MAX_PATH];
    strcpy(fileName, file);

    if (Caption != NULL)
    {
        free(Caption);
        Caption = NULL;
    }
    if (caption != NULL)
    {
        Caption = DupStr(caption);
        WholeCaption = wholeCaption;
    }
    else
        WholeCaption = FALSE;
    if (FileName != NULL)
        free(FileName);
    FileName = (char*)malloc(strlen(fileName) + 1);
    if (FileName != NULL)
        strcpy(FileName, fileName);
    TooBigSelAction = 0;
    CanSwitchToHex = TRUE;
    CanSwitchQuietlyToHex = TRUE;
    OriginX = 0;
    SeekY = 0;
    ExitTextMode = FALSE;
    ForceTextMode = FALSE;
    CodeType = 0;
    UseCodeTable = FALSE;
    BOOL fatalErr = FALSE;
    FileChanged(NULL, FALSE, fatalErr, TRUE);
    if (fatalErr)
        FatalFileErrorOccured();
    if (fatalErr || ExitTextMode)
    {
        CanSwitchQuietlyToHex = FALSE;
        return;
    }
    if (FileName == NULL)
        SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
    else
        SetViewerCaption();
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
    CanSwitchQuietlyToHex = FALSE;
}

void CViewerWindow::ReleaseMouseDrag()
{
    if (MouseDrag)
    {
        ReleaseCapture();
        KillTimer(HWindow, IDT_AUTOSCROLL);
        MouseDrag = FALSE;
        EndSelectionPrefX = -1;
    }
}

int CViewerWindow::SalMessageBoxViewerPaintBlocked(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
    BOOL oldEnablePaint = EnablePaint;
    // when displaying a messagebox, it will lead to painting = reading files = more errors,
    // thus we will disable Paint = it will only erase the background of the viewer (e.g. previously displayed parts of the file)
    EnablePaint = FALSE;
    int res = SalMessageBox(hParent, lpText, lpCaption, uType);
    EnablePaint = oldEnablePaint;
    return res;
}

void CViewerWindow::FileChanged(HANDLE file, BOOL testOnlyFileSize, BOOL& fatalErr,
                                BOOL detectFileType, BOOL* calledHeightChanged)
{
    CALL_STACK_MESSAGE3("CViewerWindow::FileChanged(, %d, , %d,)", testOnlyFileSize, detectFileType);
    fatalErr = FALSE;
    if (calledHeightChanged != NULL)
        *calledHeightChanged = FALSE;
    if (FileName == NULL)
        return;

    char* s = strrchr(FileName, '\\');
    char* namePart = FileName;
    if (s != NULL)
    {
        namePart = s + 1;
        memcpy(CurrentDir, FileName, (s - FileName) + 1);
        CurrentDir[(s - FileName) + 1] = 0;
    }
    else
        CurrentDir[0] = 0;

    BOOL close;
    if (file == NULL)
    {
        file = HANDLES_Q(CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN, NULL));
        close = TRUE;
    }
    else
        close = FALSE;

    if (file != INVALID_HANDLE_VALUE)
    {
        __int64 oldFS = FileSize;
        CQuadWord size;
        DWORD err;
        BOOL haveSize = SalGetFileSize(file, size, err);
        FileSize = size.Value;
        if (!haveSize ||                               // Error when determining file size
            size >= CQuadWord(0xFFFFFFFF, 0x7FFFFFFF)) // too large file (> 8 EB)
        {
            Seek = 0;
            Loaded = 0;
            FileSize = 0;
            FindOffset = 0;
            StartSelection = EndSelection = -1;
            ReleaseMouseDrag();
            FirstLineSize = LastLineSize = ViewSize = 0;
            LastFindSeekY = -1;
            free(FileName);
            FileName = NULL;
            if (Caption != NULL)
            {
                free(Caption);
                Caption = NULL;
            }
            if (Lock != NULL)
            {
                SetEvent(Lock);
                Lock = NULL; // now it's just on the disk cache
            }
            SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
            InvalidateRect(HWindow, NULL, FALSE);
            SalMessageBoxViewerPaintBlocked(HWindow, err == NO_ERROR ? LoadStr(IDS_UNABLETOVIEWFILENT) : GetErrorText(err),
                                            LoadStr(IDS_ERRORREADINGFILE), MB_OK | MB_ICONEXCLAMATION);
            fatalErr = TRUE;
        }
        else
        {
            if (!testOnlyFileSize || FileSize != oldFS)
            {
                Seek = 0;
                Loaded = 0;
                FindOffset = 0;
                StartSelection = EndSelection = -1;
                ResetFindOffsetOnNextPaint = TRUE;
                ReleaseMouseDrag();
                FirstLineSize = LastLineSize = ViewSize = 0;
                LastFindSeekY = -1;

                if (detectFileType)
                {
                    int defViewMode = DefViewMode; // (0=Auto-Select)
                    if (defViewMode == 0)
                    {
                        // only if Auto-Select is set, exceptions will apply
                        if (Configuration.TextModeMasks.AgreeMasks(namePart, NULL))
                        {
                            defViewMode = 1;               // Text
                            CanSwitchQuietlyToHex = FALSE; // if we are forcing Text mode, we will ask for a switch to Hex
                        }
                        else if (Configuration.HexModeMasks.AgreeMasks(namePart, NULL))
                            defViewMode = 2; // Hex
                    }
                    else
                    {
                        if (defViewMode == 1)
                            CanSwitchQuietlyToHex = FALSE; // if we are forcing Text mode, we will ask for a switch to Hex
                    }

                    int len;
                    BOOL fatalErr2 = FALSE;
                    if (CodePageAutoSelect || defViewMode == 0)
                        len = (int)Prepare(NULL, 0, RECOGNIZE_FILE_TYPE_BUFFER_LEN, fatalErr2);
                    else
                        len = 0;
                    if (CodePageAutoSelect && fatalErr2)
                        fatalErr = TRUE;
                    else // We are ignoring the error in Prepare during autoselect ViewMode (defViewMode == 0) here, a bit strange, reason unknown ;-) Petr
                    {
                        // if auto-select is enabled and we have something to perform heuristics on,
                        // we will try to find a suitable conversion table
                        if (len > 0 && (defViewMode == 0 || CodePageAutoSelect))
                        {
                            BOOL isText;
                            char codePage[101];
                            char recBuf[RECOGNIZE_FILE_TYPE_BUFFER_LEN]; // for safety, we will make a copy of data from Buffer to recBuf
                            int recLen = min(len, RECOGNIZE_FILE_TYPE_BUFFER_LEN);
                            memcpy(recBuf, (char*)Buffer, recLen);
                            BOOL oldEnablePaint = EnablePaint;
                            // when displaying a messagebox, it will lead to painting = reading files = more errors,
                            // thus we will disable Paint = it will only erase the background of the viewer (e.g. previously displayed parts of the file)
                            EnablePaint = FALSE;
                            RecognizeFileType(HWindow, recBuf, recLen, FALSE, &isText, codePage);
                            EnablePaint = oldEnablePaint;
                            if (defViewMode == 0)
                            {
                                if (isText)
                                    Type = vtText;
                                else
                                    Type = vtHex;
                            }
                            if (CodePageAutoSelect)
                            {
                                if (isText && defViewMode != 2)
                                {
                                    int c = CodeTables.GetConversionToWinCodePage(codePage);
                                    if (CodeTables.Valid(c))
                                        SetCodeType(c);
                                    else // conversion "none"
                                    {
                                        CodeType = 0;
                                        UseCodeTable = FALSE;
                                    }
                                }
                            }
                        }
                        if (defViewMode == 1)
                            Type = vtText;
                        else if (defViewMode == 2)
                            Type = vtHex;
                        // if auto-select is not enabled, we will choose the default conversion
                        if (!CodePageAutoSelect)
                        {
                            int defCodeType;
                            if (!CodeTables.GetCodeType(DefaultConvert, defCodeType))
                                defCodeType = 0;
                            if (CodeTables.Valid(defCodeType))
                                SetCodeType(defCodeType);
                            else // conversion "none"
                            {
                                CodeType = 0;
                                UseCodeTable = FALSE;
                            }
                        }
                    }
                }

                if (!fatalErr)
                {
                    HeightChanged(fatalErr);
                    if (calledHeightChanged != NULL)
                        *calledHeightChanged = TRUE;
                    if (!fatalErr && !ExitTextMode)
                        FindNewSeekY(SeekY, fatalErr);
                }
            }
        }
        if (close)
            HANDLES(CloseHandle(file));
    }
    else
    {
        DWORD err = GetLastError();
        Seek = 0;
        Loaded = 0;
        FileSize = 0;
        FindOffset = 0;
        StartSelection = EndSelection = -1;
        ReleaseMouseDrag();
        FirstLineSize = LastLineSize = ViewSize = 0;
        LastFindSeekY = -1;
        free(FileName);
        FileName = NULL;
        if (Caption != NULL)
        {
            free(Caption);
            Caption = NULL;
        }
        if (Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL; // now it's just on the disk cache
        }
        SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE));
        InvalidateRect(HWindow, NULL, FALSE);
        if (IsWindowVisible(HWindow)) // Prevent message box when closing the viewer and wiping the viewed file
            SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(err), LoadStr(IDS_ERRORREADINGFILE), MB_OK | MB_ICONEXCLAMATION);
        fatalErr = TRUE;
    }
}

void CViewerWindow::FatalFileErrorOccured(DWORD repeatCmd)
{
    // We will try to set the internal state of the viewer so that no further error occurs before it is delivered
    // WM_USER_VIEWERREFRESH
    WaitForViewerRefresh = TRUE;
    LastSeekY = SeekY;
    LastOriginX = OriginX;
    RepeatCmdAfterRefresh = repeatCmd;

    Seek = 0;
    Loaded = 0;
    OriginX = 0;
    SeekY = 0;
    MaxSeekY = 0;
    FileSize = 0;
    ViewSize = 0;
    FirstLineSize = 0;
    LastLineSize = 0;
    StartSelection = -1;
    EndSelection = -1;
    ReleaseMouseDrag();
    FindOffset = 0;
    LastFindSeekY = -1;
    LastFindOffset = 0;
    ScrollToSelection = FALSE;
    LineOffset.DestroyMembers();
    EnableSetScroll = TRUE;
    PostMessage(HWindow, WM_USER_VIEWERREFRESH, 0, 0);
}

BOOL CViewerWindow::FindNextEOL(HANDLE* hFile, __int64 seek, __int64 maxSeek, __int64& lineEnd, __int64& nextLineBegin, BOOL& fatalErr)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE3("CViewerWindow::FindNextEOL(%g, %g, , ,)", (double)seek, (double)maxSeek);
    unsigned char *s, *end;
    __int64 cr = -2; // offset of the last '\r'
    __int64 len;
    fatalErr = FALSE;
    if (seek > 0) // not the beginning of the file
    {
        len = Prepare(hFile, seek - 1, 1, fatalErr);
        if (fatalErr)
            return FALSE;
        if (len == 1 && *(Buffer + (seek - Seek - 1)) == '\r')
            cr = seek - 1;
    }
    lineEnd = seek;
    nextLineBegin = -1;
    while (lineEnd <= maxSeek)
    {
        len = Prepare(hFile, lineEnd, APROX_LINE_LEN, fatalErr);
        if (fatalErr)
            break;
        if (len == 0)
        {
            nextLineBegin = lineEnd; // end of file
            break;
        }
        s = Buffer + (lineEnd - Seek);
        end = s + len;
        while (s < end)
        {
            if (*s <= '\r')
            {
                if (*s == '\r')
                {
                    if (Configuration.EOL_CR)
                        break;
                    cr = (s - Buffer) + Seek;
                }
                else
                {
                    if (*s == '\n')
                    {
                        if (cr + 1 == (s - Buffer) + Seek &&
                            Configuration.EOL_CRLF)
                        {
                            s--; // because of this, the condition '\r\n' below (*s may not be valid)
                            break;
                        }
                        if (Configuration.EOL_LF)
                            break;
                    }
                    else
                    {
                        if (*s == 0 && Configuration.EOL_NULL)
                            break;
                    }
                }
            }
            s++;
        }
        if (s < end) // found EOL
        {
            if (cr == (s - Buffer) + Seek) // '\r\n' already detected
            {
                lineEnd = (s - Buffer) + Seek;
                if (lineEnd > maxSeek)
                    break;
                nextLineBegin = lineEnd + 2;
            }
            else
            {
                lineEnd = (s - Buffer) + Seek;
                if (lineEnd > maxSeek)
                    break;
                nextLineBegin = lineEnd + 1;
                if (*s == '\r' && Configuration.EOL_CRLF) // test for '\r\n'
                {
                    len = Prepare(hFile, lineEnd, 2, fatalErr);
                    if (fatalErr)
                        break;
                    if (len == 2 && *(Buffer + (lineEnd - Seek + 1)) == '\n')
                        nextLineBegin++;
                }
            }
            break; // end of search
        }
        lineEnd += len;
    }
    return !fatalErr && nextLineBegin != -1;
}

BOOL CViewerWindow::FindPreviousEOL(HANDLE* hFile, __int64 seek, __int64 minSeek, __int64& lineBegin,
                                    __int64& previousLineEnd, BOOL allowWrap, BOOL takeLineBegin,
                                    BOOL& fatalErr, int* lines, __int64* firstLineEndOff,
                                    __int64* firstLineCharLen, BOOL addLineIfSeekIsWrap)
{
    SLOW_CALL_STACK_MESSAGE6("CViewerWindow::FindPreviousEOL(%g, %g, , , %d, %d, , , , , %d)",
                             (double)seek, (double)minSeek, allowWrap, takeLineBegin, addLineIfSeekIsWrap);

    if (firstLineEndOff != NULL)
        *firstLineEndOff = -1;
    if (firstLineCharLen != NULL)
        *firstLineCharLen = -1;
    BOOL collectTabs = allowWrap && WrapText || firstLineCharLen != NULL;
    unsigned char *s, *end;
    fatalErr = FALSE;
    __int64 lf = -2; // offset of the last '\n'
    __int64 len;
    if (seek < FileSize) // It is not the end of the file
    {
        len = Prepare(NULL, seek, 1, fatalErr);
        if (fatalErr)
            return FALSE;
        if (len == 1 && *(Buffer + (seek - Seek)) == '\n')
            lf = seek;
    }

    TDirectArray<__int64> tabs(1000, 500); // tabulator position in the line (assumption: there won't be many of them...)
    lineBegin = seek;
    previousLineEnd = -1;
    while (lineBegin >= minSeek)
    {
        if (!FindingSoDonotSwitchToHex && CanSwitchToHex && minSeek == 0 && !ForceTextMode &&
            seek - lineBegin > TEXT_MAX_LINE_LEN)
        {
            if (!CanSwitchQuietlyToHex)
                CanSwitchToHex = FALSE;
            if (CanSwitchQuietlyToHex ||
                SalMessageBoxViewerPaintBlocked(HWindow, LoadStr(IDS_VIEWER_BINFILE), LoadStr(IDS_VIEWERTITLE),
                                                MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                CanSwitchQuietlyToHex = FALSE;
                ExitTextMode = TRUE;
                PostMessage(HWindow, WM_COMMAND, CM_TO_HEX, 0);
                return FALSE;
            }
            else
                ForceTextMode = TRUE;
        }

        len = min(APROX_LINE_LEN, lineBegin);
        len = Prepare(NULL, lineBegin - len, len, fatalErr);
        if (fatalErr)
            break;
        if (len == 0)
        {
            previousLineEnd = lineBegin = 0; // start of the file
            break;
        }
        s = Buffer + (lineBegin - Seek - 1);
        end = s - len;
        while (s > end)
        {
            if (*s <= '\r')
            {
                if (*s == '\n')
                {
                    if (Configuration.EOL_LF)
                        break;
                    lf = (s - Buffer) + Seek;
                }
                else
                {
                    if (*s == '\r')
                    {
                        if (lf - 1 == (s - Buffer) + Seek &&
                            Configuration.EOL_CRLF)
                        {
                            s++; // because of this, the condition '\r\n' below (*s may not be valid)
                            break;
                        }
                        if (Configuration.EOL_CR)
                            break;
                    }
                    else
                    {
                        if (*s == 0 && Configuration.EOL_NULL)
                            break;
                        else
                        {
                            if (collectTabs && *s == '\t')
                                tabs.Add((s - Buffer) + Seek);
                        }
                    }
                }
            }
            s--;
        }
        if (s > end) // found EOL
        {
            if (lf == (s - Buffer) + Seek) // '\r\n' already detected
            {
                lineBegin = (s - Buffer) + Seek + 1;
                if (lineBegin < minSeek)
                    break;
                previousLineEnd = lineBegin - 2;
            }
            else
            {
                lineBegin = (s - Buffer) + Seek + 1;
                if (lineBegin < minSeek)
                    break;
                previousLineEnd = lineBegin - 1;
                if (*s == '\n' && Configuration.EOL_CRLF) // test for '\r\n'
                {
                    len = min(lineBegin, 2);
                    len = Prepare(NULL, lineBegin - len, len, fatalErr);
                    if (fatalErr)
                        break;
                    if (len == 2 && *(Buffer + (lineBegin - len - Seek)) == '\r')
                        previousLineEnd--;
                }
            }
            break; // end of search
        }
        lineBegin -= len;
    }

    // We do not consider the beginning of the file as the end of the previous line (a bit nonsensical, but for the purposes
    // previousLineEnd is taken like this); WARNING: any line wrapping is handled in the code below
    if (lineBegin > 0 && firstLineEndOff != NULL)
        *firstLineEndOff = previousLineEnd;

    if (!fatalErr && allowWrap && WrapText && Width > 0 && Height > 0) // wrap mode
    {
        int columns = (Width - BORDER_WIDTH) / CharWidth; // width of the window in characters
        __int64 lineLen = seek - lineBegin;               // length of line in bytes
        int tabsCount = tabs.Count;
        __int64 originalLineBegin = lineBegin;
        while (1)
        {
            __int64 tabAdd = 0; // how many spaces do tabulators add
            while (tabsCount > 0 && tabs[tabsCount - 1] + tabAdd < lineBegin + columns)
            {
                int tab = (int)(Configuration.TabSize - ((tabs[tabsCount - 1] + tabAdd - lineBegin) % Configuration.TabSize));
                if (tabs[tabsCount - 1] + tabAdd + tab > lineBegin + columns)
                {
                    tab = (int)(lineBegin + columns - tabs[tabsCount - 1] - tabAdd);
                }
                tabAdd += tab - 1;
                tabsCount--;
            }
            if ((takeLineBegin && lineBegin + columns - tabAdd > seek) || // "seek" is taken as the character offset on the line (at the interface, the line has the meaning of the beginning of the line)
                (!takeLineBegin && lineBegin + columns - tabAdd >= seek)) // "seek" is taken as an offset to the end of the line (at the boundary, the line has the meaning of the end of the line)
            {
                if (takeLineBegin && addLineIfSeekIsWrap && originalLineBegin < lineBegin && lineBegin == seek)
                {
                    // The beginning of this wrap-line is also the end of the previous wrap-line, 'addLineIfSeekIsWrap' is
                    // TRUE if 'seek' should be considered as the end of the previous (meaning skipping one extra "EOL")
                    if (lines != NULL)
                        (*lines)++;
                    addLineIfSeekIsWrap = FALSE; // We can do it only once
                }
                if (!takeLineBegin && firstLineCharLen != NULL) // "seek" is the end of the line, we will calculate its length
                {
                    *firstLineCharLen = seek - lineBegin + tabAdd;
                    firstLineCharLen = NULL; // we have what we wanted, no further adjustments (it would be about the lengths of the previous lines)
                }
                if (firstLineEndOff != NULL)
                {
                    if (originalLineBegin < lineBegin) // interrupted line
                        *firstLineEndOff = lineBegin;  // The beginning of this wrap-line is also the end of the previous wrap-line
                    firstLineEndOff = NULL;            // We have what we wanted, no further adjustments (potentially it could be the end of the previous lines up to the count of "lines")
                }

                if (originalLineBegin < lineBegin) // interrupted line
                {
                    if (lines != NULL && *lines > 0)
                    { // if we need to find the beginning of the next lines, we will do it one by one (to utilize the loaded data)
                        (*lines)--;
                        takeLineBegin = FALSE; // "seek" uz ted bereme jako offset konce radku
                        seek = lineBegin;
                        lineBegin = originalLineBegin;
                        tabsCount = tabs.Count;
                        continue;
                    }
                    previousLineEnd = lineBegin;
                }
                break;
            }
            lineBegin += columns - tabAdd; // change offset
        }
    }
    else
    {
        if (!fatalErr && !takeLineBegin && firstLineCharLen != NULL)
        {
            __int64 tabAdd = 0; // how many spaces do tabulators add
            int tabsCount = tabs.Count;
            while (tabsCount-- > 0)
            {
                int tab = (int)(Configuration.TabSize - ((tabs[tabsCount] + tabAdd - lineBegin) % Configuration.TabSize));
                tabAdd += tab - 1;
            }
            *firstLineCharLen = seek - lineBegin + tabAdd; // line length in bytes + additional for tabs
        }
    }
    return !fatalErr && previousLineEnd != -1;
}

__int64
CViewerWindow::FindSeekBefore(__int64 seek, int lines, BOOL& fatalErr, __int64* firstLineEndOff,
                              __int64* firstLineCharLen, BOOL addLineIfSeekIsWrap) // text display
{
    CALL_STACK_MESSAGE3("CViewerWindow::FindSeekBefore(%g, %d, , ,)", (double)seek, lines);
    fatalErr = FALSE;
    if (firstLineEndOff != NULL)
        *firstLineEndOff = -1;
    if (firstLineCharLen != NULL)
        *firstLineCharLen = -1;
    __int64 beg = seek, prevEnd;
    BOOL first = TRUE; // Adjust position at the beginning and end of the line when wrapping at the end of the line
                       // the first seek must take position at the beginning, the others at the end
    while (lines--)
    {
        FindPreviousEOL(NULL, seek, 0, beg, prevEnd, TRUE, first, fatalErr, &lines, first ? firstLineEndOff : NULL,
                        firstLineCharLen != NULL && *firstLineCharLen == -1 ? firstLineCharLen : NULL,
                        first ? addLineIfSeekIsWrap : FALSE);
        if (fatalErr || ExitTextMode)
            return 0;
        seek = prevEnd;
        first = FALSE;
    }
    return beg;
}

__int64
CViewerWindow::ZeroLineSize(BOOL& fatalErr, __int64* firstLineEndOff, __int64* firstLineCharLen)
{
    CALL_STACK_MESSAGE1("CViewerWindow::ZeroLineSize()");
    fatalErr = FALSE;
    if (firstLineEndOff != NULL)
        *firstLineEndOff = -1;
    if (firstLineCharLen != NULL)
        *firstLineCharLen = -1;
    switch (Type)
    {
    case vtHex:
        return 16; // WARNING: 'firstLineEndOff' and 'firstLineCharLen' are not calculated for Hex mode as it is not used yet
    case vtText:
    {
        __int64 offset = FindSeekBefore(SeekY, 2, fatalErr, firstLineEndOff, firstLineCharLen);
        if (fatalErr || ExitTextMode)
            return 0;
        return SeekY - offset;
    }
    }
    return 0;
}

__int64
CViewerWindow::FindBegin(__int64 seek, BOOL& fatalErr)
{
    CALL_STACK_MESSAGE2("CViewerWindow::FindBegin(%g,)", (double)seek);
    fatalErr = FALSE;
    switch (Type)
    {
    case vtHex:
        return seek - (seek % 16);
    case vtText:
        return FindSeekBefore(seek, 1, fatalErr);
    }
    return 0;
}

void CViewerWindow::ChangeType(CViewType type)
{
    CALL_STACK_MESSAGE2("CViewerWindow::ChangeType(%d)", type);
    __int64 startSel = StartSelection;
    __int64 endSel = EndSelection;
    SetToolTipOffset(-1);
    Type = type;
    __int64 oldSeekY = SeekY;
    BOOL fatalErr;
    FileChanged(NULL, FALSE, fatalErr, FALSE);
    if (!fatalErr && !ExitTextMode)
        FindNewSeekY(oldSeekY, fatalErr);
    if (fatalErr)
        FatalFileErrorOccured();
    if (fatalErr || ExitTextMode)
        return;
    if (startSel >= 0 && startSel < FileSize && // the selection was valid
        endSel >= 0 && endSel < FileSize)
    {
        StartSelection = startSel; // restore the selection (good for orientation when switching modes)
        EndSelection = endSel;
    }
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow); // to calculate the ViewSize for the next PageDown
}

BOOL CViewerWindow::GetOffsetOrXAbs(__int64 x, __int64* offset, __int64* offsetX, __int64 lineBegOff,
                                    __int64 lineCharLen, __int64 lineEndOff, BOOL& fatalErr, BOOL* onHexNum,
                                    BOOL getXFromOffset, __int64 findOffset, __int64* foundX)
{
    if (offset != NULL)
        *offset = -1;
    if (offsetX != NULL)
        *offsetX = x;
    fatalErr = FALSE;
    if (onHexNum != NULL)
        *onHexNum = FALSE;
    if (foundX != NULL)
        *foundX = -1;
    switch (Type)
    {
    case vtText:
    {
        if (lineBegOff > lineEndOff ||
            getXFromOffset && (findOffset < lineBegOff || findOffset > lineEndOff))
        {
            TRACE_C("Unexpected in CViewerWindow::GetOffsetOrXAbs().");
        }
        if (getXFromOffset ? findOffset == lineEndOff : x >= lineCharLen)
        {
            if (foundX != NULL)
                *foundX = lineCharLen;
            if (offset != NULL)
                *offset = lineEndOff;
            if (offsetX != NULL)
                *offsetX = lineCharLen;
            return TRUE;
        }
        if (getXFromOffset ? findOffset == lineBegOff : x <= 0)
        {
            if (foundX != NULL)
                *foundX = 0;
            if (offset != NULL)
                *offset = lineBegOff;
            if (offsetX != NULL)
                *offsetX = 0;
            return TRUE;
        }
        __int64 off = lineBegOff, lineLen = 0;
        while (1)
        {
            __int64 readLen = APROX_LINE_LEN;
            if (off + readLen > lineEndOff)
                readLen = lineEndOff - off;
            __int64 len = Prepare(NULL, off, readLen, fatalErr);
            if (fatalErr)
                return FALSE;
            if (len > 0)
            {
                char* s = (char*)(Buffer + (off - Seek));
                while (len--)
                {
                    if (*s == '\t')
                    {
                        int tab = (int)(Configuration.TabSize - (lineLen % Configuration.TabSize));
                        if (!getXFromOffset && x >= lineLen && x <= lineLen + tab)
                        {
                            if (offset != NULL)
                                *offset = off + (x > lineLen + tab / 2 ? 1 : 0);
                            if (offsetX != NULL)
                                *offsetX = lineLen + (x > lineLen + tab / 2 ? tab : 0);
                            return TRUE;
                        }
                        lineLen += tab;
                    }
                    else
                        lineLen++;
                    s++;
                    off++;
                    if (getXFromOffset ? findOffset == off : x == lineLen)
                    {
                        if (foundX != NULL)
                            *foundX = lineLen;
                        if (offset != NULL)
                            *offset = off;
                        if (offsetX != NULL)
                            *offsetX = lineLen;
                        return TRUE;
                    }
                }
            }
            else // the last line is empty, failed to load anything
            {
                if (offset != NULL)
                    *offset = lineBegOff;
                if (offsetX != NULL)
                    *offsetX = 0;
                if (foundX != NULL)
                    *foundX = 0;
                return TRUE;
            }
        }
        break; // it will never come to this
    }

    case vtHex:
    {
        if (getXFromOffset)
            TRACE_C("CViewerWindow::GetOffsetOrXAbs(): Unsupported function!"); // we only support text mode
        __int64 foundOff = -1;
        if (x < 62 - 8 + HexOffsetLength)
        {
            if (onHexNum != NULL)
            {
                *onHexNum = (x > 9 - 8 + HexOffsetLength && x < 61 - 8 + HexOffsetLength) &&
                            ((((x - (9 - 8 + HexOffsetLength)) % 13) % 3) >= 1);
            }
            if (x > 10 - 8 + HexOffsetLength)
            {
                x = ((x - (9 - 8 + HexOffsetLength)) - (x - (9 - 8 + HexOffsetLength)) / 13) / 3;
            }
            else
                x = 0;
            foundOff = lineBegOff + x;
            if (foundOff > FileSize)
                foundOff = FileSize;
        }
        else
        {
            if (onHexNum != NULL)
                *onHexNum = TRUE; // in text column, but directly on the character...
            if (x > 62 + 16 - 8 + HexOffsetLength)
                x = 62 + 16 - 8 + HexOffsetLength;
            foundOff = lineBegOff + (x - (62 - 8 + HexOffsetLength));
            if (foundOff > FileSize)
                foundOff = FileSize;
        }
        if (offset != NULL)
            *offset = foundOff;
        return TRUE;
    }
    }
    return FALSE;
}

BOOL CViewerWindow::GetOffset(__int64 x, __int64 y, __int64& offset, BOOL& fatalErr,
                              BOOL leftMost, BOOL* onHexNum)
{
    CALL_STACK_MESSAGE4("CViewerWindow::GetOffset(%d, %d, , , %d)", (int)x, (int)y, leftMost);
    fatalErr = FALSE;
    if (onHexNum != NULL)
        *onHexNum = FALSE;
    if (x >= 0 && y >= 0 && x < Width && y < Height)
    {
        if (!leftMost)
            x = (x - BORDER_WIDTH + CharWidth / 2) / CharWidth;
        else
            x = (x - BORDER_WIDTH) / CharWidth;
        y = y / CharHeight;
        switch (Type)
        {
        case vtText:
        {
            if (3 * y + 2 < LineOffset.Count)
            {
                return GetOffsetOrXAbs(x + OriginX, &offset, NULL, LineOffset[(int)(3 * y)], LineOffset[(int)(3 * y + 2)],
                                       LineOffset[(int)(3 * y + 1)], fatalErr, onHexNum);
            }
            break;
        }

        case vtHex:
            return GetOffsetOrXAbs(x + OriginX, &offset, NULL, SeekY + y * 16, 0, 0, fatalErr, onHexNum);
        }
    }
    return FALSE;
}

void CViewerWindow::SetScrollBar()
{
    CALL_STACK_MESSAGE1("CViewerWindow::SetScrollBar()");
    if (EnableSetScroll)
    { // vertical scrollbar
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(HWindow, SB_VERT, &si);

        __int64 max = ViewSize + MaxSeekY;
        ScrollScaleY = ((double)max) / 20000.0;
        if (ScrollScaleY < 0.00001)
            ScrollScaleY = 0.00001; // against "divide by zero"
        int page = (int)(ViewSize / ScrollScaleY + 0.5 + 1);
        if (max == 0 || si.nMin != 0 || si.nMax != max / ScrollScaleY + 0.5 + 1 ||
            si.nPage != (DWORD)page ||
            si.nPos != SeekY / ScrollScaleY + 0.5) // if it is necessary to set ...
        {
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
            si.nMin = 0;
            if (max != 0 && MaxSeekY != 0)
            {
                si.nMax = (int)(max / ScrollScaleY + 0.5 + 1);
                si.nPage = page;
                si.nPos = (int)(SeekY / ScrollScaleY + 0.5);
            }
            else
            {
                si.nMax = 0;
                si.nPage = 0;
                si.nPos = 0;
            }
            SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
        }

        // horizontal scrollbar
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(HWindow, SB_HORZ, &si);

        max = OriginX + (Width - BORDER_WIDTH) / CharWidth;
        __int64 maxLL = GetMaxVisibleLineLen();
        if (max < maxLL)
            max = maxLL;

        ScrollScaleX = ((double)max) / 20000.0;
        if (ScrollScaleX < 0.00001)
            ScrollScaleX = 0.00001; // against "divide by zero"
        page = (int)(((Width - BORDER_WIDTH) / CharWidth) / ScrollScaleX + 0.5 + 2);
        if (max == 0 || si.nMin != 0 || si.nMax != max / ScrollScaleX + 0.5 + 1 ||
            si.nPage != (DWORD)page ||
            si.nPos != OriginX / ScrollScaleX + 0.5) // if it is necessary to set ...
        {
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
            si.nMin = 0;
            if (max != 0)
            {
                si.nMax = (int)(max / ScrollScaleX + 0.5 + 1);
                si.nPage = page;
                si.nPos = (int)(OriginX / ScrollScaleX + 0.5);
            }
            else
            {
                si.nMax = 0;
                si.nPage = 0;
                si.nPos = 0;
            }
            SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);
        }
    }
}

BOOL CViewerWindow::GetFindText(char* buf, int& len)
{
    CALL_STACK_MESSAGE1("CViewerWindow::GetFindText()");
    len = 0;
    if (StartSelection == EndSelection)
        return FALSE;

    __int64 startSel = min(StartSelection, EndSelection);
    // if (startSel == -1) startSel = 0; // cannot happen (-1 can only be both at the same time and that won't happen here)
    __int64 endSel = max(StartSelection, EndSelection);
    // if (endSel == -1) endSel = 0; // cannot happen (-1 can only be both at the same time and that won't happen here)
    BOOL fatalErr = FALSE;

    if (endSel - startSel > FIND_TEXT_LEN - 1)
        endSel = startSel + FIND_TEXT_LEN - 1;

    char* s = buf;
    __int64 off = startSel;
    while (off < endSel)
    {
        int l = (int)Prepare(NULL, off, min(1000, endSel - off), fatalErr);
        if (fatalErr)
            break;
        if (l == 0)
            return FALSE;
        memcpy(s, Buffer + (off - Seek), l);
        s += l;
        off += l;
    }
    *s = 0;

    if (fatalErr)
    {
        FatalFileErrorOccured();
        return FALSE;
    }
    else
    {
        len = (int)(endSel - startSel);
        return TRUE;
    }
}

BOOL CViewerWindow::CheckSelectionIsNotTooBig(HWND parent, BOOL* msgBoxDisplayed)
{
    if (msgBoxDisplayed != NULL)
        *msgBoxDisplayed = FALSE;
    __int64 startSel = min(StartSelection, EndSelection);
    if (startSel == -1)
        startSel = 0;
    __int64 endSel = max(StartSelection, EndSelection);
    if (endSel == -1)
        endSel = 0;
    if (endSel - startSel > 100 * 1024 * 1024)
    {
        if (TooBigSelAction != 0 /* to ask*/)
            return TooBigSelAction == 1 /* YES*/;

        if (msgBoxDisplayed != NULL)
            *msgBoxDisplayed = TRUE;
        int res = SalMessageBox(parent, LoadStr(IDS_VIEWER_BLOCKTOOBIG), LoadStr(IDS_VIEWERTITLE),
                                MB_YESNOCANCEL | MB_ICONQUESTION);
        if (res == IDYES)
            TooBigSelAction = 2 /* NO*/; // Is the query skip? YES = Do not copy/do not drag
        if (res == IDNO)
            TooBigSelAction = 1 /* YES*/;
        return res == IDNO;
    }
    return TRUE; // smaller than 100MB = YES
}

HGLOBAL
CViewerWindow::GetSelectedText(BOOL& fatalErr)
{
    fatalErr = FALSE;
    CALL_STACK_MESSAGE1("CViewerWindow::GetSelectedText()");
    __int64 startSel = min(StartSelection, EndSelection);
    if (startSel == -1)
        startSel = 0;
    __int64 endSel = max(StartSelection, EndSelection);
    if (endSel == -1)
        endSel = 0;
    if (startSel == endSel)
        startSel = endSel = 0;
    BOOL lowMem = FALSE;
#ifndef _WIN64
    if (endSel - startSel < (unsigned __int64)0xFFFFFFFF) // we can at least try it (32-bit version really can't handle more than 4GB)
#endif                                                    // _WIN64
    {
        HGLOBAL h = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (DWORD)(endSel - startSel + 1)));
        if (h != NULL)
        {
            char* s = (char*)HANDLES(GlobalLock(h));
            if (s != NULL)
            {
                __int64 off = startSel, len;
                while (off < endSel)
                {
                    len = Prepare(NULL, off, min(1000, endSel - off), fatalErr);
                    if (fatalErr)
                        break;
                    if (len == 0)
                    {
                        HANDLES(GlobalUnlock(h));
                        NOHANDLES(GlobalFree(h));
                        return NULL;
                    }
                    memcpy(s, Buffer + (off - Seek), (int)len);
                    s += len;
                    off += len;
                }
                *s = 0;
                HANDLES(GlobalUnlock(h));
                if (!fatalErr)
                    return h;
            }
            else
                lowMem = TRUE;
            NOHANDLES(GlobalFree(h));
        }
        else
            lowMem = TRUE;
    }
#ifndef _WIN64
    else
        lowMem = TRUE;
#endif // _WIN64
    if (lowMem)
    {
        SalMessageBoxViewerPaintBlocked(HWindow, GetErrorText(ERROR_NOT_ENOUGH_MEMORY), LoadStr(IDS_ERRORTITLE),
                                        MB_OK | MB_ICONEXCLAMATION);
    }
    return NULL;
}

void CViewerWindow::SetToolTipOffset(__int64 offset)
{
    if (HToolTip != NULL)
    {
        if (ToolTipOffset != offset)
        {
            SendMessage(HToolTip, TTM_ACTIVATE, FALSE, 0);
        }
        if (offset != -1)
        {
            SendMessage(HToolTip, TTM_ACTIVATE, TRUE, 0);
        }
    }
    ToolTipOffset = offset;
}
