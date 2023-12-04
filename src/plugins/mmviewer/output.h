// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// COutputInterface
//
// Protoze je treba iface pro output predavat do objektu parseru, volim
// tuto reprezentaci s abstraktnim rozhranim pro pripad, ze by parsery
// lezely v externi DLL knihovne.
//

class COutputInterface
{
public:
    virtual BOOL AddItem(const char* name, const char* value) = 0;
    virtual BOOL AddSeparator() = 0;
    virtual BOOL AddHeader(const char* name, BOOL superHeader = FALSE) = 0;

    virtual BOOL PrepareForRender(HWND parentWnd) = 0;
};

//****************************************************************************
//
// COutputInterface
//

#define OIF_SEPARATOR 0x00000001 // prazdna polozka
#define OIF_HEADER 0x00000002    // hlavicka
#define OIF_EMPHASIZE 0x00000004 // zesileni vlastnosti (zatim pouze pro OIF_HEADER)
#define OIF_UTF8 0x00000008      // The Value is encoded in UTF-8

struct COutputItem
{
    DWORD Flags;
    char* Name;
    char* Value;
    HWND hwnd; //edit box
};

class COutput : public COutputInterface
{
private:
    TDirectArray<COutputItem> Items;

public:
    COutput();
    ~COutput();

    // vrati pocet drzenych polozek
    int GetCount();

    // vrati polozku
    const COutputItem* GetItem(int i);

    // uvolni vsechny drzene polozky, zustane v prazdnem stavu
    void DestroyItems();

    // metody z COutputInterface
    virtual BOOL AddItem(const char* name, const char* value);
    virtual BOOL AddHeader(const char* name, BOOL superHeader = FALSE);
    virtual BOOL AddSeparator();

    virtual BOOL PrepareForRender(HWND parentWnd);
};
