// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// size of file read buffer
#define BUFSIZE 0x8000 // buffer bude 32KB

class CDecompressFile
{
public:
    // detekuje typ archivu a vrati spravne zinicializovany objekt
    static CDecompressFile* CreateInstance(LPCTSTR filename, DWORD offset, CQuadWord inputSize);

    // konstruktor a destruktor
    CDecompressFile(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord size);
    virtual ~CDecompressFile();

    virtual BOOL IsCompressed() { return FALSE; }
    virtual BOOL BuggySize() { return FALSE; }

    // vraci, v jakem jsme stavu
    BOOL IsOk() const { return Ok; }
    // pokud nastala chyba, vraci jeji kod
    unsigned int GetErrorCode() const { return ErrorCode; }
    // pokud nastala systemova chyba (I/O etc.) vraci blizsi urceni (::GetLastError())
    DWORD GetLastErr() const { return LastError; }
    // vraci velikost bufferu s neprectenymi datami
    size_t GetUnreadInputBufferSize() const { return min(size_t(DataEnd - DataStart), (InputSize - InputPos).LoDWord); }
    // vraci velikost archivu na disku
    CQuadWord GetStreamSize() const { return InputSize; }
    // vraci soucasnou pozici v aktualnim souboru v archivu na disku
    CQuadWord GetInputPos() const { return InputPos; }
    // vraci soucasnou pozici v archivu na disku
    CQuadWord GetStreamPos() const { return StreamPos; }
    // vrati puvodni nazev souboru, ktery byl v archivu (nazev taru v gzipu apod.)
    const char* GetOldName();
    // vrati nazev souboru, se kterym pracuje (jmeno archivu)
    const char* GetArchiveName() const { return FileName; }

    // vrati cast nebo cely posledni precteny blok k dalsimu pouziti
    virtual void Rewind(unsigned short size);

    virtual const unsigned char* GetBlock(unsigned short size, unsigned short* read = NULL);
    virtual void GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr);

protected:
    // cte blok ze souboru
    const unsigned char* FReadBlock(unsigned int number);
    // cte byte ze souboru
    unsigned char FReadByte();
    // nastavi puvodni jmeno souboru (pokud bylo v archivu ulozeno)
    void SetOldName(char* oldName);

    BOOL FreeBufAndFile;      // TRUE = prevzali jsme soubor a buffer, uvolnime je v destruktoru
    BOOL Ok;                  // stav
    unsigned int ErrorCode;   // pokud nastala chyba, tady je uvedeno, jaka
    const char* FileName;     // nazev archivu, nad kterym pracujem
    char* OldName;            // puvodni nazev souboru pred zapakovanim
    CQuadWord InputSize;      // velikost archivu (parcialni)
    CQuadWord InputPos;       // pozice v archivu (parcialni)
    CQuadWord StreamPos;      // pozice v archivu (pro progress)
    HANDLE File;              // otevreny archiv
    DWORD LastError;          // pokud byla chyba systemu (I/O...), tady je blizsi urceni
    unsigned char* Buffer;    // vyrovnavaci buffer pro cteni ze souboru
    unsigned char* DataStart; // zacatek nepouzitych dat v bufferu
    unsigned char* DataEnd;   // konec nepouzitych dat
};

class CZippedFile : public CDecompressFile
{
public:
    // konstruktor a destruktor
    CZippedFile(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    virtual ~CZippedFile();

    virtual BOOL IsCompressed() { return TRUE; }

    virtual void GetFileInfo(FILETIME& lastWrite, CQuadWord& fileSize, DWORD& fileAttr);
    virtual const unsigned char* GetBlock(unsigned short size, unsigned short* read);
    virtual void Rewind(unsigned short size);

protected:
    unsigned char* Window;    // output circular buffer
    unsigned char* ExtrStart; // start of unread data in circular buffer
    unsigned char* ExtrEnd;   // end of extracted data in circular buffer

    virtual BOOL DecompressBlock(unsigned short needed) = 0;
    virtual BOOL CompactBuffer();
};
