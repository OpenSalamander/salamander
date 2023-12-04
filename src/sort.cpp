// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"

//
//*****************************************************************************

// od XPcek je v systemu StrCmpLogicalW, kterou pro toto porovnani pouziva Explorer
int StrCmpLogicalEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual, BOOL ignoreCase)
{
    const char* strEnd1 = s1 + l1; // konec retezce 's1'
    const char* beg1 = s1;         // zacatek useku (textu nebo cisla)
    const char* end1 = s1;         // konec useku (textu nebo cisla)
    const char* strEnd2 = s2 + l2; // konec retezce 's2'
    const char* beg2 = s2;         // zacatek useku (textu nebo cisla)
    const char* end2 = s2;         // konec useku (textu nebo cisla)
    int suggestion = 0;            // "doporuceni" vysledku (0 / -1 / 1 = nic / s1<s2 / s1>s2) - napr. "001" < "01"

    BOOL findDots = WindowsVistaAndLater && !SystemPolicies.GetNoDotBreakInLogicalCompare(); // TRUE = jmena se deli taky po teckach (nejen po cislech)

    while (1)
    {
        const char* numBeg1 = NULL; // pozice prvni nenulovy cislice
        BOOL isStr1 = (end1 >= strEnd1 || *end1 < '0' || *end1 > '9');
        if (isStr1) // text (i prazdny) nebo tecka
        {
            if (findDots && end1 < strEnd1 && *end1 == '.')
                end1++; // tecka: pokud je hledame, bereme jednu po druhe
            else        // text (i prazdny)
            {
                while (end1 < strEnd1 && (*end1 < '0' || *end1 > '9') && (!findDots || *end1 != '.'))
                    end1++;
            }
        }
        else // cislo
        {
            while (end1 < strEnd1 && *end1 >= '0' && *end1 <= '9')
            {
                if (numBeg1 == NULL && *end1 != '0')
                    numBeg1 = end1;
                end1++;
            }
        }
        const char* numBeg2 = NULL; // pozice prvni nenulovy cislice
        BOOL isStr2 = (end2 >= strEnd2 || *end2 < '0' || *end2 > '9');
        if (isStr2) // text (i prazdny) nebo tecka
        {
            if (findDots && end2 < strEnd2 && *end2 == '.')
                end2++; // tecka: pokud je hledame, bereme jednu po druhe
            else        // text (i prazdny)
            {
                while (end2 < strEnd2 && (*end2 < '0' || *end2 > '9') && (!findDots || *end2 != '.'))
                    end2++;
            }
        }
        else // cislo
        {
            while (end2 < strEnd2 && *end2 >= '0' && *end2 <= '9')
            {
                if (numBeg2 == NULL && *end2 != '0')
                    numBeg2 = end2;
                end2++;
            }
        }

        if (isStr1 || isStr2) // porovnani textu, tecek nebo kombinovane dvojice textu, tecky nebo cisla (vse krom dvou cisel se porovnava stringove)
        {
            int ret;
            if (Configuration.SortUsesLocale)
            {
                ret = CompareString(LOCALE_USER_DEFAULT, ignoreCase ? NORM_IGNORECASE : 0,
                                    beg1, (int)(end1 - beg1), beg2, (int)(end2 - beg2)) -
                      CSTR_EQUAL;
            }
            else
            {
                if (ignoreCase)
                    ret = StrICmpEx(beg1, (int)(end1 - beg1), beg2, (int)(end2 - beg2));
                else
                    ret = StrCmpEx(beg1, (int)(end1 - beg1), beg2, (int)(end2 - beg2));
            }
            if (ret != 0)
            {
                if (numericalyEqual != NULL)
                    *numericalyEqual = FALSE;
                return ret;
            }
        }
        else // porovnani dvou cisel
        {
            if (numBeg1 == NULL)
            {
                if (numBeg2 == NULL) // obe cisla jsou nulova
                {
                    if (suggestion == 0) // zajima nas jen prvni "doporuceni" vysledku
                    {
                        if (end1 - beg1 > end2 - beg2)
                            suggestion = -1; // "000" < "00"
                        else if (end1 - beg1 < end2 - beg2)
                            suggestion = 1; // "00" > "000"
                    }
                }
                else // prvni cislo je nulove, druhe cislo neni nulove
                {
                    if (numericalyEqual != NULL)
                        *numericalyEqual = FALSE;
                    return -1; // "00" < "1"
                }
            }
            else
            {
                if (numBeg2 == NULL) // prvni cislo neni nulove, druhe cislo je nulove
                {
                    if (numericalyEqual != NULL)
                        *numericalyEqual = FALSE;
                    return 1; // "1" > "00"
                }
                else // obe cisla jsou nenulova
                {
                    if (end1 - numBeg1 > end2 - numBeg2) // prvni cislo ma vic cislic nez druhe
                    {
                        if (numericalyEqual != NULL)
                            *numericalyEqual = FALSE;
                        return 1; // "100" > "99"
                    }
                    else
                    {
                        if (end1 - numBeg1 < end2 - numBeg2) // druhe cislo ma vic cislic nez prvni
                        {
                            if (numericalyEqual != NULL)
                                *numericalyEqual = FALSE;
                            return -1; // "99" < "100"
                        }
                        else // cisla maji stejny pocet cislic, porovname je podle hodnoty (ekvivalentni retezcovemu porovnani)
                        {
                            int ret = StrCmpEx(numBeg1, (int)(end1 - numBeg1), numBeg2, (int)(end2 - numBeg2));
                            if (ret != 0) // hodnoty nejsou shodne
                            {
                                if (numericalyEqual != NULL)
                                    *numericalyEqual = FALSE;
                                return ret;
                            }
                            else // hodnoty cisel jsou stejne, pokud se lisi poctem nul v prefixu, prevezmeme to do "doporuceni" vysledku
                            {
                                if (suggestion == 0) // zajima nas jen prvni "doporuceni" vysledku
                                {
                                    if (end1 - beg1 > end2 - beg2)
                                        suggestion = -1; // "0001" < "001"
                                    else if (end1 - beg1 < end2 - beg2)
                                        suggestion = 1; // "001" > "0001"
                                }
                            }
                        }
                    }
                }
            }
        }

        if (end1 >= strEnd1 && end2 >= strEnd2)
            break; // konec porovnavani
        beg1 = end1;
        beg2 = end2;
    }

    if (numericalyEqual != NULL)
        *numericalyEqual = TRUE; // s1 a s2 jsou shodna nebo numericky shodna
    return suggestion;           // pri shode nebo numericke shode vracime "doporuceny" vysledek
}

