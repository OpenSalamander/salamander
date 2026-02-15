// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <crtdbg.h>
#include <stdio.h>
#include <ostream>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "array.h"

#ifdef INSIDE_SALAMANDER
#pragma message(__FILE__ " ERROR: Using wrong precomp.h header file! You should use Salamander's precomp.h file!")
#endif

#ifdef UNICODE
#define CSalamanderRegistryAbstract CSalamanderRegistryAbstractW
#else
#define CSalamanderRegistryAbstract CSalamanderRegistryAbstractA
#endif

//
// WARNING: this class is copied from spl_base.h for simplicity (it would be
// better to include spl_base.h)
//
// ****************************************************************************
// CSalamanderRegistryAbstract
//
// Salamander's set of methods for working with the system registry,
// used in CPluginInterfaceAbstract::LoadConfiguration
// and CPluginInterfaceAbstract::SaveConfiguration

class CSalamanderRegistryAbstract
{
public:
    // clears 'key' from all subkeys and values, returns success
    virtual BOOL WINAPI ClearKey(HKEY key) = 0;

    // creates or opens the existing subkey 'name' of key 'key', returns
    // 'createdKey' and success; the obtained key ('createdKey') must be closed
    // by calling CloseKey
    virtual BOOL WINAPI CreateKey(HKEY key, LPCTSTR name, HKEY& createdKey) = 0;

    // opens the existing subkey 'name' of key 'key', returns 'openedKey' and
    // success; the obtained key ('openedKey') must be closed by calling CloseKey
    virtual BOOL WINAPI OpenKey(HKEY key, LPCTSTR name, HKEY& openedKey) = 0;

    // closes a key opened via OpenKey or CreateKey
    virtual void WINAPI CloseKey(HKEY key) = 0;

    // deletes the subkey 'name' of key 'key', returns success
    virtual BOOL WINAPI DeleteKey(HKEY key, LPCTSTR name) = 0;

    // loads the value 'name'+'type'+'buffer'+'bufferSize' from key 'key', returns
    // success
    virtual BOOL WINAPI GetValue(HKEY key, LPCTSTR name, DWORD type, void* buffer, DWORD bufferSize) = 0;

    // stores the value 'name'+'type'+'data'+'dataSize' in key 'key'; for strings
    // you can specify 'dataSize' == -1 -> length is calculated with strlen,
    // returns success
    virtual BOOL WINAPI SetValue(HKEY key, LPCTSTR name, DWORD type, const void* data, DWORD dataSize) = 0;

    // deletes the value 'name' of key 'key', returns success
    virtual BOOL WINAPI DeleteValue(HKEY key, LPCTSTR name) = 0;

    // retrieves the required size for the value 'name'+'type' from key 'key'
    // into 'bufferSize', returns success
    virtual BOOL WINAPI GetSize(HKEY key, LPCTSTR name, DWORD type, DWORD& bufferSize) = 0;
};
