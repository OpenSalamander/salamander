// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// funkce pro pohodlnou praci s Registry + zadne hlasky o LOAD a SAVE konfigurace pri chybach
BOOL OpenKeyAux(HWND parent, HKEY hKey, const char* name, HKEY& openedKey, BOOL quiet = TRUE);
BOOL CreateKeyAux(HWND parent, HKEY hKey, const char* name, HKEY& createdKey, BOOL quiet = TRUE);
BOOL GetValueAux(HWND parent, HKEY hKey, const char* name, DWORD type, void* buffer,
                 DWORD bufferSize, BOOL quiet = TRUE);
BOOL SetValueAux(HWND parent, HKEY hKey, const char* name, DWORD type,
                 const void* data, DWORD dataSize, BOOL quiet = TRUE);
BOOL DeleteValueAux(HKEY hKey, const char* name);
BOOL ClearKeyAux(HKEY key);
void CloseKeyAux(HKEY hKey);
BOOL DeleteKeyAux(HKEY hKey, const char* name);
// neprovede kontrolu typu, takze nacte REG_DWORD stejne jako 4-byte REG_BINARY
BOOL GetValueDontCheckTypeAux(HKEY hKey, const char* name, void* buffer, DWORD bufferSize);

enum CRegistryWorkType
{
    rwtNone,
    rwtStopWorker, // servisni prace: ukonceni threadu
    rwtClearKey,
    rwtCreateKey,
    rwtOpenKey,
    rwtCloseKey,
    rwtDeleteKey,
    rwtGetValue,
    rwtGetValue2,
    rwtSetValue,
    rwtDeleteValue,
    rwtGetSize,
};

class CRegistryWorkerThread
{
protected:
    class CInUseHandler
    {
    protected:
        CRegistryWorkerThread* T;

    public:
        CInUseHandler() { T = NULL; }
        ~CInUseHandler();
        BOOL CanUseThread(CRegistryWorkerThread* t);
        void ResetT() { T = NULL; }
    };

    HANDLE Thread;           // thread registry-workera
    DWORD OwnerTID;          // TID threadu, ktery spustil worker thread (nikdo jiny ho nemuze ukoncit)
    BOOL InUse;              // TRUE = jiz nejakou praci provadi, dalsi prace se spusti bez threadu (resi rekurzi, pouziti z vice threadu se odmita, viz OwnerTID)
    int StopWorkerSkipCount; // kolik volani StopThread() v threadu OwnerTID ignorovat (pocet rekurzivnich volani StartThread())

    HANDLE WorkReady; // signaled: thread ma pripravena data ke zpracovani (hlavni thread ceka na dokonceni + provadi message-loopu)
    HANDLE WorkDone;  // signaled: thread dokoncil praci (hlavni thread muze pokracovat)

    CRegistryWorkType WorkType;
    BOOL LastWorkSuccess;
    HKEY Key;
    const char* Name;
    HKEY OpenedKey;
    DWORD ValueType;
    DWORD ValueType2;
    DWORD* ReturnedValueType;
    void* Buffer;
    DWORD BufferSize;
    const void* Data;
    DWORD DataSize;

public:
    CRegistryWorkerThread();
    ~CRegistryWorkerThread();

    // start threadu registry-workera, vraci uspech
    BOOL StartThread();

    // ukonceni threadu registry-workera
    void StopThread();

    // vycisti klic 'key' od vsech podklicu a hodnot, vraci uspech
    BOOL ClearKey(HKEY key);

    // vytvori nebo otevre existujici podklic 'name' klice 'key', vraci 'createdKey' a uspech;
    // ziskany klic ('createdKey') je nutne zavrit volanim CloseKey
    BOOL CreateKey(HKEY key, const char* name, HKEY& createdKey);

    // otevre existujici podklic 'name' klice 'key', vraci 'openedKey' a uspech
    // ziskany klic ('openedKey') je nutne zavrit volanim CloseKey
    BOOL OpenKey(HKEY key, const char* name, HKEY& openedKey);

    // zavre klic otevreny pres OpenKey nebo CreateKey
    void CloseKey(HKEY key);

    // smaze podklic 'name' klice 'key', vraci uspech
    BOOL DeleteKey(HKEY key, const char* name);

    // nacte hodnotu 'name'+'type'+'buffer'+'bufferSize' z klice 'key', vraci uspech
    BOOL GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize);

    // nacte hodnotu 'name'+'type1 || type2' do 'returnedType'+'buffer'+'bufferSize' z klice 'key', vraci uspech
    BOOL GetValue2(HKEY hKey, const char* name, DWORD type1, DWORD type2, DWORD* returnedType, void* buffer, DWORD bufferSize);

    // ulozi hodnotu 'name'+'type'+'data'+'dataSize' do klice 'key', pro retezce je mozne
    // zadat 'dataSize' == -1 -> vypocet delky retezce pomoci funkce strlen,
    // vraci uspech
    BOOL SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize);

    // smaze hodnotu 'name' klice 'key', vraci uspech
    BOOL DeleteValue(HKEY key, const char* name);

    // vytahne do 'bufferSize' protrebnou velikost pro hodnotu 'name'+'type' z klice 'key', vraci uspech
    BOOL GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize);

protected:
    // ceka na dokonceni prace + provadi message-loopu
    void WaitForWorkDoneWithMessageLoop();

    // telo threadu - zde se odehrava vsechna prace
    unsigned Body();

    static DWORD WINAPI ThreadBody(void* param); // pomocna funkce pro telo threadu
    static unsigned ThreadBodyFEH(void* param);  // pomocna funkce pro telo threadu
};

extern CRegistryWorkerThread RegistryWorkerThread;