//
//*****************************************************************************

int RegSetStrICmp(const char* s1, const char* s2)
{
    if (Configuration.SortDetectNumbers)
    {
        return StrCmpLogicalEx(s1, (int)strlen(s1), s2, (int)strlen(s2), NULL, TRUE);
    }
    else
    {
        if (Configuration.SortUsesLocale)
        {
            return CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, s1, -1, s2, -1) - CSTR_EQUAL;
        }
        else
        {
            return StrICmp(s1, s2);
        }
    }
}

int RegSetStrICmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual)
{
    if (Configuration.SortDetectNumbers)
    {
        return StrCmpLogicalEx(s1, l1, s2, l2, numericalyEqual, TRUE);
    }
    else
    {
        int ret;
        if (Configuration.SortUsesLocale)
        {
            ret = CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, s1, l1, s2, l2) - CSTR_EQUAL;
        }
        else
        {
            ret = StrICmpEx(s1, l1, s2, l2);
        }
        if (numericalyEqual != NULL)
            *numericalyEqual = ret == 0;
        return ret;
    }
}

int RegSetStrCmp(const char* s1, const char* s2)
{
    if (Configuration.SortDetectNumbers)
    {
        return StrCmpLogicalEx(s1, (int)strlen(s1), s2, (int)strlen(s2), NULL, FALSE);
    }
    else
    {
        if (Configuration.SortUsesLocale)
        {
            return CompareString(LOCALE_USER_DEFAULT, 0, s1, -1, s2, -1) - CSTR_EQUAL;
        }
        else
        {
            return strcmp(s1, s2);
        }
    }
}

