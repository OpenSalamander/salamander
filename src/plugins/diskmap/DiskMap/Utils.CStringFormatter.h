// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CStringFormatter
{
protected:
#ifndef OPENSAL_VERSION
#pragma region LocaleAPIHandler
#endif // OPENSAL_VERSION
    UINT GroupingStrToUint(LPCTSTR szGrouping)
    {
        LPCTSTR szCurr = szGrouping;
        UINT ret = 0;

        /*
		LOCALE_SGROUPING | Grouping | Sample     | Culture 
		3;0             |  3       |  1,234,567 |  United States 
		3;2;0           |  32      |  12,34,567 |  India 
		3               |  30      |   1234,567 |  ??? 
		*/

        for (;;)
        {
            ret *= 10;
            if (*szCurr == TEXT('\0'))
                break; //konec retezce pro konec nulou

            TCHAR* pch;
            ret += _tcstol(szCurr, &pch, 10); //pricist cislo

            if (_tcscmp(pch, TEXT(";0")) == 0)
                break; //konec

            szCurr = pch + 1; //ukazatel za stop znak
        }

        return ret;
    }
    // Fills the default NUMBERFMT structure for a given locale.
    BOOL LoadDefaultFormat()
    {
        TCHAR szBuf[80];

        int ret = ::GetLocaleInfo(this->_lcid, LOCALE_IDIGITS, szBuf, ARRAYSIZE(szBuf));
        if (ret == 0)
            return FALSE;
        this->_defformat.NumDigits = _tcstol(szBuf, NULL, 10);

        ret = ::GetLocaleInfo(this->_lcid, LOCALE_ILZERO, szBuf, ARRAYSIZE(szBuf));
        if (ret == 0)
            return FALSE;
        this->_defformat.LeadingZero = _tcstol(szBuf, NULL, 10);

        ret = ::GetLocaleInfo(this->_lcid, LOCALE_SGROUPING, szBuf, ARRAYSIZE(szBuf));
        if (ret == 0)
            return FALSE;
        this->_defformat.Grouping = GroupingStrToUint(szBuf);

        ret = ::GetLocaleInfo(this->_lcid, LOCALE_SDECIMAL, this->_defformat.lpDecimalSep, 5);
        if (ret == 0)
            return FALSE;

        ret = ::GetLocaleInfo(this->_lcid, LOCALE_STHOUSAND, this->_defformat.lpThousandSep, 5);
        if (ret == 0)
            return FALSE;

        ret = ::GetLocaleInfo(this->_lcid, LOCALE_INEGNUMBER, szBuf, ARRAYSIZE(szBuf));
        if (ret == 0)
            return FALSE;
        this->_defformat.NegativeOrder = _tcstol(szBuf, NULL, 10);

        return TRUE;
    }
#ifndef OPENSAL_VERSION
#pragma endregion LocaleAPIHandler
#endif // OPENSAL_VERSION

    LCID _lcid;
    NUMBERFMT _defformat;
    TCHAR _decimalSep[5];
    TCHAR _thousandSep[5];

    NUMBERFMT _intformat;

public:
    CStringFormatter(LCID lcid)
    {
        this->_lcid = lcid;
        this->_defformat.lpDecimalSep = this->_decimalSep;
        this->_defformat.lpThousandSep = this->_thousandSep;
        this->LoadDefaultFormat();
        this->_intformat = this->_defformat;
    }
    ~CStringFormatter()
    {
    }
    size_t FormatLongDate(TCHAR* s, size_t slen, SYSTEMTIME* st)
    {
        size_t len = 0;
        size_t l = GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, st, NULL, s, (int)slen);
        if (l == 0)
        {
            if (slen >= 2 + 1 + 2 + 1 + 4)
            {
                l = _stprintf(s, TEXT("%u.%u.%u"), st->wDay, st->wMonth, st->wYear);
            }
            if (l > 0)
            {
                len = l;
            }
            else
            {
                //TODO: Error...
            }
        }
        else
        {
            len = l - 1; //bez NULL
        }
        return len;
    }
    size_t FormatTime(TCHAR* s, size_t slen, SYSTEMTIME* st)
    {
        size_t len = 0;
        size_t l = GetTimeFormat(LOCALE_USER_DEFAULT, 0, st, NULL, s, (int)slen);
        if (l == 0)
        {
            if (slen >= 2 + 1 + 2 + 1 + 2)
            {
                l = _stprintf(s, TEXT("%u:%02u:%02u"), st->wHour, st->wMinute, st->wSecond);
            }
            if (l > 0)
            {
                len = l;
            }
            else
            {
                //TODO: Error...
            }
        }
        else
        {
            len = l - 1; //bez NULL
        }
        return len;
    }
    size_t FormatLongFileDate(TCHAR* s, size_t slen, FILETIME* ft)
    {
        SYSTEMTIME st;
        FILETIME lft;
        size_t len = 0;
        if (FileTimeToLocalFileTime(ft, &lft) && FileTimeToSystemTime(&lft, &st))
        {
            size_t l = FormatLongDate(s, slen, &st);
            if (l > 0)
            {
                s += l;
            }
            *s++ = TEXT(',');
            l++;
            *s++ = TEXT(' ');
            l++;
            *s = TEXT('\0'); //clovek nikdy nevi, kdyz to nevyjde...
            len = l;
            slen -= len;
            l = FormatTime(s, slen, &st);
            if (l > 0)
            {
                len += l;
            }
        }
        return len;
    }
    size_t FormatInteger(TCHAR* s, size_t slen, UINT64 val)
    {
        //%I64u -- 0 - 18446744073709551615
        TCHAR buff[30];
        size_t len = 0;
        size_t l1 = _stprintf(buff, TEXT("%I64u"), val);
        this->_intformat.NumDigits = 0;
        size_t l2 = GetNumberFormat(LOCALE_USER_DEFAULT, 0, buff, &this->_intformat, s, (int)slen);
        if (l2 == 0)
        {
            if (slen > l1)
            {
                _tcscpy(s, buff);
                len = l1;
            }
            else
            {
                //TODO: Error...
            }
        }
        else
        {
            len = l2 - 1;
        }
        return len;
    }
    size_t FormatReal(TCHAR* s, size_t slen, double val, int digits = 2)
    {
        //%1.3f --
        TCHAR buff[30];
        size_t len = 0;
        size_t l1 = _stprintf(buff, TEXT("%1.3f"), val);
        this->_intformat.NumDigits = digits;
        size_t l2 = GetNumberFormat(LOCALE_USER_DEFAULT, 0, buff, &this->_intformat, s, (int)slen);
        if (l2 == 0)
        {
            if (slen > l1)
            {
                _tcscpy(s, buff);
                len = l1;
            }
            else
            {
                //TODO: Error...
            }
        }
        else
        {
            len = l2 - 1;
        }
        return len;
    }

    size_t FormatLongFileSize(TCHAR* s, size_t slen, UINT64 size)
    {
        size_t len = 0;
        size_t l = FormatInteger(s, slen, size);
        if (l > 0)
        {
            s += l;
            len += l;
            slen -= l;
            l = 0;
            UINT bytestrid = IDS_DISKMAP_FORMAT_BYTE0;
            if (size > 0)
            {
                if (size > 1)
                {
                    bytestrid = IDS_DISKMAP_FORMAT_BYTEN;
                }
                else
                {
                    bytestrid = IDS_DISKMAP_FORMAT_BYTE1;
                }
            }
            size_t bytelen = CZResourceString::GetLength(bytestrid);
            if (slen > bytelen)
            {
                *s++ = TEXT(' ');
                l++;
                TCHAR const* bytestr = CZResourceString::GetString(bytestrid);
                while (bytelen-- > 0)
                {
                    *s++ = *bytestr++;
                    l++;
                }
            }
            *s = TEXT('\0'); //TODO: zkontrolovat, zda neprepisuju znak za koncem
            len += l;
        }
        return len;
    }
    size_t FormatShortFileSize(TCHAR* s, size_t slen, UINT64 size)
    {
        static const TCHAR expch[] = TEXT(" KMGTPEZY");
        static const int maxexp = sizeof expch / sizeof TCHAR - 2;
        size_t len = 0;
        int exp = 0;
        //C4244 ok: potrebuji jen par prvnich par ciclic, protoze pocitam kratky tvar "1,45TB"
        double rs = (double)(__int64)size;
        while (rs >= 1024 && exp < maxexp)
        {
            rs /= 1024;
            exp++;
        }
        int dec = 2;
        //if (rs < 10) dec = 3;
        //if (rs > 100) dec = 1;
        size_t l = FormatReal(s, slen - 3, rs, dec);
        if (l > 0)
        {
            s += l;
            *s++ = TEXT(' ');
            l++;
            if (exp > 0)
            {
                *s++ = expch[exp];
                l++;
            }
            *s++ = TEXT('B');
            l++;
            *s = TEXT('\0');

            len = l;
        }
        return len;
    }
    size_t FormatHumanFileSize(TCHAR* s, size_t slen, UINT64 size)
    {
        //%I64u -- 0 - 18446744073709551615
        size_t len = 0;
        size_t l = FormatLongFileSize(s, slen, size);

        if (l > 0)
        {
            if (size >= 1024)
            {
                //velky, zkusime pridat kratky zapis
                s += l;
                TCHAR* si = s;
                len += l;
                slen -= l;
                l = 0;
                if (slen > 5)
                {
                    *si++ = TEXT(' ');
                    l++;
                    *si++ = TEXT('(');
                    l++;
                    //*si = TEXT('\0'); //kdyby nahodou

                    int li = (int)FormatShortFileSize(si, slen - 3, size);
                    if (li > 0)
                    {
                        l += li;
                        si += li;
                        *si++ = TEXT(')');
                        l++;
                        *si = TEXT('\0'); //konec
                        len += l;
                    }
                    else
                    {
                        //nepodarilo se pridat vnitrnosti -> odriznout zavorku
                        *s = TEXT('\0');
                    }
                }
            }
            else
            {
                len = l;
            }
        }
        else
        {
            //neni misto pro bajty, zkusime kratky zapis
            len = FormatShortFileSize(s, slen, size);
        }
        return len;
    }
    size_t FormatExplorerFileSize(TCHAR* s, size_t slen, UINT64 size)
    {
        //%I64u -- 0 - 18446744073709551615
        size_t len = 0;
        size_t l = FormatShortFileSize(s, slen, size);

        if (l > 0)
        {
            if (size >= 1024)
            {
                //velky, zkusime pridat kratky zapis
                s += l;
                TCHAR* si = s;
                len += l;
                slen -= l;
                l = 0;
                if (slen > 5)
                {
                    *si++ = TEXT(' ');
                    l++;
                    *si++ = TEXT('(');
                    l++;
                    //*si = TEXT('\0'); //kdyby nahodou

                    int li = (int)FormatLongFileSize(si, slen - 3, size);
                    if (li > 0)
                    {
                        l += li;
                        si += li;
                        *si++ = TEXT(')');
                        l++;
                        *si = TEXT('\0'); //konec
                        len += l;
                    }
                    else
                    {
                        //nepodarilo se pridat vnitrnosti -> odriznout zavorku
                        *s = TEXT('\0');
                    }
                }
            }
            else
            {
                len = l;
            }
        }
        else
        {
            //neni misto pro bajty, zkusime kratky zapis
            len = FormatShortFileSize(s, slen, size);
        }
        return len;
    }
    void FormatLongFileDate(CZStringBuffer* s, FILETIME* ft)
    {
        s->_l = FormatLongFileDate(s->_s, s->_c, ft);
    }
    void FormatShortFileSize(CZStringBuffer* s, UINT64 size)
    {
        s->_l = FormatShortFileSize(s->_s, s->_c, size);
    }
    void FormatHumanFileSize(CZStringBuffer* s, UINT64 size)
    {
        s->_l = FormatHumanFileSize(s->_s, s->_c, size);
    }
    void FormatExplorerFileSize(CZStringBuffer* s, UINT64 size)
    {
        s->_l = FormatExplorerFileSize(s->_s, s->_c, size);
    }
    void FormatInteger(CZStringBuffer* s, UINT64 val)
    {
        s->_l = FormatInteger(s->_s, s->_c, val);
    }
};
