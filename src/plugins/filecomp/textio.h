// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CTextFileReader
//

class CTextFileReader
{
public:
    // file type
    enum eFileType
    {
        ftUnknown,
        ftBinary,
        ftText
    };
    // text encoding
    // TODO pridat ruzne verze unicode formatu
    enum eEncoding
    {
        encUnknown,
        encASCII8,
        encUTF8,
        encUTF16,
        encUTF32
    };
    // endians
    enum eEndian
    {
        endianLittle,
        endianBig
    };

    // end of line conversions
    enum
    {
        eolCR = 1,   // convert '\r' to '\n'
        eolCRLF = 2, // convert '\r\n' to '\n'
        eolNULL = 4, // convert '\0' to '\n'
    };

    CTextFileReader();
    ~CTextFileReader();

    // 'size' nesmi byt vetsi nez numeric_limits<size_t>::max - 1
    void Set(const char* name, HANDLE file, size_t size, int eolConversions,
             eEncoding encoding, eEndian endians, int performASCII8InputEnc,
             const char* parASCII8InputEncTableName, BOOL normalizationForm, bool needMD5);
    void Reset();
    void FetchFilePrefix();
    bool BOMTest();
    bool IsValidUTF16(int endian);
    bool IsUTF16Text();
    bool IsUTF8Text();
    void EstimateFileType();
    int GetType();
    int GetEncoding();
    void GetEncodingName(char* encoding);
    void ForceType(eFileType type) { Type = type; }
    void ForceEncoding(eEncoding encoding, eEndian endian) { Encoding = encoding; }
    bool HasSurrogates()
    {
        // TODO nekdy to treba budem podporovat, zatim umime jen BMP
        return false;
    }

    const char* GetName() { return Name; }
    void Get(char*& buffer, size_t& size, const int& cancel);    // allokuje pres malloc
    void Get(wchar_t*& buffer, size_t& size, const int& cancel); // allokuje pres malloc
    bool HasMD5() { return MD5 != NULL; }
    void GetMD5(BYTE md5[16])
    {
        MD5->Finalize();
        MD5->GetDigest(md5);
    }

protected:
    class CByteReader
    {
    public:
        CByteReader(const char* name, HANDLE file, size_t size, char* buffer,
                    int bufferSize, int bufferedCharacters, const int& cancel,
                    CSalamanderMD5* md5);
        ~CByteReader();
        bool GetByte(unsigned char& byte) // return false on EOF
        {
            if (InPtr >= BufferedCharacters)
                return ReloadBuffer(byte);
            byte = Buffer[InPtr++];
            return true;
        }

    private:
        const char* Name;
        HANDLE File;
        char* Buffer;
        int BufferSize;
        int BufferedCharacters;
        int InPtr;
        size_t UnreadCharacters;
        const int& Cancel;
        CSalamanderMD5* MD5;

        bool ReloadBuffer(unsigned char& byte);
    };

    enum
    {
        BufferSize = 64 * 1024
    };
    const char* Name;
    HANDLE File;
    size_t Size;
    int EolConversions;
    char* Buffer;
    int BufferedCharacters;
    eFileType Type;
    eEncoding Encoding;
    eEndian Endian;
    int PerformASCII8InputEnc;
    char ASCII8InputEncTableName[101];
    BOOL NormalizationForm;
    CSalamanderMD5* MD5;

    template <class CChar>
    size_t ConvertEols(CChar* buffer, size_t size);
    void ReadASCII8(wchar_t*& buffer, size_t& size, const int& cancel);
    void ReadUTF8(wchar_t*& buffer, size_t& size, const int& cancel);
    void ReadUTF16(wchar_t*& buffer, size_t& size, const int& cancel);
    void ReadUTF32(wchar_t*& buffer, size_t& size, const int& cancel);
};