int RegSetStrCmpEx(const char* s1, int l1, const char* s2, int l2, BOOL* numericalyEqual)
{
    if (Configuration.SortDetectNumbers)
    {
        return StrCmpLogicalEx(s1, l1, s2, l2, numericalyEqual, FALSE);
    }
    else
    {
        int ret;
        if (Configuration.SortUsesLocale)
        {
            ret = CompareString(LOCALE_USER_DEFAULT, 0, s1, l1, s2, l2) - CSTR_EQUAL;
        }
        else
        {
            ret = StrCmpEx(s1, l1, s2, l2);
        }
        if (numericalyEqual != NULL)
            *numericalyEqual = ret == 0;
        return ret;
    }
}

//
//*****************************************************************************
// QuickSort   1.klic Name, 2.klic Ext
//

int CmpNameExtIgnCase(const CFileData& f1, const CFileData& f2)
{
    /*
//--- nejprve podle Name
  BOOL numericalyEqual1;
  int res1 = RegSetStrICmpEx(f1.Name, (*f1.Ext != 0) ? (f1.Ext - 1 - f1.Name) : f1.NameLen,
                             f2.Name, (*f2.Ext != 0) ? (f2.Ext - 1 - f2.Name) : f2.NameLen,
                             &numericalyEqual1);
  if (!numericalyEqual1) return res1;   // jmena se lisi (nejsou shodne ani numericky shodne)
//--- podle Name se rovnaji, rozhodne Ext
  BOOL numericalyEqual2;
  int res2 = RegSetStrICmpEx(f1.Ext, f1.NameLen - (f1.Ext - f1.Name),
                             f2.Ext, f2.NameLen - (f2.Ext - f2.Name),
                             &numericalyEqual2);
  if (numericalyEqual2 && res1 != 0) return res1; // pripony jsou shodne nebo numericky shodne a jmena jsou jen numericky shodna (porovnani jmen je prioritnejsi)
  else return res2;
*/
    //--- porovnavame cele Name (vcetne Ext), jako Explorer
    return RegSetStrICmpEx(f1.Name, f1.NameLen, f2.Name, f2.NameLen, NULL);
}

