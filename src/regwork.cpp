// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"

CRegistryWorkerThread RegistryWorkerThread;

// ****************************************************************************

BOOL ClearKeyAux(HKEY key)
{
    char name[MAX_PATH];
    HKEY subKey;
    while (RegEnumKey(key, 0, name, MAX_PATH) == ERROR_SUCCESS)
    {
        if (HANDLES_Q(RegOpenKeyEx(key, name, 0, KEY_READ | KEY_WRITE, &subKey)) == ERROR_SUCCESS)
        {
            BOOL ret = ClearKeyAux(subKey);
            HANDLES(RegCloseKey(subKey));
            if (!ret || RegDeleteKey(key, name) != ERROR_SUCCESS)
                return FALSE;
        }
        else
            return FALSE;
    }

    DWORD size = MAX_PATH;
    while (RegEnumValue(key, 0, name, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        if (RegDeleteValue(key, name) != ERROR_SUCCESS)
        {
            TRACE_E("Unable to delete values in specified key (in registry).");
            break;
        }
        else
            size = MAX_PATH;

    return TRUE;
}

// ****************************************************************************

BOOL CreateKeyAux(HWND parent, HKEY hKey, const char* name, HKEY& createdKey, BOOL quiet)
{
    DWORD createType; // info jestli byl klic vytvoren nebo jen otevren
    LONG res = HANDLES(RegCreateKeyEx(hKey, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                                      KEY_READ | KEY_WRITE, NULL, &createdKey,
                                      &createType));
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        if (!quiet)
        {
            if (HLanguage == NULL)
            {
                MessageBox(parent, GetErrorText(res), "Error Saving Configuration",
                           MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(parent, GetErrorText(res), LoadStr(IDS_ERRORSAVECONFIG),
                              MB_OK | MB_ICONEXCLAMATION);
            }
        }
        return FALSE;
    }
}

// ****************************************************************************

BOOL OpenKeyAux(HWND parent, HKEY hKey, const char* name, HKEY& openedKey, BOOL quiet)
{
    LONG res = HANDLES_Q(RegOpenKeyEx(hKey, name, 0, KEY_READ, &openedKey));
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        if (!quiet && res != ERROR_FILE_NOT_FOUND)
        {
            if (HLanguage == NULL)
            {
                MessageBox(parent, GetErrorText(res),
                           "Error Loading Configuration", MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(parent, GetErrorText(res),
                              LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        return FALSE;
    }
}

// ****************************************************************************

void CloseKeyAux(HKEY hKey)
{
    HANDLES(RegCloseKey(hKey));
}

// ****************************************************************************

BOOL DeleteKeyAux(HKEY hKey, const char* name)
{
    return RegDeleteKey(hKey, name) == ERROR_SUCCESS;
}

// ****************************************************************************

BOOL GetValueAux(HWND parent, HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSize, BOOL quiet)
{
    DWORD gettedType;
    LONG res = SalRegQueryValueEx(hKey, name, 0, &gettedType, (BYTE*)buffer, &bufferSize);
    if (res == ERROR_SUCCESS)
        if (gettedType == type)
            return TRUE;
        else
        {
            if (!quiet)
            {
                if (HLanguage == NULL)
                {
                    MessageBox(parent, "Unexpected value type.",
                               "Error Loading Configuration", MB_OK | MB_ICONEXCLAMATION);
                }
                else
                {
                    SalMessageBox(parent, LoadStr(IDS_UNEXPECTEDVALUETYPE),
                                  LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
                }
            }
            return FALSE;
        }
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
        {
            if (!quiet)
            {
                if (HLanguage == NULL)
                {
                    MessageBox(parent, GetErrorText(res),
                               "Error Loading Configuration", MB_OK | MB_ICONEXCLAMATION);
                }
                else
                {
                    SalMessageBox(parent, GetErrorText(res),
                                  LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
                }
            }
        }
        return FALSE;
    }
}

BOOL GetValue2Aux(HWND parent, HKEY hKey, const char* name, DWORD type1, DWORD type2, DWORD* returnedType, void* buffer, DWORD bufferSize)
{
    DWORD gettedType;
    LONG res = SalRegQueryValueEx(hKey, name, 0, &gettedType, (BYTE*)buffer, &bufferSize);
    if (res == ERROR_SUCCESS)
        if (gettedType == type1 || gettedType == type2)
        {
            *returnedType = gettedType;
            return TRUE;
        }
        else
        {
            if (HLanguage == NULL)
            {
                MessageBox(parent, "Unexpected value type.",
                           "Error Loading Configuration", MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(parent, LoadStr(IDS_UNEXPECTEDVALUETYPE),
                              LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
            }
            return FALSE;
        }
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
        {
            if (HLanguage == NULL)
            {
                MessageBox(parent, GetErrorText(res),
                           "Error Loading Configuration", MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(parent, GetErrorText(res),
                              LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        return FALSE;
    }
}

BOOL GetValueDontCheckTypeAux(HKEY hKey, const char* name, void* buffer, DWORD bufferSize)
{
    return SalRegQueryValueEx(hKey, name, 0, NULL, (BYTE*)buffer, &bufferSize) == ERROR_SUCCESS;
}

// ****************************************************************************

BOOL SetValueAux(HWND parent, HKEY hKey, const char* name, DWORD type,
                 const void* data, DWORD dataSize, BOOL quiet)
{
    if (dataSize == -1)
        dataSize = (DWORD)strlen((char*)data) + 1;
    LONG res = RegSetValueEx(hKey, name, 0, type, (CONST BYTE*)data, dataSize);
    if (res == ERROR_SUCCESS)
        return TRUE;
    else
    {
        if (!quiet)
        {
            if (HLanguage == NULL)
            {
                MessageBox(parent, GetErrorText(res),
                           "Error Saving Configuration", MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(parent, GetErrorText(res),
                              LoadStr(IDS_ERRORSAVECONFIG), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        return FALSE;
    }
}

// ****************************************************************************

BOOL DeleteValueAux(HKEY hKey, const char* name)
{
    return RegDeleteValue(hKey, name) == ERROR_SUCCESS;
}

// ****************************************************************************

BOOL GetSizeAux(HWND parent, HKEY hKey, const char* name, DWORD type, DWORD& bufferSize)
{
    DWORD gettedType;
    LONG res = SalRegQueryValueEx(hKey, name, 0, &gettedType, NULL, &bufferSize);
    if (res == ERROR_SUCCESS)
        if (gettedType == type)
            return TRUE;
        else
        {
            if (HLanguage == NULL)
            {
                MessageBox(parent, "Unexpected value type.",
                           "Error Loading Configuration", MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(parent, LoadStr(IDS_UNEXPECTEDVALUETYPE),
                              LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
            }
            return FALSE;
        }
    else
    {
        if (res != ERROR_FILE_NOT_FOUND)
        {
            if (HLanguage == NULL)
            {
                MessageBox(parent, GetErrorText(res),
                           "Error Loading Configuration", MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(parent, GetErrorText(res),
                              LoadStr(IDS_ERRORLOADCONFIG), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        return FALSE;
    }
}

//
// ****************************************************************************
// CRegistryWorkerThread
//

CRegistryWorkerThread::CRegistryWorkerThread()
{
    Thread = NULL;
    OwnerTID = 0; // invalid TID
    StopWorkerSkipCount = 0;
    InUse = FALSE;

    WorkReady = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    WorkDone = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));

    WorkType = rwtNone;
    LastWorkSuccess = FALSE;
    Key = NULL;
    Name = NULL;
    OpenedKey = NULL;
    ValueType = 0;
    ValueType2 = 0;
    ReturnedValueType = NULL;
    Buffer = NULL;
    BufferSize = 0;
    Data = NULL;
    DataSize = 0;
}

CRegistryWorkerThread::~CRegistryWorkerThread()
{
    if (Thread != NULL)
        TRACE_E("CRegistryWorkerThread::~CRegistryWorkerThread(): you must call StopThread()!");
    if (WorkReady != NULL)
        HANDLES(CloseHandle(WorkReady));
    if (WorkDone != NULL)
        HANDLES(CloseHandle(WorkDone));
    if (WorkType != rwtNone)
        TRACE_E("CRegistryWorkerThread::~CRegistryWorkerThread(): unexpected situation WorkType != rwtNone");
}

BOOL CRegistryWorkerThread::StartThread()
{
    BOOL ret = FALSE;
    if (Thread == NULL)
    {
        if (WorkReady != NULL && WorkDone != NULL)
        {
            // clean-up
            if (InUse)
                TRACE_E("CRegistryWorkerThread::StartThread(): thread is not running and InUse is TRUE!");
            InUse = FALSE; // jen pro sychr, na tomto miste nemuze byt TRUE
            ResetEvent(WorkReady);
            ResetEvent(WorkDone);
            WorkType = rwtNone;

            DWORD threadID;
            Thread = HANDLES(CreateThread(NULL, 0, CRegistryWorkerThread::ThreadBody, (void*)this, 0, &threadID));
            if (Thread != NULL)
            {
                OwnerTID = GetCurrentThreadId(); // povolime pouzivani pro tento thread
                StopWorkerSkipCount = 0;
                int level = GetThreadPriority(GetCurrentThread());
                SetThreadPriority(Thread, level);
                ret = TRUE; // uspech!
            }
            else
                TRACE_E("CRegistryWorkerThread::StartThread(): unable to start registry-worker thread!");
        }
        else
            TRACE_E("CRegistryWorkerThread::StartThread(): unable to start thread, neccessary event-objects are not OK!");
    }
    else
    {
        TRACE_E("CRegistryWorkerThread::StartThread(): thread is already running!");
        if (OwnerTID == GetCurrentThreadId())
            StopWorkerSkipCount++;
    }
    return ret;
}

void CRegistryWorkerThread::StopThread()
{
    if (Thread != NULL)
    {
        if (OwnerTID == GetCurrentThreadId())
        {
            if (StopWorkerSkipCount > 0)
            {
                StopWorkerSkipCount--;
                TRACE_E("CRegistryWorkerThread::StopThread(): ignoring call!");
            }
            else
            {
                CInUseHandler i;
                if (i.CanUseThread(this))
                {
                    WorkType = rwtStopWorker;

                    WaitForWorkDoneWithMessageLoop();
                    WaitForSingleObject(Thread, INFINITE);
                    HANDLES(CloseHandle(Thread));
                    Thread = NULL;
                    OwnerTID = 0; // invalid TID
                }
                else // nikdy by se nemelo stat
                {
                    // prevence dead-locku, pokud tento thread uz ma praci v registry worker
                    // threadu, jejiho dokonceni se zde nelze dockat
                    TRACE_E("CRegistryWorkerThread::StopThread(): preventing dead lock, skipping stop signal!");
                }
            }
        }
        else
            TRACE_E("CRegistryWorkerThread::StopThread(): you can stop thread only from the same thread you have started it!");
    }
    else
        TRACE_E("CRegistryWorkerThread::StopThread(): thread is not running!");
}

void CRegistryWorkerThread::WaitForWorkDoneWithMessageLoop()
{
    SLOW_CALL_STACK_MESSAGE1("WaitForWorkDoneWithMessageLoop()");

    SetEvent(WorkReady);
    while (1)
    {
        DWORD waitRes = MsgWaitForMultipleObjects(1, &WorkDone, FALSE, INFINITE, QS_ALLINPUT);
        if (waitRes == WAIT_OBJECT_0)
            break; // prace je hotova, pokracujeme...
        else
        {
            if (waitRes == WAIT_OBJECT_0 + 1) // new input
            {
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }
}

BOOL CRegistryWorkerThread::ClearKey(HKEY key)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtClearKey;
        LastWorkSuccess = FALSE;
        Key = key;

        WaitForWorkDoneWithMessageLoop();
        return LastWorkSuccess;
    }
    else
        return ClearKeyAux(key);
}

BOOL CRegistryWorkerThread::CreateKey(HKEY key, const char* name, HKEY& createdKey)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtCreateKey;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;
        OpenedKey = NULL;

        WaitForWorkDoneWithMessageLoop();
        createdKey = OpenedKey;
        return LastWorkSuccess;
    }
    else
        return CreateKeyAux(MainWindow != NULL ? MainWindow->HWindow : NULL, key, name, createdKey, FALSE);
}

BOOL CRegistryWorkerThread::OpenKey(HKEY key, const char* name, HKEY& openedKey)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtOpenKey;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;
        OpenedKey = NULL;

        WaitForWorkDoneWithMessageLoop();
        openedKey = OpenedKey;
        return LastWorkSuccess;
    }
    else
    {
        return OpenKeyAux(MainWindow != NULL ? MainWindow->HWindow : NULL,
                          key, name, openedKey, FALSE);
    }
}

void CRegistryWorkerThread::CloseKey(HKEY key)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtCloseKey;
        Key = key;

        WaitForWorkDoneWithMessageLoop();
    }
    else
        CloseKeyAux(key);
}

BOOL CRegistryWorkerThread::DeleteKey(HKEY key, const char* name)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtDeleteKey;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;

        WaitForWorkDoneWithMessageLoop();
        return LastWorkSuccess;
    }
    else
        return DeleteKeyAux(key, name);
}

BOOL CRegistryWorkerThread::GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtGetValue;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;
        ValueType = type;
        Buffer = buffer;
        BufferSize = bufferSize;

        WaitForWorkDoneWithMessageLoop();
        return LastWorkSuccess;
    }
    else
    {
        return GetValueAux(MainWindow != NULL ? MainWindow->HWindow : NULL,
                           key, name, type, buffer, bufferSize, FALSE);
    }
}

BOOL CRegistryWorkerThread::GetValue2(HKEY key, const char* name, DWORD type1, DWORD type2, DWORD* returnedType, void* buffer, DWORD bufferSize)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtGetValue2;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;
        ValueType = type1;
        ValueType2 = type2;
        ReturnedValueType = returnedType;
        Buffer = buffer;
        BufferSize = bufferSize;

        WaitForWorkDoneWithMessageLoop();
        return LastWorkSuccess;
    }
    else
    {
        return GetValue2Aux(MainWindow != NULL ? MainWindow->HWindow : NULL,
                            key, name, type1, type2, returnedType, buffer, bufferSize);
    }
}

BOOL CRegistryWorkerThread::SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtSetValue;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;
        ValueType = type;
        Data = data;
        DataSize = dataSize;

        WaitForWorkDoneWithMessageLoop();
        return LastWorkSuccess;
    }
    else
        return SetValueAux(MainWindow != NULL ? MainWindow->HWindow : NULL, key, name, type, data, dataSize, FALSE);
}

BOOL CRegistryWorkerThread::DeleteValue(HKEY key, const char* name)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtDeleteValue;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;

        WaitForWorkDoneWithMessageLoop();
        return LastWorkSuccess;
    }
    else
        return DeleteValueAux(key, name);
}

BOOL CRegistryWorkerThread::GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize)
{
    CInUseHandler i;
    if (i.CanUseThread(this))
    {
        WorkType = rwtGetSize;
        LastWorkSuccess = FALSE;
        Key = key;
        Name = name;
        ValueType = type;
        BufferSize = 0;

        WaitForWorkDoneWithMessageLoop();
        bufferSize = BufferSize;
        return LastWorkSuccess;
    }
    else
        return GetSizeAux(MainWindow != NULL ? MainWindow->HWindow : NULL, key, name, type, bufferSize);
}

unsigned
CRegistryWorkerThread::Body()
{
    CALL_STACK_MESSAGE1("CRegistryWorkerThread::Body()");
    SetThreadNameInVCAndTrace("RegistryWorker");
    TRACE_I("Begin");

    int loops = 0;
    while (1)
    {
        DWORD res = WaitForMultipleObjects(1, &WorkReady, FALSE, INFINITE);
        switch (res)
        {
        case WAIT_OBJECT_0 + 0: // WorkReady
        {
            loops++;
            switch (WorkType)
            {
            case rwtStopWorker:
            {
                TRACE_I("CRegistryWorkerThread::Body(): loops: " << loops);
                TRACE_I("End");
                WorkType = rwtNone;
                SetEvent(WorkDone);
                return 0; // ukoncime thread
            }

            case rwtClearKey:
                LastWorkSuccess = ClearKeyAux(Key);
                break;
            case rwtCreateKey:
                LastWorkSuccess = CreateKeyAux(NULL, Key, Name, OpenedKey, FALSE);
                break;
            case rwtOpenKey:
                LastWorkSuccess = OpenKeyAux(NULL, Key, Name, OpenedKey, FALSE);
                break;
            case rwtCloseKey:
                CloseKeyAux(Key);
                break;
            case rwtDeleteKey:
                LastWorkSuccess = DeleteKeyAux(Key, Name);
                break;
            case rwtGetValue:
                LastWorkSuccess = GetValueAux(NULL, Key, Name, ValueType, Buffer, BufferSize, FALSE);
                break;
            case rwtGetValue2:
                LastWorkSuccess = GetValue2Aux(NULL, Key, Name, ValueType, ValueType2, ReturnedValueType, Buffer, BufferSize);
                break;
            case rwtSetValue:
                LastWorkSuccess = SetValueAux(NULL, Key, Name, ValueType, Data, DataSize, FALSE);
                break;
            case rwtDeleteValue:
                LastWorkSuccess = DeleteValueAux(Key, Name);
                break;
            case rwtGetSize:
                LastWorkSuccess = GetSizeAux(NULL, Key, Name, ValueType, BufferSize);
                break;
            }
            WorkType = rwtNone;
            SetEvent(WorkDone);
            break;
        }
        }
    }
}

unsigned
CRegistryWorkerThread::ThreadBodyFEH(void* param)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ((CRegistryWorkerThread*)param)->Body();
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("CRegistryWorkerThread::ThreadBodyFEH: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
    }
    return 0;
#endif // CALLSTK_DISABLE
}

DWORD WINAPI
CRegistryWorkerThread::ThreadBody(void* param)
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    ThreadBodyFEH(param);
    return 0;
}

CRegistryWorkerThread::CInUseHandler::~CInUseHandler()
{
    if (T != NULL)
        T->InUse = FALSE;
}

BOOL CRegistryWorkerThread::CInUseHandler::CanUseThread(CRegistryWorkerThread* t)
{
    if (t->Thread != NULL && t->OwnerTID == GetCurrentThreadId())
    { // prace ve workerovi se tyka jen threadu, ktery ho nastartoval
        BOOL ret = !t->InUse;
        if (ret) // prace se muze spustit v threadu registry workera
        {
            t->InUse = TRUE;
            T = t; // v destruktoru se da T->InUse = FALSE
        }
        // else  // rekurzivni volani (diky message-loope a tim distribuci zprav) = praci v threadu odmitneme
        return ret;
    }
    return FALSE;
}
