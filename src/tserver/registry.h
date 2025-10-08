// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CRegArrayItem
//
// This must be the base of every class stored in the saved/loaded array
//

class CRegArrayItem
{
public:
    // lets the class write data into the buffer; bufferLen tells the class the maximum buffer size
    virtual BOOL TransferToRegistry(WCHAR* buffer, int bufferLen) = 0;
    // lets the class load itself from the buffer
    virtual BOOL TransferFromRegistry(const WCHAR* buffer) = 0;
};

//*****************************************************************************
//
// CReg
//

class CReg
{
public:
    BOOL SaveVoid(HKEY hKey, const WCHAR* name, DWORD type, const void* data, DWORD dataSize);
    BOOL LoadVoid(HKEY hKey, const WCHAR* name, DWORD type, void* data, DWORD dataSize, BOOL* notFound);
    BOOL GetSize(HKEY hKey, const WCHAR* name, DWORD type, DWORD& dataSize, BOOL* notFound);

    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data) = 0;
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL) = 0;
};

//*****************************************************************************
//
// CRegBOOL
//

class CRegBOOL : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegBOOL RegBOOL;

//*****************************************************************************
//
// CRegWORD
//

class CRegWORD : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegWORD RegWORD;

//*****************************************************************************
//
// CRegDWORD
//

class CRegDWORD : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegDWORD RegDWORD;

//*****************************************************************************
//
// CRegInt
//

class CRegInt : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegInt RegInt;

//*****************************************************************************
//
// CRegDouble
//

class CRegDouble : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegDouble RegDouble;

//*****************************************************************************
//
// CRegCOLORREF
//

class CRegCOLORREF : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegCOLORREF RegCOLORREF;

//*****************************************************************************
//
// CRegWindowPlacement
//

class CRegWindowPlacement : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegWindowPlacement RegWindowPlacement;

//*****************************************************************************
//
// CRegLogFont
//

class CRegLogFont : public CReg
{
public:
    virtual BOOL Save(HKEY hKey, const WCHAR* name, void* data);
    virtual BOOL Load(HKEY hKey, const WCHAR* name, void* data, BOOL* notFound = NULL);
};

extern CRegLogFont RegLogFont;

//*****************************************************************************
//
// CRegIndirectArray
//
/*
template <class DATA_TYPE>
class CRegIndirectArray : public CReg
{
  virtual BOOL Save(HKEY hKey, const WCHAR *name, void *data);
  virtual BOOL Load(HKEY hKey, const WCHAR *name, void *data, BOOL *notFound = NULL);

  virtual DATA_TYPE *Create() {return new DATA_TYPE;}
};
*/
//*****************************************************************************
//
// CRegistryPath
//

class CRegistryPath
{
protected:
    HKEY HRootKey; // handle of the key from which the path expands
    WCHAR* Path;   // path separated by backslashes
    HKEY HKey;     // handle of the key for the path

public:
    CRegistryPath(HKEY hRootKey, WCHAR* path);
    ~CRegistryPath();

    HKEY GetHRootKey() { return HRootKey; }
    HKEY GetHKey() { return HKey; }
    const WCHAR* GetPath() { return Path; }

    BOOL CreateKey();                    // creates the key and fills HKey
    BOOL OpenKey(BOOL* notFound = NULL); // opens the key and fills HKey
    BOOL CloseKey();                     // closes the key and clears HKey
};

//*****************************************************************************
//
// CRegistryClass
//

class CRegistryClass
{
protected:
    CReg* IOClass;               // pointer to the object handling data load/save
    const WCHAR* DataName;       // item name
    void* Data;                  // pointer to the data being read/stored
    CRegistryPath* RegistryPath; // pointer to the path for the data

public:
    CRegistryClass(CRegistryPath* registryPath, const WCHAR* dataName,
                   void* data, CReg* ioClass);

    BOOL SaveValue();               // saves data through IOClass
    BOOL LoadValue(BOOL& notFound); // loads data through IOClass
};

//*****************************************************************************
//
// CRegistry
//

typedef WORD HRegistryPath;

class CRegistry
{
protected:
    TIndirectArray<CRegistryPath> RegistryPaths;
    TIndirectArray<CRegistryClass> RegistryClasses;

    BOOL CreateKeys(); // creates keys
    BOOL OpenKeys();   // opens existing keys
    BOOL CloseKeys();  // closes the keys

public:
    CRegistry();

    // registers a path; fills the HRegistryPath handle
    // the path is provided as pointers to individual segments; the last pointer must be NULL
    BOOL RegisterPath(HRegistryPath& hRegistryPath, HKEY hRootKey, const WCHAR* path, ...);
    // registers data and their Save/Load object for a particular path
    BOOL RegisterClass(HRegistryPath hRegistryPath, const WCHAR* dataName,
                       void* dataClass, CReg* ioClass);

    BOOL Save(); // saves data
    BOOL Load(); // loads data
};

//*****************************************************************************
//
// Inlines
//

/*
template <class DATA_TYPE>
BOOL
CRegIndirectArray<DATA_TYPE>::Save(HKEY hKey, const WCHAR *name, void *data)
{
  WCHAR buff[1024];
  WCHAR realName[255];
  DWORD count = ((TIndirectArray<DATA_TYPE>*)data)->Count;
  DWORD i;
  for (i = 0; i < count; i++)
  {
    swprintf_s(realName, L"%s%d", name, i);
//    DATA_TYPE *item = ((TIndirectArray<DATA_TYPE>*)data)->At(i);  // someone generates this for int*
    CRegArrayItem *item = (CRegArrayItem *)((TIndirectArray<DATA_TYPE>*)data)->At(i);
    item->TransferToRegistry(buff, 1024);
    SaveVoid(hKey, realName, REG_SZ, buff, sizeof(WCHAR) * (wcslen(buff) + 1));
  }
  swprintf_s(realName, L"%s%d", name, i);
  RegDeleteValue(hKey, realName);      // remove the next element if it exists
  return TRUE;
}

template <class DATA_TYPE>
BOOL
CRegIndirectArray<DATA_TYPE>::Load(HKEY hKey, const WCHAR *name, void *data, BOOL *notFound)
{
  if (notFound != NULL) notFound = FALSE;
  WCHAR buff[1024];
  WCHAR realName[255];
  BOOL myNotFound;
  DWORD i = 0;
  ((TIndirectArray<DATA_TYPE>*)data)->DestroyMembers();
  while(1)
  {
    swprintf_s(realName, L"%s%d", name, i);
    if (!LoadVoid(hKey, realName, REG_SZ, buff, sizeof(buff), &myNotFound))
    {
      if (myNotFound)
        return TRUE;  // no more items
      else
        return FALSE;
    }
    CRegArrayItem *item = (CRegArrayItem *)Create();
    if (item == NULL) return FALSE;
    item->TransferFromRegistry(buff);
    ((TIndirectArray<DATA_TYPE>*)data)->Add((int*)item);
    if (!((TIndirectArray<DATA_TYPE>*)data)->IsGood()) return FALSE;
    i++;
  }
}
*/
