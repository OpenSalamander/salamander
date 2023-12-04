// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

using namespace std;

// ****************************************************************************
//
// CTextFileReader::CByteReader
//

CTextFileReader::CByteReader::CByteReader(const char* name, HANDLE file,
                                          size_t size, char* buffer, int bufferSize, int bufferedCharacters, const int& cancel, CSalamanderMD5* md5) : Name(name), File(file), UnreadCharacters(size - size_t(bufferedCharacters)),
                                                                                                                                                       Buffer(buffer), BufferSize(bufferSize), BufferedCharacters(bufferedCharacters),
                                                                                                                                                       InPtr(0), Cancel(cancel), MD5(md5)
{
}

CTextFileReader::CByteReader::~CByteReader()
{
    if (Buffer)
        free(Buffer);
}

bool CTextFileReader::CByteReader::ReloadBuffer(unsigned char& byte)
{
    if (UnreadCharacters == 0)
        return false;
    if (Cancel)
        throw CFilecompWorker::CAbortByUserException();
    if (!Buffer) // allocate buffer
    {
        Buffer = (char*)malloc(BufferSize);
        if (!Buffer)
            CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);
    }

    DWORD toRead = DWORD(min(size_t(BufferSize), UnreadCharacters));
    DWORD read;
    if (!ReadFile(File, Buffer, toRead, &read, NULL) ||
        toRead != read)
        CFilecompWorker::CException::Raise(IDS_READFILE, GetLastError(), Name);
    if (MD5)
        MD5->Update(Buffer, toRead);

    UnreadCharacters -= toRead;
    InPtr = 0;
    BufferedCharacters = toRead;

    byte = Buffer[InPtr++];
    return true;
}

// ****************************************************************************
//
// CTextFileReader
//

CTextFileReader::CTextFileReader()
{
    File = INVALID_HANDLE_VALUE;
    Buffer = NULL;
    Type = ftUnknown;
    Encoding = encUnknown;
    BufferedCharacters = 0;
    MD5 = NULL;
}

CTextFileReader::~CTextFileReader()
{
    Reset();
}

void CTextFileReader::Set(const char* name, HANDLE file, size_t size, int eolConversions, eEncoding encoding, eEndian endians, int performASCII8InputEnc, const char* parASCII8InputEncTableName, BOOL normalizationForm, bool needMD5)
{
    Name = name;
    File = file;
    Size = size;
    EolConversions = eolConversions;
    Encoding = encoding;
    Endian = endians;
    if (encUTF8 == Encoding)
    {
        // Patera 2009.04.16: Sanity check needed when comparing first as UTFxyBE and then UTF8
        Endian = endianLittle;
    }
    PerformASCII8InputEnc = performASCII8InputEnc;
    strcpy(this->ASCII8InputEncTableName, parASCII8InputEncTableName);
    NormalizationForm = normalizationForm;
    if (MD5)
    {
        SG->FreeSalamanderMD5(MD5);
        MD5 = NULL;
    }
    if (needMD5)
        MD5 = SG->AllocSalamanderMD5();
}

void CTextFileReader::Reset()
{
    if (Buffer)
        free(Buffer);
    Buffer = NULL;
    BufferedCharacters = NULL;
    if (MD5)
    {
        SG->FreeSalamanderMD5(MD5);
        MD5 = NULL;
    }
}

