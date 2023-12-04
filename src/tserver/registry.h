// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CRegArrayItem
//
// Toto musi byt predek vsech trid, ktere jsou drzeny v ukladanem / ctenem poli
//

class CRegArrayItem
{
public:
    // necha tridu nalejt do bufferu data; bufferLen rika tride maximalni velikost bufferu
    virtual BOOL TransferToRegistry(WCHAR* buffer, int bufferLen) = 0;
    // necha tridu nacist se z bufferu
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
    HKEY HRootKey; // handle klice, od ktereho se bude rozvijet cesta
    WCHAR* Path;   // cesta oddelena backslahema
    HKEY HKey;     // handle klice k ceste

public:
    CRegistryPath(HKEY hRootKey, WCHAR* path);
    ~CRegistryPath();

    HKEY GetHRootKey() { return HRootKey; }
    HKEY GetHKey() { return HKey; }
    const WCHAR* GetPath() { return Path; }

    BOOL CreateKey();                    // vytvori klic a naplni HKey
    BOOL OpenKey(BOOL* notFound = NULL); // otevre klic a naplni HKey
    BOOL CloseKey();                     // zavre klic a nuluje HKey
};

//*****************************************************************************
//
// CRegistryClass
//

class CRegistryClass
{
protected:
    CReg* IOClass;               // ukazatel na objekt zajistujici Load/Save dat
    const WCHAR* DataName;       // nazev polozky
    void* Data;                  // ukazatel na ctena/ukladana data
    CRegistryPath* RegistryPath; // ukazatel na cestu k datum

public:
    CRegistryClass(CRegistryPath* registryPath, const WCHAR* dataName,
                   void* data, CReg* ioClass);

    BOOL SaveValue();               // ulozi data pomoci IOClass
    BOOL LoadValue(BOOL& notFound); // nacte data pomoci IOClass
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

    BOOL CreateKeys(); // vytvori klice
    BOOL OpenKeys();   // otevre existujici klice
    BOOL CloseKeys();  // zavre klice

public:
    CRegistry();

    // registuje cestu; naplni handle HRegistryPath
    // cesta se zadava ukazatele na jednotlive dily cesty; posledni ukazatel musi byt NULL
    BOOL RegisterPath(HRegistryPath& hRegistryPath, HKEY hRootKey, const WCHAR* path, ...);
    // registruje pro urcitou cestu data a jejich Save/Load objekt
    BOOL RegisterClass(HRegistryPath hRegistryPath, const WCHAR* dataName,
                       void* dataClass, CReg* ioClass);

    BOOL Save(); // ulozi data
    BOOL Load(); // nacte data
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
//    DATA_TYPE *item = ((TIndirectArray<DATA_TYPE>*)data)->At(i);  // nekdo to generuje pro int*
    CRegArrayItem *item = (CRegArrayItem *)((TIndirectArray<DATA_TYPE>*)data)->At(i);
    item->TransferToRegistry(buff, 1024);
    SaveVoid(hKey, realName, REG_SZ, buff, sizeof(WCHAR) * (wcslen(buff) + 1));
  }
  swprintf_s(realName, L"%s%d", name, i);
  RegDeleteValue(hKey, realName);      // odmazu dalsi prvek, pokud existuje
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
        return TRUE;  // neni dalsi prvek
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
