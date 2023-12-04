// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
  PE Viewer Plugin for Open Salamander

  Copyright (c) 2015-2016 Milan Kase <manison@manison.cz>
  Copyright (c) 2015-2016 Open Salamander Authors

  BinaryReader.h
  Binary reader.
*/

class BinaryReader
{
private:
    const BYTE* m_pBase;
    const BYTE* m_p;
    const BYTE* m_pMax;

#define READER_ALIGN_DOWN_POINTER(address, alignment) \
    ((void*)((DWORD_PTR)(address) & ~((DWORD_PTR)(alignment)-1)))

#define READER_ALIGN_UP_POINTER(address, alignment) \
    (READER_ALIGN_DOWN_POINTER(((DWORD_PTR)(address) + (alignment)-1), alignment))

    bool TryAlign(DWORD alignment)
    {
        const BYTE* p = (const BYTE*)READER_ALIGN_UP_POINTER(m_p, alignment);
        if (p < m_pMax)
        {
            m_p = p;
            return true;
        }
        return false;
    }

#undef READER_ALIGN_DOWN_POINTER
#undef READER_ALIGN_UP_POINTER

public:
    BinaryReader(const void* p, DWORD size)
    {
        m_pBase = reinterpret_cast<const BYTE*>(p);
        m_p = m_pBase;
        m_pMax = m_pBase + size;
    }

    bool TryReadByte(BYTE& b)
    {
        if (!SizeAvailable(sizeof(BYTE)))
        {
            return false;
        }

        b = *(reinterpret_cast<const BYTE*>(m_p));
        m_p += sizeof(BYTE);
        return true;
    }

    bool TryReadWord(WORD& w)
    {
        if (!SizeAvailable(sizeof(WORD)))
        {
            return false;
        }

        w = *(reinterpret_cast<const WORD*>(m_p));
        m_p += sizeof(WORD);
        return true;
    }

    bool TryReadDWord(DWORD& dw)
    {
        if (!SizeAvailable(sizeof(DWORD)))
        {
            return false;
        }

        dw = *(reinterpret_cast<const DWORD*>(m_p));
        m_p += sizeof(DWORD);
        return true;
    }

    bool EqualsUnicodeStringZ(LPCWSTR pwz)
    {
        bool readOk;
        const BYTE* p = m_p;
        for (;;)
        {
            readOk = static_cast<DWORD>(m_pMax - p) >= sizeof(WCHAR);
            if (!readOk)
            {
                break;
            }

            WCHAR charInStream = *(reinterpret_cast<const WCHAR*>(p));
            WCHAR charInString = *pwz;
            readOk = (charInStream == charInString);
            if (!readOk)
            {
                break;
            }

            p += sizeof(WCHAR);
            ++pwz;
            if (charInString == '\0')
            {
                break;
            }
        }

        if (readOk)
        {
            m_p = p;
        }

        return readOk;
    }

    bool TryReadUnicodeStringZ(LPCWSTR& pwz, DWORD& length)
    {
        bool readOk;
        const BYTE* p = m_p;
        for (;;)
        {
            readOk = static_cast<DWORD>(m_pMax - p) >= sizeof(WCHAR);
            if (!readOk)
            {
                break;
            }

            WCHAR charInStream = *(reinterpret_cast<const WCHAR*>(p));
            p += sizeof(WCHAR);
            if (charInStream == '\0')
            {
                break;
            }
        }

        if (readOk)
        {
            length = (DWORD)((p - m_p) / sizeof(WCHAR) - 1);
            pwz = reinterpret_cast<LPCWSTR>(m_p);
            m_p = p;
        }

        return readOk;
    }

    bool TryReadUnicodeStringZ(LPCWSTR& pwz)
    {
        DWORD dummy;
        return TryReadUnicodeStringZ(pwz, dummy);
    }

    bool TryReadUntil(BYTE b, const void*& pstart, DWORD& length)
    {
        pstart = m_p;

        const BYTE* p = m_p;
        while (p < m_pMax && *p != b)
        {
            ++p;
        }

        if (*p == b)
        {
            length = (DWORD)(p - m_p);
            m_p = p;
            return true;
        }

        return false;
    }

    bool TryReadLine(const void*& pstart, DWORD& length)
    {
        // CR...
        if (TryReadUntil('\r', pstart, length))
        {
            // Preskocit CR.
            if (m_p < m_pMax)
            {
                ++m_p;

                // LF...
                if (m_p < m_pMax && *m_p == '\n')
                {
                    ++m_p;
                }
            }

            return true;
        }

        return false;
    }

    bool TryReadToEnd(const void*& pstart, DWORD& length)
    {
        if (m_p < m_pMax)
        {
            pstart = m_p;
            length = (DWORD)(m_pMax - m_p);
            return true;
        }
        return false;
    }

    bool TrySkip(DWORD numberOfBytesToSkip)
    {
        if (!SizeAvailable(numberOfBytesToSkip))
        {
            return false;
        }

        m_p += numberOfBytesToSkip;
        return true;
    }

    bool TrySkipTo(const void* p)
    {
        const BYTE* pb = reinterpret_cast<const BYTE*>(p);
        if (pb >= m_pBase && pb < m_pMax)
        {
            m_p = pb;
            return true;
        }
        return false;
    }

    bool TryAlignDWord()
    {
        return TryAlign(sizeof(DWORD));
    }

    bool Eos() const
    {
        return m_p >= m_pMax;
    }

    DWORD TotalSize() const
    {
        return static_cast<DWORD>(m_pMax - m_pBase);
    }

    DWORD SizeAvailable() const
    {
        return static_cast<DWORD>(m_pMax - m_p);
    }

    bool SizeAvailable(DWORD requiredSize) const
    {
        return SizeAvailable() >= requiredSize;
    }

    const void* GetPointer() const
    {
        return m_p;
    }
};