void CTextFileReader::FetchFilePrefix()
{
    if (!Buffer) // allocate buffer and read prefix of the file
    {
        Buffer = (char*)malloc(BufferSize);
        if (!Buffer)
            CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);

        if (SetFilePointer(File, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
            CFilecompWorker::CException::Raise(IDS_ACCESFILE, GetLastError(), Name);

        DWORD toRead = DWORD(min(size_t(BufferSize), Size));
        DWORD read;
        if (!ReadFile(File, Buffer, toRead, &read, NULL) ||
            toRead != read)
            CFilecompWorker::CException::Raise(IDS_READFILE, GetLastError(), Name);
        if (MD5)
            MD5->Update(Buffer, toRead);
        BufferedCharacters = toRead;
    }
}

bool CTextFileReader::BOMTest()
{
    eEncoding encoding = Encoding;
    if (BufferedCharacters > 1)
    {
        // Byte-Order Mark test (http://www.cl.cam.ac.uk/~mgk25/unicode.html)
        // UTF-16LE BOM: 0xFF 0xFE
        // UTF-16BE BOM: 0xFE 0xFF
        // UTF-32LE BOM: 0xFF 0xFE 0x00 0x00
        // UTF-32BE BOM: 0x00 0x00 0xFE 0xFF
        // UTF-8 BOM: 0xEF 0xBB 0xBF
        unsigned char b0 = Buffer[0];
        unsigned char b1 = Buffer[1];
        unsigned char b2 = Buffer[2];
        unsigned char b3 = Buffer[3];
        if (b0 == 0xFF && b1 == 0xFE)
        {
            if (BufferedCharacters > 3 && b2 == 0 && b3 == 0)
            {
                Type = ftText;
                Encoding = encUTF32;
                Endian = endianLittle;
            }
            else
            {
                Type = ftText;
                Encoding = encUTF16;
                Endian = endianLittle;
            }
        }
        elif (b0 == 0xFE && b1 == 0xFF)
        {
            Type = ftText;
            Encoding = encUTF16;
            Endian = endianBig;
        }
        elif (BufferedCharacters > 3 && b0 == 0 && b1 == 0 && b2 == 0xFE && b3 == 0xFF)
        {
            Type = ftText;
            Encoding = encUTF32;
            Endian = endianBig;
        }
        elif (BufferedCharacters > 2 && b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
        {
            Type = ftText;
            Encoding = encUTF8;
        }
    }
    return (encoding == encUnknown) && (Encoding != encUnknown);
}

bool CTextFileReader::IsValidUTF16(int endian)
{
    LPBYTE buffer = LPBYTE(Buffer); // unsigned alias
    bool expectHiSurrogate = false;
    bool b0D, b0A, b0A0D, b0D0A, b0D0D, b0A0A;

    b0D = b0A = b0A0D = b0D0A = b0D0D = b0A0A = false;
    int i;
    for (i = 0; i < BufferedCharacters - 1;)
    {
        unsigned short c;
        if (endian == endianLittle)
        {
            c = (unsigned short)buffer[i++];
            c |= ((unsigned short)buffer[i++]) << 8;
        }
        else
        {
            c = ((unsigned short)buffer[i++]) << 8;
            c |= (unsigned short)buffer[i++];
        }
        // U+FFFE and U+FFFF must not occur in normal UCS-4 data.
        // (see http://www.cl.cam.ac.uk/~mgk25/unicode.html)
        //
        // Additionaly, we do not expect zero characters to appear in text files.
        //
        if (c == 0 || c == 0xFFFE || c == 0xFFFF)
        {
            TRACE_I("This file cannot be UTF-16" << (endian == endianLittle ? "" : "BE") << ", it contains " << std::hex << c << " at adress " << (i - 2) << std::dec);
            return false;
        }
        if (expectHiSurrogate)
        {
            if (c < 0xDC00 || c > 0xDFFF)
            {
                TRACE_I("This file cannot be UTF-16" << (endian == endianLittle ? "" : "BE") << ", missing high-surrogate at address " << std::hex << (i - 2) << std::dec);
                return false;
            }
            expectHiSurrogate = false;
        }
        else
        {
            if (c >= 0xD800 && c <= 0xDBFF)
                expectHiSurrogate = true;
            // Check for end of line codes
            if (0x0A == c)
                b0A = true;
            if (0x0D == c)
                b0D = true;
            if (0x0A0A == c)
                b0A0A = true; // Gurmukhi block
            if (0x0A0D == c)
                b0A0D = true; // Gurmukhi block
            if (0x0D0A == c)
                b0D0A = true; // Malayalam block
            if (0x0D0D == c)
                b0D0D = true; // Malayalam block
        }
    }
    if ((b0A0D || b0D0A || b0D0D || b0A0A) && !b0D & !b0A)
        return false;
    return true;
}

bool CTextFileReader::IsUTF16Text()
{
    // do we have enough data for statistical modeling?
    if (BufferedCharacters < 5000)
        return false;

    int histOdd[256];
    int histEven[256];

    // model probability density of odd and even bytes by 256-bin histograms
    fill_n(histOdd, 256, 0);
    fill_n(histEven, 256, 0);
    int i = 0;
    for (; i < BufferedCharacters - 1;)
    {
        histEven[BYTE(Buffer[i++])]++;
        histOdd[BYTE(Buffer[i++])]++;
    }

    // calculate Bhattacharyya coefficient, 0 means that distributions are
    // orthogonal, 1 means that distributions are equal
    double bc = 0;
    double norm = 1 / double(i / 2); // normalization to get proper distributions
    for (i = 0; i < 256; ++i)
        bc += sqrt(double(histOdd[i]) * double(histEven[i]));
    bc *= norm;

    TRACE_I("Bhattacharyya coefficient of " << Name << " is " << bc);

    // This filters out 99% of 130K files on my computer and 99% of 180K files on
    // our school terminal server. The remaining 1% are either UTF-16 files or
    // binary files (false detections). The binary files get filtered out later,
    // in the next stage of this detector.
    if (bc > 0.4)
    {
        // this can't be a unicode file
        return false;
    }

    bool isValidUTF16 = IsValidUTF16(endianLittle);
    bool isValidUTF16BE = IsValidUTF16(endianBig);
    TRACE_I("Is valid UTF-16. " << isValidUTF16);
    TRACE_I("Is valid UTF-16BE. " << isValidUTF16BE);
    if (isValidUTF16 && isValidUTF16BE)
    {
        // determine endians by comparing entropy in odd and even bytes, the high
        // order bytes should have lower entropy than low order bytes
        double oe = 0, ee = 0;
        double p;
        // we normalize probabilities to have nonzero values by adding 1/256 to
        // each bin, to do this we must also change bin normalization constant to
        // keep sum over all bins equal to zero
        norm = 1 / (1 / norm + 1);
        for (i = 0; i < 256; ++i)
        {
            p = (histOdd[i] + 1. / 256.) * norm;
            oe -= p * log(p);
            p = (histEven[i] + 1. / 256.) * norm;
            ee -= p * log(p);
        }
        TRACE_I("Even bytes entropy of " << Name << " is " << ee);
        TRACE_I("Odd  bytes entropy of " << Name << " is " << oe);
        Type = ftText;
        Encoding = encUTF16;
        Endian = ee > oe ? endianLittle : endianBig;
    }
    elif (isValidUTF16)
    {
        Type = ftText;
        Encoding = encUTF16;
        Endian = endianLittle;
    }
    elif (isValidUTF16BE)
    {
        Type = ftText;
        Encoding = encUTF16;
        Endian = endianBig;
    }

    return Encoding != encUnknown;
}

bool CTextFileReader::IsUTF8Text()
{
    // TODO Use file prefix stored in `Buffer' to test whether the data are UTF-8
    // text file. `BufferedCharacters' tells ho many bytes is stored in `Buffer'.

    // do we have enough data for reliable reasoning?
    // TODO adjust the threshold as needed
    if ((BufferedCharacters < 2500) && ((size_t)BufferedCharacters < Size))
        return false;

    int cnt = BufferedCharacters;
    int nUTF8 = 0;
    char* s = Buffer;

    while (cnt-- > 0)
    {
        if (*s & 0x80)
        {
            if ((*s & 0xe0) == 0xc0)
            {
                if (!cnt || !s[1])
                {
                    if (cnt)
                    { // incomplete 2-byte sequence
                        nUTF8 = 0;
                    }
                    break;
                }
                else if ((s[1] & 0xc0) != 0x80)
                {
                    nUTF8 = 0;
                    break; // not in UCS2
                }
                else
                {
                    nUTF8++;
                    s += 2;
                    cnt--;
                }
            }
            else if ((*s & 0xf0) == 0xe0)
            {
                if ((cnt < 2) || !s[1] || !s[2])
                {
                    if (cnt > 1)
                    { // incomplete 3-byte sequence
                        nUTF8 = 0;
                    }
                    break;
                }
                else if ((s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80)
                {
                    nUTF8 = 0;
                    break; // not in UCS2
                }
                else
                {
                    nUTF8++;
                    s += 3;
                    cnt -= 2;
                }
            }
            else
            {
                nUTF8 = 0;
                break; // not in UCS2
            }
        }
        else
        {
            s++;
        }
    }

    if (nUTF8 > 0)
    { // At least one 2-or-more-bytes UTF8 sequence found and no invalid sequences found
        Type = ftText;
        Encoding = encUTF8;
    }
    return Encoding != encUnknown;
}

void CTextFileReader::EstimateFileType()
{
    // this should never happen, but to be sure...
    if (Type == ftBinary)
        return;

    // in case of unknown file type or know file type but unknown encoding we do
    // a dest for a Unicode
    if (Type == ftUnknown || Encoding == encUnknown)
    {
        // ensure we have file prefix in the buffer
        FetchFilePrefix();
        eEncoding encoding = Encoding;
        eEndian endian = Endian;

        // do the tests
        BOMTest() || IsUTF16Text() || IsUTF8Text();
        if (encoding != encUnknown)
        { // Keep previous values
            Encoding = encoding;
            Endian = endian;
        }
    }

    // if the test above did not recognize Unicode text file or we already know
    // the file is text with know encoding but unknown input-enc table, we use
    // RecognizeFileType from SS API
    if (Type == ftUnknown ||      // unknown type
        Encoding == encUnknown || // text file, but unknown encoding
        // text file, known encoding, but unknown input-enc table
        (Encoding == encASCII8 && PerformASCII8InputEnc == 1 &&
         ASCII8InputEncTableName[0] == 0))
    {
        FetchFilePrefix(); // ensure we have file prefix in the buffer
        char cp[101];
        BOOL isText;
        SG->RecognizeFileType(NULL, Buffer, BufferedCharacters, Type == ftText, &isText, cp);
        if (Type == ftText || isText)
        {
            Type = ftText;
            if ((Encoding == encUnknown) || ((Encoding == encASCII8) && PerformASCII8InputEnc && !ASCII8InputEncTableName[0]))
            {
                Encoding = encASCII8;
                // estimated conversion
                char wincp[101];
                SG->GetWindowsCodePage(NULL, wincp);
                if (wincp[0] && cp[0] && strlen(cp) + strlen(wincp) + 4 <= 101 && SG->StrICmp(cp, wincp) != 0)
                {
                    strcpy(ASCII8InputEncTableName, cp);
                    strcat(ASCII8InputEncTableName, " - ");
                    strcat(ASCII8InputEncTableName, wincp);
                    PerformASCII8InputEnc = 1;
                }
                else
                    PerformASCII8InputEnc = 0;
            }
        }
        else
            Type = ftBinary;
    }

    TRACE_I("File type " << Type);
    TRACE_I("File encoding " << Encoding);
    TRACE_I("File endians " << Endian);
    TRACE_I("PerformASCII8InputEnc " << PerformASCII8InputEnc);
    TRACE_I("ASCII8InputEncTableName " << ASCII8InputEncTableName);
}

int CTextFileReader::GetType()
{
    if (Type == ftUnknown)
        EstimateFileType();
    return Type;
}

int CTextFileReader::GetEncoding()
{
    EstimateFileType(); // ensure we know everything
    return Encoding;
}

void CTextFileReader::GetEncodingName(char* encoding)
{
    encoding[0] = 0;
    if (Encoding == encASCII8)
    {
        if (PerformASCII8InputEnc && *ASCII8InputEncTableName)
            _snprintf_s(encoding, 100, _TRUNCATE, "[%s] ", ASCII8InputEncTableName);
    }
    else
    {
        switch (Encoding)
        {
        case encUTF8:
            strcpy(encoding, "[UTF-8");
            break;
        case encUTF16:
            strcpy(encoding, "[UTF-16");
            break;
        case encUTF32:
            strcpy(encoding, "[UTF-32");
            break;
        }
        if (Endian == endianBig)
            strcat(encoding, "BE");
        strcat(encoding, "] ");
    }
}

void CTextFileReader::Get(char*& buffer, size_t& size, const int& cancel)
{
    // TODO what if the file has zero length?

    if (Buffer == NULL || BufferSize < Size + 1)
    {
        // reserve 1 char more at the end, for possible new-line insertion
        char* ret = (char*)realloc(Buffer, Size + 1);
        if (!ret)
            CFilecompWorker::CException::Raise(IDS_LOWMEM_TRY_BINARY, 0);
        Buffer = ret;
    }

    if (SetFilePointer(File, BufferedCharacters, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        CFilecompWorker::CException::Raise(IDS_ACCESFILE, GetLastError(), Name);

    // read rest of the file
    size_t bytesLeft = Size - BufferedCharacters;
    char* writePtr = Buffer + BufferedCharacters;
    while (bytesLeft)
    {
        DWORD toRead = DWORD(min(size_t(512 * 1024), bytesLeft));
        DWORD read;
        if (!ReadFile(File, writePtr, toRead, &read, NULL) ||
            toRead != read)
            CFilecompWorker::CException::Raise(IDS_READFILE, GetLastError(), Name);
        if (MD5)
            MD5->Update(writePtr, toRead);
        bytesLeft -= toRead;
        writePtr += toRead;
        if (cancel)
            throw CFilecompWorker::CAbortByUserException();
    }

    if (PerformASCII8InputEnc)
    {
        char conv[256];
        if (SG->GetConversionTable(NULL, conv, ASCII8InputEncTableName))
        {
            size_t i;
            for (i = 0; i < Size; ++i)
                Buffer[i] = conv[(unsigned char)Buffer[i]];
        }
    }

    size = ConvertEols(Buffer, size_t(Size));
    // add virtual line at the end of the file
    Buffer[size++] = '\n'; // text vzdy konci na '\n'

    buffer = Buffer;
    Buffer = NULL; // uze je prirazeny volajicimu
    BufferedCharacters = 0;
}

void CTextFileReader::Get(wchar_t*& buffer, size_t& size, const int& cancel)
{
    // TODO what if the file has zero length?

    if (Encoding == encUnknown)
        EstimateFileType();

    if (SetFilePointer(File, BufferedCharacters, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        CFilecompWorker::CException::Raise(IDS_ACCESFILE, GetLastError(), Name);

    buffer = NULL;
    try
    {
        // TODO nezapominat skipnout BOM mark
        switch (Encoding)
        {
        case encASCII8:
            ReadASCII8(buffer, size, cancel);
            break;
        case encUTF8:
            ReadUTF8(buffer, size, cancel);
            break;
        case encUTF16:
            ReadUTF16(buffer, size, cancel);
            break;
        case encUTF32:
            ReadUTF32(buffer, size, cancel);
            break;
        default:
            TRACE_E("This should never happen. Unknown input encoding.");
            throw CFilecompWorker::CException(
                "This should never happened. Unknown input encoding.");
        }

        size = ConvertEols(buffer, size);
        // add virtual line at the end of the file
        buffer[size++] = '\n'; // text vzdy konci na '\n'
        if (NormalizationForm)
        {
            int len = PNormalizeString(NormalizationC, buffer, (int)size, NULL, 0);
            if (len > 0)
            {
                LPWSTR buffer2 = (LPWSTR)malloc(len * sizeof(WCHAR));
                if (buffer2)
                {
                    if ((len = PNormalizeString(NormalizationC, buffer, (int)size, buffer2, len)) > 0)
                    {
                        free(buffer);
                        buffer = (LPWSTR)realloc(buffer2, len * sizeof(WCHAR));
                        if (!buffer)
                            buffer = buffer2;
                        size = len;
                    }
                    else
                    {
                        free(buffer2);
                    }
                }
            }
        }
    }
    catch (exception e)
    {
        if (buffer)
            free(buffer); // release memory
        buffer = NULL;
        throw; // let it go...
    }

    // TODO
    //
    // In addition to the encoding alternatives, Unicode also specifies various
    // Normalization Forms, which provide reasonable subsets of Unicode,
    // especially to ***REMOVE ENCODING AMBIGUITIES*** caused by the presence of
    // precomposed and compatibility characters:
    //
    //   http://www.cl.cam.ac.uk/~mgk25/unicode.html
    //   http://www.unicode.org/unicode/reports/tr15/
    //
    // see WINAPI FoldString
    //
    // this has to be optional 'Ignore difference caused by unicode encoding
    // ambiguilties', test for surrogates afterwards
}

template <class CChar>
size_t
CTextFileReader::ConvertEols(CChar* buffer, size_t size)
{
    if (!EolConversions)
        return size;

    bool cr = (EolConversions & eolCR) != 0;
    bool crlf = (EolConversions & eolCRLF) != 0;
    bool null = (EolConversions & eolNULL) != 0;
    bool crconv = cr | crlf;

    CChar* src = buffer;
    CChar* dst = buffer;
    for (; src < buffer + size; ++src, ++dst)
    {
        if (*src == '\r' && crconv)
        {
            if (crlf && src + 1 < buffer + size && src[1] == '\n')
                src++;
            *dst = '\n';
        }
        elif (*src == 0 && null)
        {
            *dst = '\n';
        }
        else
        {
            if (dst < src)
                *dst = *src;
        }
    }
    return dst - buffer;
}

void CTextFileReader::ReadASCII8(wchar_t*& buffer, size_t& size, const int& cancel)
{
    if (Buffer == NULL || BufferSize < Size)
    {
        char* ret = (char*)realloc(Buffer, Size);
        if (!ret)
            CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);
        Buffer = ret;
    }

    // read rest of the file
    size_t bytesLeft = Size - BufferedCharacters;
    char* writePtr = Buffer + BufferedCharacters;
    while (bytesLeft)
    {
        DWORD toRead = DWORD(min(size_t(512 * 1024), bytesLeft));
        DWORD read;
        if (!ReadFile(File, writePtr, toRead, &read, NULL) ||
            toRead != read)
            CFilecompWorker::CException::Raise(IDS_READFILE, GetLastError(), Name);
        if (MD5)
            MD5->Update(writePtr, toRead);
        bytesLeft -= toRead;
        writePtr += toRead;
        if (cancel)
            throw CFilecompWorker::CAbortByUserException();
    }

    // inputenc
    if (PerformASCII8InputEnc)
    {
        char conv[256];
        if (SG->GetConversionTable(NULL, conv, ASCII8InputEncTableName))
        {
            size_t i;
            for (i = 0; i < Size; ++i)
                Buffer[i] = conv[(unsigned char)Buffer[i]];
        }
    }

    // TODO what if the file has zero length? MultiByteToWideChar fails?

    int ret = 0;
    if (Size > 0)
    {
        // estimage length
        ret = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, Buffer, int(Size), NULL, 0);
        if (ret == 0)
            CFilecompWorker::CException::Raise(IDS_ERRORUNICODE, GetLastError(), Name);
    }

    // test
    if (cancel)
        throw CFilecompWorker::CAbortByUserException();

    // reserve 1 char more at the end, for possible new-line insertion
    size = ret;
    buffer = (wchar_t*)malloc((size + 1) * sizeof(wchar_t));
    if (!buffer)
        CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);

    if (size > 0)
    {
        // do the conversion
        ret = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, Buffer, int(Size), buffer, int(size));
        if (ret == 0)
            CFilecompWorker::CException::Raise(IDS_ERRORUNICODE, GetLastError(), Name);
    }

    // release further needless memory
    free(Buffer);
    Buffer = NULL;
    BufferedCharacters = NULL;
}

void CTextFileReader::ReadUTF8(wchar_t*& buffer, size_t& size, const int& cancel)
{
    CByteReader input(Name, File, Size, Buffer, BufferSize, BufferedCharacters, cancel, MD5);

    Buffer = NULL; // o uvolneni se stara input
    BufferedCharacters = 0;

    size_t allocated = Size; // worst case estimate (each UTF-8 byte codes one UCS-4 char)
    // reserve 1 char more at the end, for possible new-line insertion
    buffer = (wchar_t*)malloc((allocated + 1) * sizeof(wchar_t));
    if (!buffer)
        CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);
    size = 0;

    DWORD utf32;
    unsigned char z, y, x, w, v, u;
    while (input.GetByte(z))
    {
        // R.5 Mapping from UTF-8 form to UCS-4 form
        // (see http://www.cl.cam.ac.uk/~mgk25/ucs/ISO-10646-UTF-8.html)
        //
        // Sequence of			Four-octet
        // octets in UTF-8			sequences in UCS-4
        //
        // z = 00 .. 7F;			z;
        // z = C0 .. DF; y;			(z-C0)*2**6 + (y-80);
        // z = E0 .. EF; y; x;		(z-E0)*2**12 + (y-80)*2**6 + (x-80);
        // z = F0 .. F7; y; x; w;		(z-F0)*2**18 + (y-80)*2**12 +(x-80)*2**6 + (w-80);
        // z = F8 ..FB; y; x; w; v;		(z-F8)*2**24 + (y-80)*2**18 +(x-80)*2**12 + (w-80)*2**6 + (v - 80);
        // z = FC, FD; y; x; w; v; u;	(z-FC)*2**30 + (y-80)*2**24 +(x-80)*2**18 + (w-80)*2**12 + (v-80)*2**6 + (u-80);
        //
        // The following byte sequences are used to represent a character. The
        // sequence to be used depends on the Unicode number of the character (see
        // http://www.cl.cam.ac.uk/~mgk25/unicode.html#utf-8):
        //
        // U-00000000 – U-0000007F: 	0xxxxxxx
        // U-00000080 – U-000007FF: 	110xxxxx 10xxxxxx
        // U-00000800 – U-0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
        // U-00010000 – U-001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        // U-00200000 – U-03FFFFFF: 	111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        // U-04000000 – U-7FFFFFFF: 	1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

        // we do not control if the code is correct, if the input has incorrect
        // code, the resulting mapping is undefined
        //
        // we use masking by 0x3f instead of subtracking 0x80 to protect from
        // invalid values in input coding
        if (z <= 0x7F)
            utf32 = z;
        elif (z <= 0xDF)
        {
            if (!input.GetByte(y))
                break; // incomplete code
            utf32 = ((z - 0xC0) << 6) + (y & 0x3f);
        }
        elif (z <= 0xEF)
        {
            if (!input.GetByte(y))
                break; // incomplete code
            if (!input.GetByte(x))
                break; // incomplete code
            utf32 = ((z - 0xE0) << 12) + ((y & 0x3f) << 6) + (x & 0x3f);
        }
        elif (z <= 0xF7)
        {
            if (!input.GetByte(y))
                break; // incomplete code
            if (!input.GetByte(x))
                break; // incomplete code
            if (!input.GetByte(w))
                break; // incomplete code
            utf32 = ((z - 0xF0) << 18) + ((y & 0x3f) << 12) + ((x & 0x3f) << 6) + (w & 0x3f);
        }
        elif (z <= 0xFB)
        {
            if (!input.GetByte(y))
                break; // incomplete code
            if (!input.GetByte(x))
                break; // incomplete code
            if (!input.GetByte(w))
                break; // incomplete code
            if (!input.GetByte(v))
                break; // incomplete code
            utf32 = ((z - 0xF8) << 24) + ((y & 0x3f) << 18) + ((x & 0x3f) << 12) + ((w & 0x3f) << 6) + (v & 0x3f);
        }
        else
        {
            if (!input.GetByte(y))
                break; // incomplete code
            if (!input.GetByte(x))
                break; // incomplete code
            if (!input.GetByte(w))
                break; // incomplete code
            if (!input.GetByte(v))
                break; // incomplete code
            if (!input.GetByte(u))
                break; // incomplete code
            // mask it instead of subtracting 0xFC, to protect from invalid values in
            // input coding
            z &= 1;
            utf32 = (z << 30) + ((y & 0x3f) << 24) + ((x & 0x3f) << 18) + ((w & 0x3f) << 12) + ((v & 0x3f) << 6) + (u & 0x3f);
        }

        // Q.3 Mapping from UCS-4 form to UTF-16 form
        // (see http://www.cl.cam.ac.uk/~mgk25/ucs/ISO-10646-UTF-16.html)
        //
        // UCS-4 (4-octet)				UTF-16, 2-octet elements
        // -----------------------------------------------------------------------------------
        //   x = 0000 0000 .. 0000 FFFF (see Note 1)	  x % 0001 0000;
        // -----------------------------------------------------------------------------------
        //   x = 0001 0000 ..0010 FFFF			  y; z;
        //  						where
        //  						  y = ((x - 0001 0000) / 400) + D800
        //    						  z = ((x - 0001 0000) % 400) + DC00
        // -----------------------------------------------------------------------------------
        //   x 0011 0000 ..7FFF FFFF			(no mapping is defined)

        int convertToSurrogates = utf32 > 0xFFFF ? 1 : 0;

        // need more memory to store the file
        if (size + convertToSurrogates >= allocated)
        {
            allocated *= 2;
            // reserve 1 char more at the end, for possible new-line insertion
            wchar_t* ret = (wchar_t*)realloc(buffer, (allocated + 1) * sizeof(wchar_t));
            if (!ret)
                CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);
            buffer = ret;
        }

        if (convertToSurrogates)
        {
            buffer[size++] = wchar_t((((utf32 - 0x00010000) >> 10) + 0xD800) & 0xffff);
            buffer[size++] = wchar_t((((utf32 - 0x00010000) & 0x3FF) + 0xDC00) & 0xffff);
        }
        else
        {
            buffer[size++] = wchar_t(utf32);
        }
    }
}

void CTextFileReader::ReadUTF16(wchar_t*& buffer, size_t& size, const int& cancel)
{
    _ASSERT(sizeof(wchar_t) == 2);

    // read at once
    if (Buffer == NULL || BufferSize < Size + sizeof(wchar_t))
    {
        char* ret = (char*)realloc(Buffer, Size + sizeof(wchar_t));
        if (!ret)
            CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);
        Buffer = ret;
    }

    // read rest of the file
    size_t bytesLeft = Size - BufferedCharacters;
    char* writePtr = Buffer + BufferedCharacters;
    while (bytesLeft)
    {
        DWORD toRead = DWORD(min(size_t(512 * 1024), bytesLeft));
        DWORD read;
        if (!ReadFile(File, writePtr, toRead, &read, NULL) ||
            toRead != read)
            CFilecompWorker::CException::Raise(IDS_READFILE, GetLastError(), Name);
        if (MD5)
            MD5->Update(writePtr, toRead);
        bytesLeft -= toRead;
        writePtr += toRead;
        if (cancel)
            throw CFilecompWorker::CAbortByUserException();
    }

    buffer = (wchar_t*)Buffer;
    size = Size / 2; // TODO warn if byte is missing

    Buffer = NULL; // buffer byl predan volajicimu
    BufferedCharacters = 0;

    // convert to little endian
    if (Endian == endianBig)
    {
        for (wchar_t* iterator = buffer; iterator < buffer + size; ++iterator)
            *iterator = ((*iterator >> 8) | (*iterator << 8)) & 0xffff;
    }
}

