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
// POZOR: tato trida je pro jednoduchost jen prevzata z spl_base.h (lepe by bylo includit spl_base.h)
//
// ****************************************************************************
// CSalamanderRegistryAbstract
//
// sada metod Salamandera pro praci se systemovym registry,
// pouziva se v CPluginInterfaceAbstract::LoadConfiguration
// a CPluginInterfaceAbstract::SaveConfiguration

class CSalamanderRegistryAbstract
{
public:
    // vycisti klic 'key' od vsech podklicu a hodnot, vraci uspech
    virtual BOOL WINAPI ClearKey(HKEY key) = 0;

    // vytvori nebo otevre existujici podklic 'name' klice 'key', vraci 'createdKey' a uspech;
    // ziskany klic ('createdKey') je nutne zavrit volanim CloseKey
    virtual BOOL WINAPI CreateKey(HKEY key, LPCTSTR name, HKEY& createdKey) = 0;

    // otevre existujici podklic 'name' klice 'key', vraci 'openedKey' a uspech
    // ziskany klic ('openedKey') je nutne zavrit volanim CloseKey
    virtual BOOL WINAPI OpenKey(HKEY key, LPCTSTR name, HKEY& openedKey) = 0;

    // zavre klic otevreny pres OpenKey nebo CreateKey
    virtual void WINAPI CloseKey(HKEY key) = 0;

    // smaze podklic 'name' klice 'key', vraci uspech
    virtual BOOL WINAPI DeleteKey(HKEY key, LPCTSTR name) = 0;

    // nacte hodnotu 'name'+'type'+'buffer'+'bufferSize' z klice 'key', vraci uspech
    virtual BOOL WINAPI GetValue(HKEY key, LPCTSTR name, DWORD type, void* buffer, DWORD bufferSize) = 0;

    // ulozi hodnotu 'name'+'type'+'data'+'dataSize' do klice 'key', pro retezce je mozne
    // zadat 'dataSize' == -1 -> vypocet delky retezce pomoci funkce strlen,
    // vraci uspech
    virtual BOOL WINAPI SetValue(HKEY key, LPCTSTR name, DWORD type, const void* data, DWORD dataSize) = 0;

    // smaze hodnotu 'name' klice 'key', vraci uspech
    virtual BOOL WINAPI DeleteValue(HKEY key, LPCTSTR name) = 0;

    // vytahne do 'bufferSize' potrebnou velikost pro hodnotu 'name'+'type' z klice 'key', vraci uspech
    virtual BOOL WINAPI GetSize(HKEY key, LPCTSTR name, DWORD type, DWORD& bufferSize) = 0;
};