int CmpNameExt(const CFileData& f1, const CFileData& f2)
{
    /*  // stara varianta: porovnavame oddelene jmeno a priponu
//--- nejprve podle Name
  BOOL numericalyEqual1;
  int res1 = RegSetStrICmpEx(f1.Name, (*f1.Ext != 0) ? (f1.Ext - 1 - f1.Name) : f1.NameLen,
                             f2.Name, (*f2.Ext != 0) ? (f2.Ext - 1 - f2.Name) : f2.NameLen,
                             &numericalyEqual1);
  if (!numericalyEqual1) return res1;   // jmena se lisi (nejsou shodne ani numericky shodne)
//--- podle Name se rovnaji, rozhodne Ext
  BOOL numericalyEqual2;
  int res2 = RegSetStrICmpEx(f1.Ext, f1.NameLen - (f1.Ext - f1.Name),
                             f2.Ext, f2.NameLen - (f2.Ext - f2.Name),
                             &numericalyEqual2);
  if (numericalyEqual2 && res1 != 0) return res1; // pripony jsou shodne nebo numericky shodne a jmena jsou jen numericky shodna (porovnani jmen je prioritnejsi)
  else
  {
    if (res2 != 0 || f1.Name == f2.Name) return res2; // pokud jsou shodne adresy, musi se rovnat
  }
//--- shodna jmena (archivy nebo FS) - zkusime jestli se nelisi aspon ve velikosti pismen
  res1 = RegSetStrCmpEx(f1.Name, (*f1.Ext != 0) ? (f1.Ext - 1 - f1.Name) : f1.NameLen,
                        f2.Name, (*f2.Ext != 0) ? (f2.Ext - 1 - f2.Name) : f2.NameLen,
                        &numericalyEqual1);
  if (!numericalyEqual1) return res1;   // jmena se lisi (nejsou shodne ani numericky shodne)
//--- podle Name se opet rovnaji, rozhodne Ext
  res2 = RegSetStrCmpEx(f1.Ext, f1.NameLen - (f1.Ext - f1.Name),
                        f2.Ext, f2.NameLen - (f2.Ext - f2.Name),
                        &numericalyEqual2);
  if (numericalyEqual2 && res1 != 0) return res1; // pripony jsou shodne nebo numericky shodne a jmena jsou jen numericky shodna (porovnani jmen je prioritnejsi)
  else return res2;
*/
    //--- porovnavame cele Name (vcetne Ext), jako Explorer
    int res = RegSetStrICmpEx(f1.Name, f1.NameLen, f2.Name, f2.NameLen, NULL);
    if (res != 0 || f1.Name == f2.Name)
        return res; // pokud jsou shodne adresy, musi se rovnat
                    //--- shodna jmena (archivy nebo FS) - zkusime jestli se nelisi aspon ve velikosti pismen
    return RegSetStrCmpEx(f1.Name, f1.NameLen, f2.Name, f2.NameLen, NULL);
}

BOOL LessNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse)
{
    int res = CmpNameExt(f1, f2);
    return reverse ? res > 0 : res < 0;
}

BOOL LessNameExtIgnCase(const CFileData& f1, const CFileData& f2, BOOL reverse)
{
    int res = CmpNameExtIgnCase(f1, f2);
    return reverse ? res > 0 : res < 0;
}

