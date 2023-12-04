// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(push, 4)

struct CMessage
{
    int Size;
    int SenderID; // doplni message center pri odeslani
};

#pragma pack(pop)

class CMessageListener
{
public:
    virtual void RecieveMessage(const CMessage* message) = 0;
};

class CMessageCenter
{
public:
    CMessageCenter(const char* name, BOOL sender);
    ~CMessageCenter();

    BOOL IsGood() { return Good; }

    BOOL SendMessage(CMessage* message, BOOL bufferTimeout);
    BOOL RecieveMessages(CMessageListener* listener);
    BOOL WaitForMessage(BOOL& windowMessage);
    BOOL WaitForMessage(BOOL& success, DWORD timeout);
    BOOL WaitForMessage(BOOL& success, DWORD timeout, HANDLE cancel,
                        BOOL& canceled);
    const char* GetName() { return Name; }
    int GetSenderID() { return SenderID; }
    DWORD GetRecieverPid() { return RecieverPid; }
    BOOL Init();
    void Release();

private:
    struct CBuffer
    {
        DWORD Pid;
        int UniqueCounter;
        int WritePos;
    };

public:
    enum
    {
        BufferSize = 4094
    };
    enum
    {
        MaxMessage = BufferSize - sizeof(CBuffer)
    };

private:
    static const char* Version;
    BOOL Good;
    char* Name;
    BOOL Sender;
    HANDLE StartupMutex;
    HANDLE DataMutex;
    HANDLE BufferFree;
    HANDLE HaveMessage;
    HANDLE FileMapping;
    CBuffer* Buffer;
    HANDLE Reciever;   // jen pro odesilatele, handle ciloveho procesu
    DWORD RecieverPid; // jen pro odesilatele, id ciloveho procesu
    int SenderID;      // unikatni identifikator odesilatele
};
