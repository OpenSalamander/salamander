// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// COutputInterface
//
// Because the output interface needs to be passed to parser objects, I choose
// this representation with an abstract interface in case the parsers
// live in an external DLL library.
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

#define OIF_SEPARATOR 0x00000001 // empty item
#define OIF_HEADER 0x00000002    // header
#define OIF_EMPHASIZE 0x00000004 // emphasize the property (for now only for OIF_HEADER)
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

    // returns the number of held items
    int GetCount();

    // returns an item
    const COutputItem* GetItem(int i);

    // releases all held items, leaving it in an empty state
    void DestroyItems();

    // methods from COutputInterface
    virtual BOOL AddItem(const char* name, const char* value);
    virtual BOOL AddHeader(const char* name, BOOL superHeader = FALSE);
    virtual BOOL AddSeparator();

    virtual BOOL PrepareForRender(HWND parentWnd);
};