void SortNameExtAux(CFilesArray& files, int left, int right, BOOL reverse)
{

LABEL_SortNameExtAux:

    int i = left, j = right;
    CFileData pivot = files[(i + j) / 2];

    do
    {
        while (LessNameExt(files[i], pivot, reverse) && i < right)
            i++;
        while (LessNameExt(pivot, files[j], reverse) && j > left)
            j--;

        if (i <= j)
        {
            CFileData swap = files[i];
            files[i] = files[j];
            files[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortNameExtAux(files, left, j, reverse);
    //  if (i < right) SortNameExtAux(files, i, right, reverse);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                SortNameExtAux(files, left, j, reverse);
                left = i;
                goto LABEL_SortNameExtAux;
            }
            else
            {
                SortNameExtAux(files, i, right, reverse);
                right = j;
                goto LABEL_SortNameExtAux;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortNameExtAux;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortNameExtAux;
        }
    }
}

void SortNameExt(CFilesArray& files, int left, int right, BOOL reverse)
{
    SortNameExtAux(files, left, right, reverse);
}

//
//*****************************************************************************
// QuickSort   1.klic Ext, 2.klic Name
//

BOOL LessExtName(const CFileData& f1, const CFileData& f2, BOOL reverse)
{
    //--- nejprve podle Ext
    BOOL numericalyEqual1;
    int res1 = RegSetStrICmpEx(f1.Ext, f1.NameLen - (int)(f1.Ext - f1.Name),
                               f2.Ext, f2.NameLen - (int)(f2.Ext - f2.Name),
                               &numericalyEqual1);
    if (!numericalyEqual1)
        return reverse ? res1 > 0 : res1 < 0; // pripony se lisi (nejsou shodne ani numericky shodne)
                                              //--- podle Ext se rovnaji, rozhodne Name
    BOOL numericalyEqual2;
    int res2 = RegSetStrICmpEx(f1.Name, (*f1.Ext != 0) ? (int)(f1.Ext - 1 - f1.Name) : f1.NameLen,
                               f2.Name, (*f2.Ext != 0) ? (int)(f2.Ext - 1 - f2.Name) : f2.NameLen,
                               &numericalyEqual2);
    if (numericalyEqual2 && res1 != 0)
        return reverse ? res1 > 0 : res1 < 0; // pripony jsou shodne nebo numericky shodne a jmena jsou jen numericky shodna (porovnani jmen je prioritnejsi)
    else
    {
        if (res2 == 0 && f1.Name != f2.Name) // shodna jmena (archivy nebo FS) - zkusime jestli se nelisi aspon ve velikosti pismen
        {
            res1 = RegSetStrCmpEx(f1.Ext, f1.NameLen - (int)(f1.Ext - f1.Name),
                                  f2.Ext, f2.NameLen - (int)(f2.Ext - f2.Name),
                                  &numericalyEqual1);
            if (!numericalyEqual1)
                return reverse ? res1 > 0 : res1 < 0; // pripony se lisi (nejsou shodne ani numericky shodne)
            //--- podle Ext se opet rovnaji, rozhodne Name
            res2 = RegSetStrCmpEx(f1.Name, (*f1.Ext != 0) ? (int)(f1.Ext - 1 - f1.Name) : f1.NameLen,
                                  f2.Name, (*f2.Ext != 0) ? (int)(f2.Ext - 1 - f2.Name) : f2.NameLen,
                                  &numericalyEqual2);
            if (numericalyEqual2 && res1 != 0)
                return reverse ? res1 > 0 : res1 < 0; // jmena jsou shodna nebo numericky shodna a pripony jsou jen numericky shodne (porovnani pripon je prioritnejsi)
        }
        return reverse ? res2 > 0 : res2 < 0;
    }
}

void SortExtNameAux(CFilesArray& files, int left, int right, BOOL reverse)
{

LABEL_SortExtNameAux:

    int i = left, j = right;
    CFileData pivot = files[(i + j) / 2];

    do
    {
        while (LessExtName(files[i], pivot, reverse) && i < right)
            i++;
        while (LessExtName(pivot, files[j], reverse) && j > left)
            j--;

        if (i <= j)
        {
            CFileData swap = files[i];
            files[i] = files[j];
            files[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortExtNameAux(files, left, j, reverse);
    //  if (i < right) SortExtNameAux(files, i, right, reverse);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                SortExtNameAux(files, left, j, reverse);
                left = i;
                goto LABEL_SortExtNameAux;
            }
            else
            {
                SortExtNameAux(files, i, right, reverse);
                right = j;
                goto LABEL_SortExtNameAux;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortExtNameAux;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortExtNameAux;
        }
    }
}

void SortExtName(CFilesArray& files, int left, int right, BOOL reverse)
{
    SortExtNameAux(files, left, right, reverse);
}

//
//*****************************************************************************
// QuickSort   1.klic Time 2.klic Name, 3.klic Ext
//

BOOL LessTimeNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse)
{
    //--- nejprve podle Time
    int res = CompareFileTime(&f1.LastWrite, &f2.LastWrite);
    if (res != 0)
        return (reverse ^ Configuration.SortNewerOnTop) ? res > 0 : res < 0;
    //--- podle Time se rovnaji, dale Name
    res = CmpNameExt(f1, f2);
    return reverse ? res > 0 : res < 0;
}

void SortTimeNameExtAux(CFilesArray& files, int left, int right, BOOL reverse)
{

LABEL_SortTimeNameExtAux:

    int i = left, j = right;
    CFileData pivot = files[(i + j) / 2];

    do
    {
        while (LessTimeNameExt(files[i], pivot, reverse) && i < right)
            i++;
        while (LessTimeNameExt(pivot, files[j], reverse) && j > left)
            j--;

        if (i <= j)
        {
            CFileData swap = files[i];
            files[i] = files[j];
            files[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortTimeNameExtAux(files, left, j, reverse);
    //  if (i < right) SortTimeNameExtAux(files, i, right, reverse);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                SortTimeNameExtAux(files, left, j, reverse);
                left = i;
                goto LABEL_SortTimeNameExtAux;
            }
            else
            {
                SortTimeNameExtAux(files, i, right, reverse);
                right = j;
                goto LABEL_SortTimeNameExtAux;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortTimeNameExtAux;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortTimeNameExtAux;
        }
    }
}

void SortTimeNameExt(CFilesArray& files, int left, int right, BOOL reverse)
{
    SortTimeNameExtAux(files, left, right, reverse);
}

//
//*****************************************************************************
// QuickSort   1.klic Size 2.klic Name, 3.klic Ext
//

BOOL LessSizeNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse)
{
    //--- nejprve podle Time
    if (f1.Size != f2.Size)
        return reverse ? f1.Size > f2.Size : f1.Size < f2.Size; // prvni soubor = nejvetsi soubor
                                                                //--- podle Size se rovnaji, dale Name
    int res = CmpNameExt(f1, f2);
    return reverse ? res > 0 : res < 0;
}

void SortSizeNameExtAux(CFilesArray& files, int left, int right, BOOL reverse)
{

LABEL_SortSizeNameExtAux:

    int i = left, j = right;
    CFileData pivot = files[(i + j) / 2];

    do
    {
        while (LessSizeNameExt(files[i], pivot, reverse) && i < right)
            i++;
        while (LessSizeNameExt(pivot, files[j], reverse) && j > left)
            j--;

        if (i <= j)
        {
            CFileData swap = files[i];
            files[i] = files[j];
            files[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortSizeNameExtAux(files, left, j, reverse);
    //  if (i < right) SortSizeNameExtAux(files, i, right, reverse);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                SortSizeNameExtAux(files, left, j, reverse);
                left = i;
                goto LABEL_SortSizeNameExtAux;
            }
            else
            {
                SortSizeNameExtAux(files, i, right, reverse);
                right = j;
                goto LABEL_SortSizeNameExtAux;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortSizeNameExtAux;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortSizeNameExtAux;
        }
    }
}

void SortSizeNameExt(CFilesArray& files, int left, int right, BOOL reverse)
{
    SortSizeNameExtAux(files, left, right, reverse);
}

//
//*****************************************************************************
// QuickSort   1.klic Attr 2.klic Name, 3.klic Ext
//

BOOL LessAttrNameExt(const CFileData& f1, const CFileData& f2, BOOL reverse)
{
    // okopcim FILE_ATTRIBUTE_READONLY na nejvyznamejsi bit
    //  DWORD f1Attr = f1.Attr;
    //  DWORD f2Attr = f2.Attr;
    //  if (f1.Attr & FILE_ATTRIBUTE_READONLY) f1Attr |= 0x80000000;
    //  if (f2.Attr & FILE_ATTRIBUTE_READONLY) f2Attr |= 0x80000000;

    // pokud podporime zobrazovani dalsiho atributu,
    // je treba rozsirit masku DISPLAYED_ATTRIBUTES

    // prejdeme na abecedni razeni, jako ma explorer a speed commander
    DWORD f1Attr = 0;
    DWORD f2Attr = 0;
    if (f1.Attr & FILE_ATTRIBUTE_ARCHIVE)
        f1Attr |= 0x00000001;
    if (f1.Attr & FILE_ATTRIBUTE_COMPRESSED)
        f1Attr |= 0x00000002;
    if (f1.Attr & FILE_ATTRIBUTE_ENCRYPTED)
        f1Attr |= 0x00000004;
    if (f1.Attr & FILE_ATTRIBUTE_HIDDEN)
        f1Attr |= 0x00000008;
    if (f1.Attr & FILE_ATTRIBUTE_READONLY)
        f1Attr |= 0x00000010;
    if (f1.Attr & FILE_ATTRIBUTE_SYSTEM)
        f1Attr |= 0x00000020;
    if (f1.Attr & FILE_ATTRIBUTE_TEMPORARY)
        f1Attr |= 0x00000040;

    if (f2.Attr & FILE_ATTRIBUTE_ARCHIVE)
        f2Attr |= 0x00000001;
    if (f2.Attr & FILE_ATTRIBUTE_COMPRESSED)
        f2Attr |= 0x00000002;
    if (f2.Attr & FILE_ATTRIBUTE_ENCRYPTED)
        f2Attr |= 0x00000004;
    if (f2.Attr & FILE_ATTRIBUTE_HIDDEN)
        f2Attr |= 0x00000008;
    if (f2.Attr & FILE_ATTRIBUTE_READONLY)
        f2Attr |= 0x00000010;
    if (f2.Attr & FILE_ATTRIBUTE_SYSTEM)
        f2Attr |= 0x00000020;
    if (f2.Attr & FILE_ATTRIBUTE_TEMPORARY)
        f2Attr |= 0x00000040;

    //--- nejprve podle Attr
    if (f1Attr != f2Attr)
        return reverse ? f1Attr > f2Attr : f1Attr < f2Attr;
    //--- podle Attr se rovnaji, dale Name
    int res = CmpNameExt(f1, f2);
    return reverse ? res > 0 : res < 0;
}

void SortAttrNameExtAux(CFilesArray& files, int left, int right, BOOL reverse)
{

LABEL_SortAttrNameExtAux:

    int i = left, j = right;
    CFileData pivot = files[(i + j) / 2];

    do
    {
        while (LessAttrNameExt(files[i], pivot, reverse) && i < right)
            i++;
        while (LessAttrNameExt(pivot, files[j], reverse) && j > left)
            j--;

        if (i <= j)
        {
            CFileData swap = files[i];
            files[i] = files[j];
            files[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) SortAttrNameExtAux(files, left, j, reverse);
    //  if (i < right) SortAttrNameExtAux(files, i, right, reverse);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                SortAttrNameExtAux(files, left, j, reverse);
                left = i;
                goto LABEL_SortAttrNameExtAux;
            }
            else
            {
                SortAttrNameExtAux(files, i, right, reverse);
                right = j;
                goto LABEL_SortAttrNameExtAux;
            }
        }
        else
        {
            right = j;
            goto LABEL_SortAttrNameExtAux;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_SortAttrNameExtAux;
        }
    }
}

void SortAttrNameExt(CFilesArray& files, int left, int right, BOOL reverse)
{
    SortAttrNameExtAux(files, left, right, reverse);
}

//
//*****************************************************************************
// QuickSort pro integer
//

void IntSort(int array[], int left, int right)
{

LABEL_IntSort:

    int i = left, j = right;
    int pivot = array[(i + j) / 2];

    do
    {
        while (array[i] < pivot && i < right)
            i++;
        while (pivot < array[j] && j > left)
            j--;

        if (i <= j)
        {
            int swap = array[i];
            array[i] = array[j];
            array[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    // nasledujici "hezky" kod jsme nahradili kodem podstatne setricim stack (max. log(N) zanoreni rekurze)
    //  if (left < j) IntSort(array, left, j);
    //  if (i < right) IntSort(array, i, right);

    if (left < j)
    {
        if (i < right)
        {
            if (j - left < right - i) // je potreba seradit obe "poloviny", tedy do rekurze posleme tu mensi, tu druhou zpracujeme pres "goto"
            {
                IntSort(array, left, j);
                left = i;
                goto LABEL_IntSort;
            }
            else
            {
                IntSort(array, i, right);
                right = j;
                goto LABEL_IntSort;
            }
        }
        else
        {
            right = j;
            goto LABEL_IntSort;
        }
    }
    else
    {
        if (i < right)
        {
            left = i;
            goto LABEL_IntSort;
        }
    }
}
