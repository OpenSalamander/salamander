// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "messages.h"

// ****************************************************************************
//
// CMessageCenter
//

const char* CMessageCenter::Version = "1";

CMessageCenter::CMessageCenter(const char* name, BOOL sender)
{
    CALL_STACK_MESSAGE3("CMessageCenter::CMessageCenter(%s, %d)", name, sender);
    lstrcpy(Name = new char[lstrlen(name) + 1], name);
    Sender = sender;

    StartupMutex = NULL;
    Buffer = NULL;
    FileMapping = NULL;
    DataMutex = NULL;
    BufferFree = NULL;
    HaveMessage = NULL;
    Reciever = NULL;

    Init();
}

CMessageCenter::~CMessageCenter()
{
    CALL_STACK_MESSAGE1("CMessageCenter::~CMessageCenter()");
    Release();
    if (Name)
        delete[] Name;
}

void my_memcpy2(void* dst, const void* src, int len)
{
    char* d = (char*)dst;
    const char* s = (char*)src;
    while (len--)
        *d++ = *s++;
}

BOOL CMessageCenter::SendMessage(CMessage* message, BOOL bufferTimeout)
{
    CALL_STACK_MESSAGE2("CMessageCenter::SendMessage(, %d)", bufferTimeout);
    if (!IsGood())
        return FALSE;
    if (!Sender)
        return FALSE;
    if (message->Size >= MaxMessage)
    {
        TRACE_E("Message is too big.");
        return FALSE;
    }

    while (1)
    {
        // vlezeme do kriticke sekce
        if (WaitForSingleObject(DataMutex, INFINITE) != WAIT_OBJECT_0)
        {
            TRACE_E("Unable to enter critical section for sending message.");
            break;
        }

        if (Buffer->Pid != RecieverPid)
            break;

        if (Buffer->WritePos + message->Size <= BufferSize)
        {
            // nastavime ID odesilatele
            message->SenderID = SenderID;
            // provedeme zapis
            my_memcpy2((char*)Buffer + Buffer->WritePos, message, message->Size);
            // informuje prijemce, ze ma v bufferu zpravu
            if (Buffer->WritePos == sizeof(CBuffer))
            {
                SetEvent(HaveMessage);
                ResetEvent(BufferFree);
            }
            Buffer->WritePos += message->Size;
            // uvolnime kritickou sekci
            ReleaseMutex(DataMutex);
            return TRUE; // uspech
        }

        // uvolnime kritickou sekci
        ReleaseMutex(DataMutex);

        // pockame az se uvolni misto v bufferu, nebo server chcipne
        HANDLE handles[] = {BufferFree, Reciever};
        DWORD ret = WaitForMultipleObjects(2, handles, FALSE, bufferTimeout);
        if (ret == WAIT_TIMEOUT)
            return FALSE;
        if (ret != WAIT_OBJECT_0)
            break;
    }

    Release();

    return FALSE;
}

#define MESSAGE_AT_POS(p) ((CMessage*)((char*)Buffer + p))

BOOL CMessageCenter::RecieveMessages(CMessageListener* listener)
{
    CALL_STACK_MESSAGE1("CMessageCenter::RecieveMessages()");
    if (!IsGood())
        return FALSE;
    if (Sender)
        return FALSE;

    // vlezeme do kriticke sekce
    DWORD ret = WaitForSingleObject(DataMutex, INFINITE);
    if (ret == WAIT_FAILED)
    {
        TRACE_E("Unable to enter critical section for receiving message.");
        Release();
        return FALSE;
    }
    if (ret == WAIT_OBJECT_0)
    {
        // vyprazdnime message buffer
        for (int pos = sizeof(CBuffer); pos < Buffer->WritePos;
             pos += MESSAGE_AT_POS(pos)->Size)
        {
            if (MESSAGE_AT_POS(pos)->Size == 0)
                break; // to by byl nekonecny cyklus a navic je to nesmysl
            listener->RecieveMessage(MESSAGE_AT_POS(pos));
        }
    }
    // nastavime buffer na prazdny (i pro abadoned data-mutex)
    Buffer->WritePos = sizeof(CBuffer);
    SetEvent(BufferFree);
    ResetEvent(HaveMessage);
    // uvolnime kritickou sekci
    ReleaseMutex(DataMutex);
    return TRUE;
}

BOOL CMessageCenter::WaitForMessage(BOOL& windowMessage)
{
    CALL_STACK_MESSAGE_NONE
    if (!IsGood())
        return FALSE;
    if (Sender)
        return FALSE;

    switch (MsgWaitForMultipleObjects(1, &HaveMessage, FALSE, INFINITE,
                                      QS_ALLINPUT))
    {
    case WAIT_OBJECT_0:
        windowMessage = FALSE;
        return TRUE;
    case WAIT_OBJECT_0 + 1:
        windowMessage = TRUE;
        return TRUE;
    }
    TRACE_E("Unable to wait for message. MsgWaitForMultipleObjects returned error.");
    Release();
    return FALSE;
}

BOOL CMessageCenter::WaitForMessage(BOOL& success, DWORD timeout)
{
    CALL_STACK_MESSAGE_NONE
    BOOL dummy;
    return WaitForMessage(success, timeout, NULL, dummy);
}