void CTextFileReader::ReadUTF32(wchar_t*& buffer, size_t& size, const int& cancel)
{
    CByteReader input(Name, File, Size, Buffer, BufferSize, BufferedCharacters, cancel, MD5);

    Buffer = NULL; // o uvolneni se stara input
    BufferedCharacters = 0;

    // optimistic estimate (each UTF-32 byte codes one UTF-16 char, no surrogate
    // needed)
    size_t allocated = Size / 4;

    // reserve 1 char more at the end, for possible new-line insertion
    buffer = (wchar_t*)malloc((allocated + 1) * sizeof(wchar_t));
    if (!buffer)
        CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);
    size = 0;

    DWORD utf32;
    unsigned char b0, b1, b2, b3;
    while (1)
    {
        if (!input.GetByte(b0))
            break; // EOF
        if (!input.GetByte(b1))
            break; // incomplete code
        if (!input.GetByte(b2))
            break; // incomplete code
        if (!input.GetByte(b3))
            break; // incomplete code

        if (Endian == endianBig)
            utf32 = (DWORD(b0) << 24) + (DWORD(b1) << 16) + (DWORD(b2) << 8) + DWORD(b0);
        else
            utf32 = (DWORD(b3) << 24) + (DWORD(b2) << 16) + (DWORD(b1) << 8) + DWORD(b0);

        // Q.3 Mapping from UCS-4 form to UTF-16 form
        // (see http://www.cl.cam.ac.uk/~mgk25/ucs/ISO-10646-UTF-16.html)
        //
        // UCS-4 (4-octet)				UTF-16, 2-octet elements
        // -----------------------------------------------------------------------------------
        //   x = 0000 0000 .. 0000 FFFF (see Note 1)	  x % 0001 0000;
        // -----------------------------------------------------------------------------------
        //   x = 0001 0000 ..0010 FFFF			  y; z;
        //  						where
        //  						  y = ((x - 0001 0000) / 400) + D800
        //    						  z = ((x - 0001 0000) % 400) + DC00
        // -----------------------------------------------------------------------------------
        //   x 0011 0000 ..7FFF FFFF			(no mapping is defined)

        int convertToSurrogates = utf32 > 0xFFFF ? 1 : 0;

        // need more memory to store the file
        if (size + convertToSurrogates >= allocated)
        {
            allocated *= 2;
            // reserve 1 char more at the end, for possible new-line insertion
            wchar_t* ret = (wchar_t*)realloc(buffer, (allocated + 1) * sizeof(wchar_t));
            if (!ret)
                CFilecompWorker::CException::Raise(IDS_LOWMEM, 0);
            buffer = ret;
        }

        if (convertToSurrogates)
        {
            buffer[size++] = wchar_t(((utf32 - 0x00010000) >> 10) + 0xD800);
            buffer[size++] = wchar_t(((utf32 - 0x00010000) & 0x3FF) + 0xDC00);
        }
        else
        {
            buffer[size++] = wchar_t(utf32);
        }
    }
}

/*
template<CChar>
size_t 
CTextFileReader::ConvertEols(CChar * buffer, size_t size)
{
  if (!EolConversions) return size;

  bool cr = EolConversions & eolCR;
  bool crlf = EolConversions & eolCRLF;
  bool null = EolConversions & eolNULL;
  bool crconv = cr | crlf;

  CChar src = buffer;
  CChar dst = buffer;
  for ( ; src < buffer + size; ++src, ++dst)
  {
    if (*src == '\r' && crconv)
    {
      if (crlf && src + 1 < buffer + size && src[1] == '\n')
        src++;
      *dst = '\n';
    }
    elif (*src == 0 && null)
    {
      *dst = '\n';
    }
    else
    {
      if (dst < src) *dst = *src;
    }
  }
  return dst - buffer;
}
*/
