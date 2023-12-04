// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

typedef enum eRPE_ERROR
{
    RPE_OK,
    RPE_INVALID_FORMAT,
    RPE_NOT_REG_FILE,
    RPE_ROOT_INVALID_KEY,
    RPE_OUT_OF_MEMORY,
    RPE_INVALID_KEY,
    RPE_KEY_OPEN,
    RPE_KEY_CREATE,
    RPE_VALUE_GET_SIZE,
    RPE_VALUE_MISSING_QUOTE,
    RPE_VALUE_MISSING_ASSIG,
    RPE_VALUE_INVALID_TYPE,
    RPE_VALUE_GET,
    RPE_VALUE_SET,
    RPE_VALUE_DWORD,
    RPE_VALUE_STRING,
    RPE_VALUE_HEX,
    RPE_INVALID_MBCS,
} eRPE_ERROR;

#ifdef UNICODE
#define CSalamanderRegistryExAbstract CSalamanderRegistryExAbstractW
#else
#define CSalamanderRegistryExAbstract CSalamanderRegistryExAbstractA
#endif

class CSalamanderRegistryExAbstract : public CSalamanderRegistryAbstract
{
public: // Methods added by Jan Patera
    // Returns subKeyIndex-th subkey name in buffer name
    virtual BOOL WINAPI EnumKey(HKEY key, DWORD subKeyIndex, LPTSTR name, DWORD bufferSize) = 0;

    // Returns valIndex-th value, value name in buffer name and value data in buffer data; valType+data+dataSize are optional
    virtual BOOL WINAPI EnumValue(HKEY key, DWORD valIndex, LPTSTR name, DWORD nameSize, LPDWORD valType, LPBYTE data, LPDWORD dataSize) = 0;

    // Remove keys and values with ".hidden" in name
    virtual void WINAPI RemoveHiddenKeysAndValues() = 0;

    // vycisti klic 'key' od vsech podklicu a hodnot; je-li 'doNotDeleteHiddenKeysAndValues' TRUE,
    // nesmazne klice a hodnoty, kterych jmena konci na ".hidden"; vraci uspech
    virtual BOOL WINAPI ClearKeyEx(HKEY key, BOOL doNotDeleteHiddenKeysAndValues, BOOL* keyIsNotEmpty) = 0;

    virtual void WINAPI Release() = 0;
    virtual BOOL WINAPI Dump(LPCTSTR fileName, LPCTSTR clearKeyName) = 0;
};

#ifdef UNICODE
#define REG_SysRegistryFactory REG_SysRegistryFactoryW
#define REG_MemRegistryFactory REG_MemRegistryFactoryW
#else
#define REG_SysRegistryFactory REG_SysRegistryFactoryA
#define REG_MemRegistryFactory REG_MemRegistryFactoryA
#endif

CSalamanderRegistryExAbstract* REG_SysRegistryFactory();
CSalamanderRegistryExAbstract* REG_MemRegistryFactory();

eRPE_ERROR Parse(LPTSTR buf, CSalamanderRegistryExAbstract* pRegistry, BOOL doNotDeleteHiddenKeysAndValues);
eRPE_ERROR CopyBranch(LPCTSTR branch, CSalamanderRegistryExAbstract* pInRegistry, CSalamanderRegistryExAbstract* pOutRegistry);

DWORD ConvertIfNeeded(LPTSTR* pBuf, DWORD size);

#define SizeOf(x) (sizeof(x) / sizeof(x[0]))