BOOL CMessageCenter::WaitForMessage(BOOL& success, DWORD timeout, HANDLE cancel,
                                    BOOL& canceled)
{
    CALL_STACK_MESSAGE_NONE
    if (!IsGood())
        return FALSE;
    if (Sender)
        return FALSE;

    HANDLE handles[] = {HaveMessage, cancel};
    switch (WaitForMultipleObjects(cancel == NULL ? 1 : 2, handles, FALSE, timeout))
    {
    case WAIT_OBJECT_0:
        success = TRUE;
        canceled = FALSE;
        return TRUE;
    case WAIT_OBJECT_0 + 1:
        success = FALSE;
        canceled = TRUE;
        return TRUE;
    case WAIT_TIMEOUT:
        success = FALSE;
        canceled = FALSE;
        return TRUE;
    }
    TRACE_E("Unable to wait for message. MsgWaitForMultipleObjects returned error.");
    Release();
    return FALSE;
}

BOOL CMessageCenter::Init()
{
    CALL_STACK_MESSAGE1("CMessageCenter::Init()");
    BOOL ret = FALSE;
    const char* str;

    do
    {
        // initem muze prochazet pouze jeden process
        str = Concatenate(Name, " - Startup Mutex");
        if (Sender)
        {
            // odesilatel se muze aktivovat, jedine pokud bezi prijemce
            StartupMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, str);
            if (!StartupMutex)
            {
                TRACE_I("Unable to open starting mutex -- server is probably not yet running.");
                break;
            }
            if (WaitForSingleObject(StartupMutex, INFINITE) != WAIT_OBJECT_0)
                break;
        }
        else
        {
            StartupMutex = CreateMutex(NULL, TRUE, str);
            if (!StartupMutex)
            {
                TRACE_E("Unable to create starting mutex.");
                break;
            }
            // jde pustit pouze jeden prijemce
            if (GetLastError() == ERROR_ALREADY_EXISTS)
            {
                TRACE_I("Receiver '" << Name << "' is already running.");
                break;
            }
        }

        // vytvorime synchronizacni objekty
        str = Concatenate(Name, " - Data Mutex");
        DataMutex = Sender ? OpenMutex(MUTEX_ALL_ACCESS, FALSE, str) : CreateMutex(NULL, FALSE, str);
        if (!DataMutex)
        {
            TRACE_E("Unable to create data mutex.");
            break;
        }

        str = Concatenate(Name, " - Buffer Full");
        BufferFree = Sender ? OpenEvent(EVENT_ALL_ACCESS, FALSE, str) : CreateEvent(NULL, TRUE, FALSE, str);
        str = Concatenate(Name, " - Have Message");
        HaveMessage = Sender ? OpenEvent(EVENT_ALL_ACCESS, FALSE, str) : CreateEvent(NULL, TRUE, FALSE, str);
        if (!BufferFree || !HaveMessage)
        {
            TRACE_E("Unable to create event.");
            break;
        }

        // vytvorime sdilenou cast pameti
        const char* mapname = Concatenate(
            Concatenate(Name, " - Buffer v"), Version);
        FileMapping = Sender ? OpenFileMapping(FILE_MAP_WRITE, FALSE, mapname) : CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
                                                                                                   BufferSize, mapname);
        if (!FileMapping)
        {
            TRACE_E("Unable to create file mapping.");
            break;
        }

        Buffer = (CBuffer*)MapViewOfFile(FileMapping, FILE_MAP_WRITE, 0, 0, 0);
        if (!Buffer)
        {
            TRACE_E("Unable to map shared memory.");
            break;
        }

        if (Sender)
        {
            // vlezeme do kriticke sekce
            if (WaitForSingleObject(DataMutex, INFINITE) != WAIT_OBJECT_0)
            {
                TRACE_E("Unable to enter critical section.");
                break;
            }
            // otevreme si handle na prijemce
            RecieverPid = Buffer->Pid;
            Reciever = OpenProcess(SYNCHRONIZE, FALSE, RecieverPid);
            if (!Reciever)
            {
                TRACE_E("Unable to get receiver's handle.");
                break;
            }
            SenderID = Buffer->UniqueCounter++;
            // uvolnime kritickou sekci
            ReleaseMutex(DataMutex);
        }
        else
        {
            // vlezeme do kriticke sekce
            if (WaitForSingleObject(DataMutex, INFINITE) == WAIT_FAILED)
                break;
            // inicializujeme buffer
            Buffer->Pid = GetCurrentProcessId();
            Buffer->UniqueCounter = 0;
            Buffer->WritePos = sizeof(CBuffer);
            SetEvent(BufferFree);
            ResetEvent(HaveMessage);
            // uvolnime kritickou sekci
            ReleaseMutex(DataMutex);
        }

        // nechame ostatni procesy projit initem
        ReleaseMutex(StartupMutex);
        if (Sender)
        {
            CloseHandle(StartupMutex);
            StartupMutex = NULL;
        }

        ret = TRUE;
    } while (0);

    if (!ret)
        Release();

    return Good = ret;
}

void CMessageCenter::Release()
{
    CALL_STACK_MESSAGE1("CMessageCenter::Release()");
    if (Buffer)
        UnmapViewOfFile(Buffer);
    if (FileMapping)
        CloseHandle(FileMapping);
    if (DataMutex)
        CloseHandle(DataMutex);
    if (BufferFree)
        CloseHandle(BufferFree);
    if (HaveMessage)
        CloseHandle(HaveMessage);
    if (Reciever)
        CloseHandle(Reciever);
    if (StartupMutex)
    {
        ReleaseMutex(StartupMutex);
        CloseHandle(StartupMutex);
    }

    StartupMutex = NULL;
    Buffer = NULL;
    FileMapping = NULL;
    DataMutex = NULL;
    BufferFree = NULL;
    HaveMessage = NULL;
    Reciever = NULL;

    Good = FALSE;
}
