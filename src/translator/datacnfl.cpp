// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
//#include <math.h>

#include "wndout.h"
#include "wndframe.h"
#include "datarh.h"
#include "config.h"

enum CButtonType
{
    btOther,
    btPush,
    btCheck,
    btRadio,
    btGroup,
};

CButtonType GetButtonType(DWORD style)
{
    switch (style & BS_TYPEMASK)
    {
    case BS_PUSHBUTTON:
    case BS_DEFPUSHBUTTON:
        return btPush;

    case BS_CHECKBOX:
    case BS_AUTOCHECKBOX:
    case BS_AUTO3STATE:
    case BS_3STATE:
    {
        if (style & BS_PUSHLIKE)
            return btPush;
        return btCheck;
    }

    case BS_RADIOBUTTON:
    case BS_AUTORADIOBUTTON:
    {
        if (style & BS_PUSHLIKE)
            return btPush;
        return btRadio;
    }

    case BS_GROUPBOX:
        return btGroup;
    }
    return btOther;
}

// Hotkey collision detector

struct CKey
{
    wchar_t Key;
    WORD Index;
    WORD LVIndex;
    WORD ControlID; // If it is 0, LVIndex is used; otherwise ControlID is used and LVIndex is 0
    DWORD Param1;
    DWORD Param2;
    DWORD Param3;
    DWORD Param4;
    BOOL Conflict;
};

class CKeyConflict
{
protected:
    TDirectArray<CKey> Keys;
    int Iterator;

public:
    CKeyConflict();

    // Remove all added keys from the array
    void Cleanup();

    // Add a key to the array; returns TRUE if the insertion succeeds
    // FALSE if memory is insufficient
    BOOL AddKey(wchar_t key, WORD index, WORD lvIndex, WORD controlID, DWORD param1 = 0, DWORD param2 = 0,
                DWORD param3 = 0, DWORD param4 = 0);
    BOOL AddString(const wchar_t* string, WORD index, WORD lvIndex, WORD controlID, DWORD param1 = 0,
                   DWORD param2 = 0, DWORD param3 = 0, DWORD param4 = 0);

    // Must be called before the first GetNextConflict invocation
    void PrepareConflictIteration(CDialogData* dialog = NULL);

    // Returns TRUE if a conflict was found and fills the pointers
    // Returns FALSE when there are no more conflicts
    BOOL GetNextConflict(WORD* index, WORD* lvIndex, WORD* controlID, DWORD* param1 = NULL,
                         DWORD* param2 = NULL, DWORD* param3 = NULL, DWORD* param4 = NULL);

    // Walk through 'string', remove the hotkeys already used, and return the rest in 'availableKeys'
    void GetAvailableKeys(const wchar_t* string, wchar_t* availableKeys, int availableKeysSize);

protected:
    void SortItems(int left, int right);
    BOOL HasKey(wchar_t key);
};

CKeyConflict::CKeyConflict()
    : Keys(50, 25)
{
    Cleanup();
}

void CKeyConflict::Cleanup()
{
    Keys.DestroyMembers();
    PrepareConflictIteration();
}

void CKeyConflict::SortItems(int left, int right)
{
    int i = left, j = right;
    wchar_t pivot = Keys[(i + j) / 2].Key;

    do
    {
        while ((Keys[i].Key < pivot) && i < right)
            i++;
        while ((pivot < Keys[j].Key) && j > left)
            j--;

        if (i <= j)
        {
            CKey swap = Keys[i];
            Keys[i] = Keys[j];
            Keys[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    if (left < j)
        SortItems(left, j);
    if (i < right)
        SortItems(i, right);
}

void CKeyConflict::PrepareConflictIteration(CDialogData* dialog)
{
    if (Keys.Count > 0)
    {
        SortItems(0, Keys.Count - 1);

        for (int i = 0; i < Keys.Count; i++)
            Keys[i].Conflict = FALSE;
        for (int i = 0; i < Keys.Count; i++)
        {
            for (int j = i + 1; j < Keys.Count && Keys[i].Key == Keys[j].Key; j++)
            {
                if (dialog == NULL ||
                    !Data.IgnoreDlgHotkeysConflict(dialog->ID, dialog->Controls[Keys[i].Index]->ID,
                                                   dialog->Controls[Keys[j].Index]->ID))
                {
                    Keys[i].Conflict = TRUE;
                    Keys[j].Conflict = TRUE;
                }
            }
        }
    }

    Iterator = 0;
}

BOOL CKeyConflict::GetNextConflict(WORD* index, WORD* lvIndex, WORD* controlID, DWORD* param1, DWORD* param2,
                                   DWORD* param3, DWORD* param4)
{
    if (Iterator >= Keys.Count)
        return FALSE;

    for (int i = Iterator; i < Keys.Count; i++)
    {
        if (Keys[i].Conflict)
        {
            *index = Keys[i].Index;
            *lvIndex = Keys[i].LVIndex;
            *controlID = Keys[i].ControlID;
            if (param1 != NULL)
                *param1 = Keys[i].Param1;
            if (param2 != NULL)
                *param2 = Keys[i].Param2;
            if (param3 != NULL)
                *param3 = Keys[i].Param3;
            if (param4 != NULL)
                *param4 = Keys[i].Param4;
            Iterator = i + 1;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CKeyConflict::AddKey(wchar_t key, WORD index, WORD lvIndex, WORD controlID, DWORD param1, DWORD param2, DWORD param3, DWORD param4)
{
    CKey item;
    item.Key = LOWORD(CharLowerW((wchar_t*)MAKELPARAM(key, 0)));
    item.Index = index;
    item.LVIndex = lvIndex;
    item.ControlID = controlID;
    item.Conflict = FALSE;
    item.Param1 = param1;
    item.Param2 = param2;
    item.Param3 = param3;
    item.Param4 = param4;
    Keys.Add(item);
    return Keys.IsGood();
}

BOOL IsAlphaW(wchar_t ch)
{
    return IsCharAlphaW(ch) || IsCharAlphaNumericW(ch);
}

BOOL CKeyConflict::AddString(const wchar_t* string, WORD index, WORD lvIndex, WORD controlID, DWORD param1, DWORD param2, DWORD param3, DWORD param4)
{
    const wchar_t* p = string;
    while (*p != 0)
    {
        if (*p == L'&')
        {
            if (*(p + 1) != L'&')
            {
                if (IsAlphaW(*(p + 1)))
                {
                    return AddKey(*(p + 1), index, lvIndex, controlID, param1, param2, param3, param4);
                }
            }
            else
            {
                p++;
            }
        }
        p++;
    }
    return TRUE;
}

BOOL CKeyConflict::HasKey(wchar_t key)
{
    wchar_t lower = (wchar_t)(UINT_PTR)CharLowerW((LPWSTR)key);
    for (int i = 0; i < Keys.Count; i++)
    {
        if (Keys[i].Key == lower)
            return TRUE;
    }
    return FALSE;
}

void CKeyConflict::GetAvailableKeys(const wchar_t* string, wchar_t* availableKeys, int availableKeysSize)
{
    int index = 0;
    const wchar_t* p = string;
    BOOL allowAllHotkeys = FALSE; // TRUE = search for hotkeys among all letters and digits, FALSE = only among a-z and digits

    for (int x = 0; x < 2; x++)
    {
        while (*p != 0 && index < availableKeysSize - 1)
        {
            if (*p == L'\\' && *(p + 1) == L't') // do not include hotkeys in the hint
                break;
            wchar_t ch = (wchar_t)(UINT_PTR)CharLowerW((LPWSTR)*p);
            if (allowAllHotkeys && IsAlphaW(ch) || ch >= L'a' && ch <= L'z')
            {
                BOOL alreadyAvailable = FALSE;
                for (int i = 0; i < index; i++)
                {
                    if (availableKeys[i] == ch)
                    {
                        alreadyAvailable = TRUE;
                        break;
                    }
                }

                if (!alreadyAvailable && !HasKey(ch))
                {
                    availableKeys[index] = ch;
                    index++;
                }
            }
            p++;
        }
        if (index > 0)
            break;
        else
        {
            p = string;
            allowAllHotkeys = TRUE;
        }
    }
    availableKeys[index] = 0;
}

// Return TRUE if the text points to one of the control characters \n, \r, \t

BOOL IsControlCharacter(const wchar_t* text)
{
    if (*text == L'\\')
    {
        text++;
        if (*text == L'n' || *text == L'r' || *text == L't')
            return TRUE;
    }
    return FALSE;
}

// Strings in TRANSLATOR are stored in source form, e.g. CR as two characters: '\\' and 'r'
// therefore we effectively handle escape sequences twice
BOOL SkipEscapeSeqInPlurStr(const wchar_t*& s, BOOL* error)
{
    if (*s == L'\\')
    {
        if (*(s + 1) == L'r' || *(s + 1) == L'n' || *(s + 1) == L't')
        { // in the .res there is a single character CR, LF, or TAB
            s += 2;
        }
        else
        {
            if (*(s + 1) == L'\\' && (*(s + 2) == L'|' || *(s + 2) == L'{' || *(s + 2) == L'}' || *(s + 2) == L':'))
            { // in the .res there are two characters: \|, \{, \: or \} (escape sequences for |, {, and } in a plural string)
                s += 3;
            }
            else
            {
                if (*(s + 1) == L'\\' && *(s + 2) == L'\\' && *(s + 3) == L'\\')
                { // in the .res there are two characters: \\ (escape sequence for \ in a plural string)
                    s += 4;
                }
                else
                {
                    if (*(s + 1) == L'\\')
                    { // in the .res there is a single backslash character
                        s += 2;
                    }
                    else
                    {
                        OutWindow.AddLine(L"Plural string contains invalid escape sequention.", mteError);
                        *error = TRUE;
                    }
                }
            }
        }
        return TRUE;
    }
    else
        return FALSE;
}

const wchar_t* FindPluralStrPar(const wchar_t* s, BOOL* error)
{
    while (*s != 0)
    {
        if (SkipEscapeSeqInPlurStr(s, error))
        {
            if (*error)
                return s; // invalid escape sequence
        }
        else
        {
            if (*s == L'{')
                return s; // found the start of a parameter
            s++;
        }
    }
    return s; // not found, return the end of the string
}

BOOL IsPrintfCharacter(const wchar_t* text, int* length);

const wchar_t* FindPluralStrParEnd(const wchar_t* s, BOOL* error, BOOL calcNumOfParams,
                                   int* actParIndex, int* numOfParams, BOOL* parUsedArr,
                                   int parUsedArrCount)
{
    wchar_t buff[300];

    s++; // skip L'{'

    const wchar_t* parInd = s;
    int parIndVal = 0;
    while (*parInd >= L'0' && *parInd <= L'9')
        parIndVal = 10 * parIndVal + *parInd++ - L'0';
    if (*parInd == L':' && parInd > s) // an explicit index is present, use it
    {
        if (parIndVal >= 1 && (calcNumOfParams || parIndVal <= *numOfParams))
        {
            if (calcNumOfParams && parIndVal > *numOfParams)
                *numOfParams = parIndVal;
            if (!calcNumOfParams)
            {
                if (parIndVal - 1 < parUsedArrCount)
                    parUsedArr[parIndVal - 1] = TRUE;
                else
                {
                    swprintf_s(buff, L"Unexpected number of parameters used in plural string (over %d).", parUsedArrCount);
                    OutWindow.AddLine(buff, mteError);
                    *error = TRUE;
                    return s; // error, indexes start from one
                }
            }
            s = parInd + 1;
        }
        else
        {
            if (parIndVal < 1)
                OutWindow.AddLine(L"Index of parameter for variable part in plural string must be at least 1.", mteError);
            else
            {
                swprintf_s(buff, L"Index of parameter for variable part in plural string must be at most number of parameters (%d).",
                           *numOfParams);
                OutWindow.AddLine(buff, mteError);
            }
            *error = TRUE;
            return s; // error, indexes start from one
        }
    }
    else
    {
        (*actParIndex)++;
        if (calcNumOfParams && *actParIndex > *numOfParams)
            *numOfParams = *actParIndex;
        if (!calcNumOfParams && *actParIndex > *numOfParams)
        {
            swprintf_s(buff, L"Translated plural string has too many variable parts without specified "
                             L"index of parameter, original plural string uses only %d parameters.",
                       *numOfParams);
            OutWindow.AddLine(buff, mteError);
            *error = TRUE;
            return s; // error, the implicit index exceeded the number of parameters in the original
        }
        if (!calcNumOfParams)
        {
            if (*actParIndex - 1 < parUsedArrCount)
                parUsedArr[*actParIndex - 1] = TRUE;
            else
            {
                swprintf_s(buff, L"Unexpected number of parameters used in plural string (over %d).", parUsedArrCount);
                OutWindow.AddLine(buff, mteError);
                *error = TRUE;
                return s; // error, indexes start from one
            }
        }
    }

    while (*s != 0 && *s != L'}')
    {
        // Skip text
        while (*s != L'}' && *s != 0 && *s != '|')
        {
            if (IsControlCharacter(s))
            {
                OutWindow.AddLine(L"Plural string cannot contain control chars in its variable part.", mteError);
                *error = TRUE;
                return s; // these sequences can only be verified on the expanded string; hopefully we never need to do that, because validating every variant would be painful
            }
            if (SkipEscapeSeqInPlurStr(s, error))
            {
                if (*error)
                    return s; // invalid escape sequence
            }
            else
            {
                int len;
                if (IsPrintfCharacter(s, &len))
                {
                    OutWindow.AddLine(L"Plural string cannot contain format specifiers in its variable part.", mteError);
                    *error = TRUE;
                    return s; // these sequences can only be verified on the expanded string; hopefully we never need to do that, because validating every variant would be painful
                }
                s++;
            }
        }
        if (*s == L'|')
        {
            s++;

            // Skip the numeric bound
            const wchar_t* numBeg = s;
            while (*s != L'}' && *s != 0 && *s != L'|')
            {
                if (*s < L'0' || *s > L'9')
                {
                    OutWindow.AddLine(L"Plural string contains interval bound which is not a decimal number.", mteError);
                    *error = TRUE;
                    return s; // error, only a decimal number is expected here
                }
                s++;
            }
            if (s > numBeg && *s == L'|')
                s++; // the bound must not be empty and has to end with L'|'
            else
            {
                if (s == numBeg)
                    OutWindow.AddLine(L"Plural string cannot contain empty interval bound.", mteError);
                else
                    OutWindow.AddLine(L"Plural string's variable part cannot end with interval bound (text variant must follow).", mteError);
                *error = TRUE;
                return s; // error: either the bound number is missing or there is no text after L'|'
            }
        }
    }
    return s;
}

// Compare the strings and return TRUE if neither is a plural string
// or if both are plural strings with the same number of parameters
// and are syntactically correct
BOOL ValidatePluralStrings(const wchar_t* original, const wchar_t* translated)
{
    wchar_t buff[300];

    BOOL isOrigPlural = *original == L'{' && *(original + 1) == L'!' && *(original + 2) == L'}';
    BOOL isTranPlural = *translated == L'{' && *(translated + 1) == L'!' && *(translated + 2) == L'}';
    if (isOrigPlural != isTranPlural)
        return FALSE; // one is plural and the other is not—that is definitely wrong
    if (isOrigPlural)
    {
        // skip signature
        original += 3;
        translated += 3;

        BOOL error = FALSE;
        int numOfParams = 0;
        int actParIndex = 0;
        while (*original != 0)
        {
            original = FindPluralStrPar(original, &error);
            if (error)
                return FALSE; // invalid escape sequence
            if (*original == 0)
                break; // done
            original = FindPluralStrParEnd(original, &error, TRUE, &actParIndex, &numOfParams, NULL, 0);
            if (error || *original != L'}')
            {
                if (!error)
                    OutWindow.AddLine(L"Original plural string contains variable part which is not closed by '}'.", mteError);
                return FALSE; // invalid escape sequence or parameters are not closed by '}'
            }
            original++;
        }
        actParIndex = 0;
        BOOL parUsedArr[100];
        memset(parUsedArr, 0, sizeof(parUsedArr));
        while (*translated != 0)
        {
            translated = FindPluralStrPar(translated, &error);
            if (error)
                return FALSE; // invalid escape sequence
            if (*translated == 0)
                break; // done
            translated = FindPluralStrParEnd(translated, &error, FALSE, &actParIndex, &numOfParams, parUsedArr, 100);
            if (error || *translated != L'}')
            {
                if (!error)
                    OutWindow.AddLine(L"Plural string contains variable part which is not closed by '}'.", mteError);
                return FALSE; // invalid escape sequence or parameters are not closed by '}'
            }
            translated++;
        }
        for (int i = 0; i < 100 && i < numOfParams; i++)
        {
            if (!parUsedArr[i])
            {
                swprintf_s(buff, L"Plural string does not use all parameters, first unused parameter index is %d.", i + 1);
                OutWindow.AddLine(buff, mteError);
                return FALSE; // unused parameter—most likely an error
            }
        }
    }
    return TRUE;
}

// Compare the strings and return TRUE if they contain the same number and order
// of control characters \r \n \t
BOOL ValidateControlCharacters(const wchar_t* original, const wchar_t* translated)
{
    const wchar_t* oIter = original;
    const wchar_t* tIter = translated;
    wchar_t oFound = 0;
    wchar_t tFound = 0;
    while (*oIter != 0 || *tIter != 0)
    {
        while (*oIter != 0 && !IsControlCharacter(oIter))
            oIter++;
        if (*oIter != 0)
        {
            oFound = *(oIter + 1);
            oIter += 2;
        }
        else
            oFound = 0;

        while (*tIter != 0 && !IsControlCharacter(tIter))
            tIter++;
        if (*tIter != 0)
        {
            tFound = *(tIter + 1);
            tIter += 2;
        }
        else
            tFound = 0;

        if (oFound != tFound)
            return FALSE;
    }
    return TRUE;
}

// Check whether the number of commas matches in the original and translation (the number of values in a CSV string)
BOOL ValidateStringWithCSV(const wchar_t* original, const wchar_t* translated)
{
    const wchar_t* oIter = original;
    const wchar_t* tIter = translated;
    int oCount = 0;
    int tCount = 0;
    while (*oIter != 0)
        if (*oIter++ == L',')
            oCount++;
    while (*tIter != 0)
        if (*tIter++ == L',')
            tCount++;
    return oCount == tCount;
}

// Return TRUE if the text pointer references a printf control specifier
// and fill the length variable

BOOL IsPrintfCharacter(const wchar_t* text, int* length)
{
    const wchar_t* p = text;
    if (*p == L'%')
    {
        p++;
        if (*p == L'%') // escape sequence for '%'
        {
            *length = p - text + 1;
            return TRUE;
        }
        while (*p == L'-' || *p == L'+' || *p == L'#' || *p == L'0' || *p == L' ') // flags
            p++;
        while (*p >= L'0' && *p <= L'9') // width
            p++;
        if (*p == L'*') // width
            p++;
        if (*p == L'.') // precision
        {
            p++;
            while (*p >= L'0' && *p <= L'9')
                p++;
            if (*p == L'*')
                p++;
        }
        if (*p == L'I' && *(p + 1) == L'6' && *(p + 2) == L'4') // type prefix
            p += 3;
        else
        {
            if (*p == L'I' && *(p + 1) == L'3' && *(p + 2) == L'2') // type prefix
                p += 3;
            else
            {
                if (*p == L'l' && *(p + 1) == L'l') // type prefix
                    p += 2;
                else
                {
                    if (*p == L'h' || *p == L'l' || *p == L'w' || *p == L'I') // type prefix
                        p++;
                }
            }
        }
        if (*p == L'a' || *p == L'A' || *p == L'c' || *p == L'C' || *p == L'd' || *p == L'e' ||
            *p == L'E' || *p == L'f' || *p == L'g' || *p == L'G' || *p == L'i' || *p == L'o' || *p == L'p' ||
            *p == L's' || *p == L'S' || *p == L'u' || *p == L'x' || *p == L'X' || *p == L'Z') // type
        {
            *length = p - text + 1;
            return TRUE;
        }
    }
    return FALSE;
}

// Compare the strings and return TRUE if they contain the same number and order
// of printf format specifiers
BOOL ValidatePrintfSpecifier(const wchar_t* original, const wchar_t* translated)
{
    const wchar_t* oIter = original;
    const wchar_t* tIter = translated;
    int len;
    wchar_t oFound[300];
    wchar_t tFound[300];
    while (*oIter != 0 || *tIter != 0)
    {
        while (*oIter != 0 && !IsPrintfCharacter(oIter, &len))
            oIter++;
        if (*oIter != 0)
        {
            memcpy(oFound, oIter, len * 2);
            oFound[len] = 0;
            oIter += len;
        }
        else
            oFound[0] = 0;

        while (*tIter != 0 && !IsPrintfCharacter(tIter, &len))
            tIter++;
        if (*tIter != 0)
        {
            memcpy(tFound, tIter, len * 2);
            tFound[len] = 0;
            tIter += len;
        }
        else
            tFound[0] = 0;

        if (wcscmp(oFound, tFound) != 0)
            return FALSE;
    }
    return TRUE;
}

// Return FALSE if exactly one string contains a hotkey
// otherwise return TRUE
BOOL ValidateHotKeys(const wchar_t* original, const wchar_t* translated)
{
    const wchar_t* oIter = original;
    const wchar_t* tIter = translated;

    // Number of hotkeys
    int oCount = 0;
    int tCount = 0;

    while (*oIter != 0)
    {
        if (*oIter == L'&')
        {
            if (*(oIter + 1) != L'&')
                oCount++;
            else
                oIter++;
        }
        oIter++;
    }

    while (*tIter != 0)
    {
        if (*tIter == L'&')
        {
            if (*(tIter + 1) != L'&')
                tCount++;
            else
                tIter++;
        }
        tIter++;
    }

    if (oCount != tCount)
        return FALSE;

    return TRUE;
}

// Return FALSE if the strings start with a different number of spaces
// otherwise return TRUE
BOOL ValidateTextBeginning(const wchar_t* original, const wchar_t* translated)
{
    const wchar_t* oIter = original;
    const wchar_t* tIter = translated;

    int oCount = 0;
    int tCount = 0;
    while (*oIter == L' ')
    {
        oIter++;
        oCount++;
    }
    while (*tIter == L' ')
    {
        tIter++;
        tCount++;
    }
    return oCount == tCount;
}

#define IDEOGRAPHIC_PERIOD L'\x3002' // sentence period in Simplified Chinese (and likely other logographic scripts)

// Check whether both string endings use the same trailing sequence (spaces, dots, colons)
// Return TRUE if the endings match; otherwise return FALSE
BOOL ValidateTextEnding(const wchar_t* original, const wchar_t* translated, WORD strID)
{
    // Petr: also add a test that one text is not empty while the other is not;
    // that would also be an error and falls under mismatching beginnings/endings
    const wchar_t* oWS = original;
    while (*oWS != 0 && (*oWS == ' ' || *oWS == '\t'))
        oWS++;
    const wchar_t* tWS = translated;
    while (*tWS != 0 && (*tWS == ' ' || *tWS == '\t'))
        tWS++;
    if (*oWS == 0 && *tWS != 0 || *oWS != 0 && *tWS == 0)
        return FALSE;

    int oLen = wcslen(original);
    int tLen = wcslen(translated);

    const wchar_t* oIter = original + oLen - 1;
    const wchar_t* tIter = translated + tLen - 1;

    BOOL ignoreMissingColonAtEnd = FALSE;
    if (strID != 0)
        ignoreMissingColonAtEnd = Data.IgnoreMissingColonAtEnd(strID);

    BOOL oCont, tCont;
    do
    {
        oCont = (oIter >= original && (*oIter == L'.' || *oIter == L':' || *oIter == L' ' || *oIter == IDEOGRAPHIC_PERIOD));
        tCont = (tIter >= translated && (*tIter == L'.' || *tIter == L':' || *tIter == L' ' || *tIter == IDEOGRAPHIC_PERIOD));
        if (ignoreMissingColonAtEnd && oCont && *oIter == L':' && (!tCont || *tIter != L':'))
        {
            ignoreMissingColonAtEnd = FALSE; // ignore only a single missing colon
            oIter--;
            oCont = (oIter >= original && (*oIter == L'.' || *oIter == L':' || *oIter == L' ' || *oIter == IDEOGRAPHIC_PERIOD));
        }
        if (oCont && tCont)
        {
            wchar_t oCh = *oIter;
            wchar_t tCh = *tIter;
            if (oCh == IDEOGRAPHIC_PERIOD)
                oCh = L'.'; // IDEOGRAPHIC_PERIOD and L'.' are equivalent for our purposes
            if (tCh == IDEOGRAPHIC_PERIOD)
                tCh = L'.';
            if (oCh != tCh)
                return FALSE;
        }
        oIter--;
        tIter--;
    } while (oCont && tCont);
    return (oCont == tCont);
}

BOOL IsTranslatableControl(const wchar_t* text)
{
    if (wcslen(text) == 0)
        return FALSE;

    //  if (_wcsicmp(text, L"_dummy_") == 0)
    //    return FALSE;

    BOOL foundChar = FALSE;
    const wchar_t* iter = text;
    while (*iter != 0)
    {
        if (*iter != L' ')
        {
            foundChar = TRUE;
            break;
        }
        iter++;
    }

    return foundChar;
}

BOOL DoesControlsOverlap(WORD dialogID, const CControl* ctrl1, const CControl* ctrl2)
{
    RECT r1;
    int r1Height = ctrl1->TCY;
    if (ctrl1->IsComboBox()) // For combo boxes we want their "collapsed" size
        r1Height = COMBOBOX_BASE_HEIGHT;
    r1.left = ctrl1->TX;
    r1.top = ctrl1->TY;
    BOOL isSmallIcon = ctrl1->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialogID, ctrl1->ID);
    r1.right = r1.left + (isSmallIcon ? 10 : ctrl1->TCX);
    r1.bottom = r1.top + (isSmallIcon ? 10 : r1Height);

    RECT r2;
    int r2Height = ctrl2->TCY;
    if (ctrl2->IsComboBox()) // For combo boxes we want their "collapsed" size
        r2Height = COMBOBOX_BASE_HEIGHT;
    r2.left = ctrl2->TX;
    r2.top = ctrl2->TY;
    isSmallIcon = ctrl2->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialogID, ctrl2->ID);
    r2.right = r2.left + (isSmallIcon ? 10 : ctrl2->TCX);
    r2.bottom = r2.top + (isSmallIcon ? 10 : r2Height);

    RECT dummy;
    return IntersectRect(&dummy, &r1, &r2);
}

BOOL DoesGroupBoxLabelOverlap(BOOL control1IsCheckOrRadioBox, const CControl* control1, const CControl* control2)
{
    return control1IsCheckOrRadioBox && control2->IsGroupBox() &&                                  // solve whether the checkbox/radio label overlaps the group box
           control1->TY <= control2->TY + 4 && control1->TY + control1->TCY >= control2->TY + 5 && // +4 and +5 determined experimentally (checkbox/radio overlaps the top edge vertically)
           control1->TX > control2->TX && control1->TX < control2->TX + control2->TCX &&           // checkbox/radio overlaps the group box horizontally
           control1->TX + control1->TCX > control2->TX + control2->TCX - 3;                        // checkbox/radio ends too far and damages the upper-right corner of the group box
}

BOOL ShouldValidateLayoutFor(const CControl* control)
{
    // Ignore group boxes
    if (control->IsGroupBox())
        return FALSE;

    // A short static control without text represents a horizontal separator
    if ((DWORD)control->ClassName == 0x0082ffff && (control->TCY < 2) && (control->TWindowName[0] == 0))
        return FALSE;

    return TRUE;
}

BOOL IsRadioOrCheckBox(int bt)
{
    CButtonType b = GetButtonType(bt);
    return b == btCheck || b == btRadio;
}

BOOL IsRadioBox(int bt)
{
    CButtonType b = GetButtonType(bt);
    return b == btRadio;
}

BOOL IsCheckBox(int bt)
{
    CButtonType b = GetButtonType(bt);
    return b == btCheck;
}

BOOL IsPushButton(int bt)
{
    CButtonType b = GetButtonType(bt);
    return b == btPush;
}

BOOL IsStaticLeftText(int ss)
{
    ss = (ss & SS_TYPEMASK);
    return ss == SS_LEFT || ss == SS_SIMPLE || ss == SS_LEFTNOWORDWRAP;
}

BOOL ShouldValidateAlignment(CDialogData* dialog, const CControl* control1,
                             const CControl* control2, BOOL* checkLeft,
                             BOOL* checkRight, BOOL* checkVertical)
{
    *checkLeft = FALSE;
    *checkRight = FALSE;
    *checkVertical = TRUE;

    if ((DWORD)control1->ClassName == 0x0082ffff &&
        (DWORD)control2->ClassName == 0x0082ffff &&
        control1->TCY == 1 && control2->TCY == 1) // check horizontal lines on both sides
    {
        *checkLeft = control1->TY < 20 && control2->TY < 20; // only if they start at the dialog edge; otherwise they likely continue after a static text and validating makes no sense
        *checkRight = TRUE;
        return TRUE;
    }

    // Align "left text" statics and radio/check boxes to the left edge
    if (((DWORD)control1->ClassName == 0x0080ffff && IsRadioOrCheckBox(control1->Style) ||
         (DWORD)control1->ClassName == 0x0082ffff && IsStaticLeftText(control1->Style) &&
             control1->TCY > 1 && !Data.IgnoreStaticItIsProgressBar(dialog->ID, control1->ID)) &&
        ((DWORD)control2->ClassName == 0x0080ffff && IsRadioOrCheckBox(control2->Style) ||
         (DWORD)control2->ClassName == 0x0082ffff && IsStaticLeftText(control2->Style) &&
             control2->TCY > 1 && !Data.IgnoreStaticItIsProgressBar(dialog->ID, control2->ID)))
    {
        *checkLeft = TRUE;
        if (control1->ClassName != control2->ClassName)
            *checkVertical = FALSE;
        return !control1->IsTContainedIn(control2) && !control2->IsTContainedIn(control1);
    }

    if (control1->ClassName != control2->ClassName)
        return FALSE;

    if ((DWORD)control1->ClassName == 0x0080ffff) // BUTTON
    {
        CButtonType b1 = GetButtonType(control1->Style);
        CButtonType b2 = GetButtonType(control2->Style);
        *checkLeft = TRUE; // always test the left side for buttons (hopefully no right-aligned checkboxes/radios)
        *checkRight = b1 == btPush && b2 == btPush ||
                      b1 == btGroup && b2 == btGroup; // if it is a push button or group box, verify the right edge as well
        *checkVertical = *checkRight;
        return *checkRight || (control1->Style & BS_TYPEMASK) == (control2->Style & BS_TYPEMASK);
    }

    if ((DWORD)control1->ClassName == 0x0082ffff) // STATIC or progress bar
    {
        if (Data.IgnoreStaticItIsProgressBar(dialog->ID, control1->ID)) // progress bar
        {
            *checkLeft = TRUE;
            *checkRight = TRUE;
            *checkVertical = TRUE;
            return Data.IgnoreStaticItIsProgressBar(dialog->ID, control2->ID); // only if the other one is also a progress bar
        }
        DWORD ssStyle = (control1->Style & SS_TYPEMASK);
        *checkLeft = IsStaticLeftText(control1->Style); // test the left edge only if it is left-aligned
        *checkRight = (ssStyle == SS_RIGHT);            // test the right edge only if it is right-aligned
        return ((control1->Style & SS_TYPEMASK) == (control2->Style & SS_TYPEMASK) ||
                IsStaticLeftText(control1->Style) && IsStaticLeftText(control2->Style)) &&
               control1->TCY > 1 && control2->TCY > 1 &&                    // exclude horizontal lines
               !Data.IgnoreStaticItIsProgressBar(dialog->ID, control2->ID); // exclude progress bars
    }

    if ((DWORD)control1->ClassName == 0x0081ffff) // EDIT
    {
        if ((control1->Style & WS_BORDER) != (control2->Style & WS_BORDER))
            return FALSE; // aligning edits with and without borders makes no sense
        *checkLeft = TRUE;
        *checkRight = (control1->Style & WS_BORDER) != 0; // test the right edge only if it has a border (otherwise the edge is not visible)
        return TRUE;
    }

    // Validate all remaining controls strictly; we can add exceptions later, see the BUTTON and STATIC blocks
    *checkLeft = TRUE;
    *checkRight = TRUE;
    return TRUE;
}

BOOL ShouldValidateSize(const CControl* control1, const CControl* control2)
{
    if (control1->ClassName != control2->ClassName)
        return FALSE;

    if ((DWORD)control1->ClassName == 0x0080ffff) // BUTTON
    {
        CButtonType b1 = GetButtonType(control1->Style);
        CButtonType b2 = GetButtonType(control2->Style);
        return b1 == btPush && b2 == btPush ||
               b1 == btGroup && b2 == btGroup; // if it is a push button or group box
    }

    if ((DWORD)control1->ClassName == 0x0081ffff) // EDIT
    {
        if ((control1->Style & WS_BORDER) != (control2->Style & WS_BORDER))
            return FALSE; // Do not align edit boxes with and without borders—it makes no sense
        return TRUE;
    }

    if ((DWORD)control1->ClassName == 0x0085ffff) // COMBOBOX
    {
        return TRUE;
    }

    return FALSE;
}

enum EnumSpacingControlType
{
    esctPushButton,
    esctCheckBox,
    esctRadioButton,
    //  esctStatic, if we ever wanted to go overboard
    esctEditBox,
    esctComboBox,
    esctNone
};

EnumSpacingControlType GetSpacingControlType(const CControl* control)
{
    if ((DWORD)control->ClassName == 0x0080ffff) // BUTTON
    {
        switch (GetButtonType(control->Style))
        {
        case btPush:
            return esctPushButton;
        case btCheck:
            return esctCheckBox;
        case btRadio:
            return esctRadioButton;
        }
    }

    /*
  if ((DWORD)control->ClassName == 0x0082ffff) // STATIC
  {
    return esctStatic;
  }
  */
    if ((DWORD)control->ClassName == 0x0081ffff) // EDIT
    {
        return esctEditBox;
    }
    if ((DWORD)control->ClassName == 0x0085ffff) // COMBO
    {
        return esctComboBox;
    }

    return esctNone;
}

BOOL HasDifferentSpacing(CDialogData* dialogData, int controlIndex)
{
    const CControl* control = dialogData->Controls[controlIndex];
    // Determine whether this control should be checked for spacing
    EnumSpacingControlType ctrlType = GetSpacingControlType(control);
    if (ctrlType != esctNone)
    {
        BOOL checkHoriz = ctrlType != esctRadioButton && ctrlType != esctCheckBox; // elements without a frame should not be aligned horizontally

        RECT outline;
        outline.left = control->TX;
        outline.top = control->TY;
        outline.right = control->TX + control->TCX;
        int ctrlH = control->TCY;
        if (control->IsComboBox()) // for combo boxes we want their "collapsed" size
            ctrlH = COMBOBOX_BASE_HEIGHT;
        outline.bottom = control->TY + ctrlH;

#define OUTERSPACEMAX 1000000
        RECT outerSpace;
        outerSpace.left = OUTERSPACEMAX;
        outerSpace.top = OUTERSPACEMAX;
        outerSpace.right = OUTERSPACEMAX;
        outerSpace.bottom = OUTERSPACEMAX;

        // Look for distances to the same control types (in four directions)

        // Second phase: from the control in each direction
        for (int i = 1; i < dialogData->Controls.Count; i++) // 0 = dialog title
        {
            const CControl* control2 = dialogData->Controls[i];
            if (control != control2 && ctrlType == GetSpacingControlType(control2)) // find the same type and avoid ourselves
            {
                int ctrlX = control2->TX;
                int ctrlW = control2->TCX;
                int ctrlY = control2->TY;
                ctrlH = control2->TCY;
                if (control2->IsComboBox()) // for combo boxes we want their "collapsed" size
                    ctrlH = COMBOBOX_BASE_HEIGHT;

                // WARNING: similar code lives in GetOuterSpace() and GetMutualControlsPosition()

                // The control is vertically within the band's range
                if ((ctrlY >= outline.top && ctrlY < outline.bottom) ||                  // top edge lies inside the group
                    (ctrlY + ctrlH >= outline.top && ctrlY + ctrlH <= outline.bottom) || // bottom edge lies inside the group
                    (ctrlY < outline.top && ctrlY + ctrlH > outline.bottom))             // spans above and below the group (taller than the band)
                {
                    if ((ctrlX + ctrlW <= outline.left) && (outline.left - (ctrlX + ctrlW) < outerSpace.left))
                    {
                        outerSpace.left = outline.left - (ctrlX + ctrlW);
                    }
                    if ((ctrlX >= outline.right) && (ctrlX - outline.right < outerSpace.right))
                    {
                        outerSpace.right = ctrlX - outline.right;
                    }
                }
                // The control is horizontally within the band's range
                if ((ctrlX >= outline.left && ctrlX < outline.right) ||
                    (ctrlX + ctrlW >= outline.left && ctrlX + ctrlW <= outline.right) ||
                    (ctrlX < outline.left && ctrlX + ctrlW > outline.right))
                {
                    if ((ctrlY + ctrlH <= outline.top) && (outline.top - (ctrlY + ctrlH) < outerSpace.top))
                    {
                        outerSpace.top = outline.top - (ctrlY + ctrlH);
                    }
                    if ((ctrlY >= outline.bottom) && (ctrlY - outline.bottom < outerSpace.bottom))
                    {
                        outerSpace.bottom = ctrlY - outline.bottom;
                    }
                }
            }
        }
#define DIFFSPACING_MAXSPACE 2
        if (checkHoriz && outerSpace.left != OUTERSPACEMAX && outerSpace.right != OUTERSPACEMAX && outerSpace.left != outerSpace.right && abs(outerSpace.left - outerSpace.right) <= DIFFSPACING_MAXSPACE ||
            outerSpace.top != OUTERSPACEMAX && outerSpace.bottom != OUTERSPACEMAX && outerSpace.top != outerSpace.bottom && abs(outerSpace.top - outerSpace.bottom) <= DIFFSPACING_MAXSPACE)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void GetMutualControlsPosition(const CControl* control1, const CControl* control2, BOOL* inRow, BOOL* inCol)
{
    int ctrl1X = control1->TX;
    int ctrl1W = control1->TCX;
    int ctrl1Y = control1->TY;
    int ctrl1H = control1->TCY;
    if (control1->IsComboBox()) // for combo boxes we want their "collapsed" size
        ctrl1H = COMBOBOX_BASE_HEIGHT;

    int ctrl2X = control2->TX;
    int ctrl2W = control2->TCX;
    int ctrl2Y = control2->TY;
    int ctrl2H = control2->TCY;
    if (control2->IsComboBox()) // for combo boxes we want their "collapsed" size
        ctrl2H = COMBOBOX_BASE_HEIGHT;

    // The control is vertically within the outline band
    *inRow = ((ctrl2Y >= ctrl1Y && ctrl2Y < ctrl1Y + ctrl1H) ||                   // second control's top edge lies inside the first
              (ctrl2Y + ctrl2H > ctrl1Y && ctrl2Y + ctrl2H <= ctrl1Y + ctrl1H) || // second control's bottom edge lies inside the first
              (ctrl1Y < ctrl2Y && ctrl1Y + ctrl1H > ctrl2Y + ctrl2H) ||           // first control spans above and below the second
              (ctrl2Y < ctrl1Y && ctrl2Y + ctrl2H > ctrl1Y + ctrl1H));            // second control spans above and below the first
    *inCol = ((ctrl2X >= ctrl1X && ctrl2X < ctrl1X + ctrl1W) ||
              (ctrl2X + ctrl2W > ctrl1X && ctrl2X + ctrl2W <= ctrl1X + ctrl1W) ||
              (ctrl1X < ctrl2X && ctrl1X + ctrl1W > ctrl2X + ctrl2W) ||
              (ctrl2X < ctrl1X && ctrl2X + ctrl2W > ctrl1X + ctrl1W));
}

BOOL IsControlForLabelPlacingTest(const CDialogData* dialog, const CControl* control, const CControl* firstControl, int* deltaY)
{
    if ((DWORD)control->ClassName == 0x0080ffff) // BUTTON
    {
        switch (GetButtonType(control->Style))
        {
        case btPush:
        {
            if (firstControl != NULL)
            {
                if ((DWORD)firstControl->ClassName == 0x0080ffff) // BUTTON
                {
                    CButtonType b = GetButtonType(firstControl->Style);
                    if (b == btCheck || b == btRadio) // Checkbox or radio button before the button
                    {
                        *deltaY = (control->TCY - 12) / 2;
                        return TRUE;
                    }
                }

                if ((DWORD)firstControl->ClassName == 0x0082ffff) // STATIC
                {
                    DWORD ss = (firstControl->Style & SS_TYPEMASK);
                    if (ss == SS_LEFT || ss == SS_RIGHT || ss == SS_CENTER || ss == SS_SIMPLE || ss == SS_LEFTNOWORDWRAP)
                    { // Static before the button
                        *deltaY = (control->TCY - 12) / 2;
                        return TRUE;
                    }
                }

                if ((DWORD)firstControl->ClassName == 0x0081ffff && // EDIT
                    (firstControl->Style & WS_BORDER) != 0)         // With a border
                {
                    *deltaY = (control->TCY - firstControl->TCY) / 2; // Button after an edit box (likely a Browse button) — align the tops (edits always have *deltaY == 0)
                    return TRUE;
                }

                *deltaY = 2;
                return TRUE;
            }
            return FALSE; // We do not check buttons used as labels (needed for font changes, but we'll ignore it)
        }

        case btCheck:
        {
            *deltaY = (control->TCY - 12) / 2;
            return TRUE;
        }

        case btRadio:
        {
            *deltaY = (control->TCY - 12) / 2;
            return TRUE;
        }
        }
    }

    if ((DWORD)control->ClassName == 0x0082ffff) // STATIC a progress bar
    {
        if (Data.IgnoreStaticItIsProgressBar(dialog->ID, control->ID)) // progress bar
        {
            *deltaY = -1;
            return TRUE;
        }
        DWORD ss = (control->Style & SS_TYPEMASK);
        if (ss == SS_LEFT || ss == SS_RIGHT || ss == SS_CENTER || ss == SS_SIMPLE || ss == SS_LEFTNOWORDWRAP)
        {
            *deltaY = -2;
            return TRUE;
        }
    }

    if (firstControl != NULL && LOWORD(control->ClassName) != 0xFFFF && wcscmp(control->ClassName, L"SysDateTimePick32") == 0) // DATEPICK
    {
        *deltaY = 0;
        return TRUE;
    }

    if (firstControl != NULL && LOWORD(control->ClassName) != 0xFFFF && wcscmp(control->ClassName, L"msctls_progress32") == 0) // progress
    {
        *deltaY = (control->TCY - 12) / 2;
        return TRUE;
    }

    if (firstControl != NULL && (DWORD)control->ClassName == 0x0085ffff) // COMBO
    {
        *deltaY = 0;
        return TRUE;
    }

    if ((DWORD)control->ClassName == 0x0081ffff) // EDIT
    {
        BOOL hasBorder = ((control->Style & WS_BORDER) != 0);
        if (hasBorder)
            *deltaY = 0;
        else
            *deltaY = -2;
        return TRUE;
    }

    return FALSE;
}

BOOL IsNotLeftStaticFollowedByLine(const CControl* control, CDialogData* dialogData, int controlIndex)
{
    if (control->IsStaticText(TRUE) && control->TCY < 16) // Labels must be left-aligned statics; only single-line labels are considered
    {
        for (int i = 0; i < 2; i++)
        {
            const CControl* control2 = NULL;
            if (i == 0 && controlIndex < dialogData->Controls.Count - 1)
                control2 = dialogData->Controls[controlIndex + 1];
            if (i == 1 && controlIndex > 0)
                control2 = dialogData->Controls[controlIndex - 1];
            if (control2 != NULL && control2->TCY == 1 &&
                control2->ClassName == (wchar_t*)0x0082FFFF &&                                   // STATIC: horizontal line
                control2->TY >= control->TY && control2->TY < control->TY + control->TCY &&      // The line sits vertically within the control
                control2->TX >= control->TX && control2->TX - (control->TX + control->TCX) < 20) // The line starts after the control, at most 20 dialog units away
            {
                return FALSE; // This is a left static followed by a line
            }
        }
    }
    return TRUE;
}

BOOL IsLabelCorrectlyPlaced(CDialogData* dialogData, int controlIndex, int* controlIndexOut, int* delta, const char** deltaAxis)
{
    const CControl* control1 = dialogData->Controls[controlIndex];
    *delta = 0;
    *deltaAxis = "horizontally";
    *controlIndexOut = 0;

    int ctrl1YOffset;
    if (IsControlForLabelPlacingTest(dialogData, control1, NULL, &ctrl1YOffset) &&
        IsNotLeftStaticFollowedByLine(control1, dialogData, controlIndex)) // A left static with a bar glued to its right cannot be a label
    {
        const CControl* control2 = NULL;
        BOOL tryingNextCtrl = FALSE;
        if (controlIndex < dialogData->Controls.Count - 1) // Try the control immediately after the label first
        {
            control2 = dialogData->Controls[controlIndex + 1];
            *controlIndexOut = controlIndex + 1;
            tryingNextCtrl = TRUE;
        }

        BOOL tryNeighborAtRight = TRUE;
        BOOL tryNeighborAtBottom = TRUE;

        int ctrl1X = control1->TX;
        int ctrl1W = control1->TCX;
        int ctrl1Y = control1->TY;
        int ctrl1H = control1->TCY;
        if (control1->IsComboBox()) // For combo boxes we want their "collapsed" size
            ctrl1H = COMBOBOX_BASE_HEIGHT;
        if (control1->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialogData->ID, control1->ID))
            ctrl1W = ctrl1H = 10;

        while (1)
        {
            if (control2 == NULL) // Try to find the control for the label solely by coordinates
            {
                if (tryNeighborAtRight)
                {
                    tryNeighborAtRight = FALSE;
                    int minDist = -1;
                    int found = -1;
                    for (int i = 0; i < dialogData->Controls.Count; i++)
                    {
                        if (i != controlIndex) // The control itself cannot be behind the label
                        {
                            const CControl* c = dialogData->Controls[i];
                            int x = c->TX;
                            int w = c->TCX;
                            int y = c->TY;
                            int h = c->TCY;
                            if (c->IsComboBox()) // For combo boxes we want their "collapsed" size
                                h = COMBOBOX_BASE_HEIGHT;
                            if (c->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialogData->ID, c->ID))
                                w = h = 10;

                            if (x >= ctrl1X + ctrl1W && y + h > ctrl1Y && y < ctrl1Y + ctrl1H) // It is to the right and overlaps the label height
                            {
                                if (minDist == -1 || minDist > x - (ctrl1X + ctrl1W)) // Use the first found control or the one closer to the label
                                {
                                    found = i;
                                    minDist = x - (ctrl1X + ctrl1W);
                                }
                            }
                        }
                    }
                    if (found != -1)
                    {
                        control2 = dialogData->Controls[found];
                        *controlIndexOut = found;
                        tryingNextCtrl = FALSE;
                    }
                }
                if (control2 == NULL && tryNeighborAtBottom)
                {
                    tryNeighborAtBottom = FALSE;
                    int minDist = -1;
                    int found = -1;
                    for (int i = 0; i < dialogData->Controls.Count; i++)
                    {
                        if (i != controlIndex) // The control itself cannot be behind the label
                        {
                            const CControl* c = dialogData->Controls[i];
                            int x = c->TX;
                            int w = c->TCX;
                            int y = c->TY;
                            int h = c->TCY;
                            if (c->IsComboBox()) // For combo boxes we want their "collapsed" size
                                h = COMBOBOX_BASE_HEIGHT;
                            if (c->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialogData->ID, c->ID))
                                w = h = 10;

                            if (y >= ctrl1Y + ctrl1H && x + w > ctrl1X && x < ctrl1X + ctrl1W) // It is below and overlaps the label width
                            {
                                if (minDist == -1 || minDist > y - (ctrl1Y + ctrl1H)) // Use the first found control or the one closer to the label
                                {
                                    found = i;
                                    minDist = y - (ctrl1Y + ctrl1H);
                                }
                            }
                        }
                    }
                    if (found != -1)
                    {
                        control2 = dialogData->Controls[found];
                        *controlIndexOut = found;
                        tryingNextCtrl = FALSE;
                    }
                }
            }
            if (control2 == NULL)
                break;

            BOOL inRow, inCol;
            GetMutualControlsPosition(control1, control2, &inRow, &inCol);

            int deltaX = control1->TX - control2->TX;
            int deltaY = control1->TY - control2->TY;

#define TOPLABELDISTANCE_MAX_X1 10
#define TOPLABELDISTANCE_MAX_X2 20
#define TOPLABELDISTANCE_MAX_Y 10
            if (!inRow && inCol && deltaY < 0 &&
                control1->IsStaticText(TRUE) && control1->TCY < 16 &&                                // Labels must be left-aligned statics; only single-line labels are considered
                control2->TY - (control1->TY + 8) <= TOPLABELDISTANCE_MAX_Y &&                       // The control must not be too far in Y from the label
                abs(deltaX) <= (tryingNextCtrl ? TOPLABELDISTANCE_MAX_X2 : TOPLABELDISTANCE_MAX_X1)) // The control must not be too far in X from the label
            {
                CButtonType bt;
                if ((DWORD)control2->ClassName == 0x0081ffff ||                                                    // EDIT
                    (DWORD)control2->ClassName == 0x0083ffff ||                                                    // LISTBOX
                    (DWORD)control2->ClassName == 0x0085ffff ||                                                    // COMBO
                    LOWORD(control2->ClassName) != 0xFFFF && wcscmp(control2->ClassName, L"SysListView32") == 0 || // LISTVIEW
                    (DWORD)control2->ClassName == 0x0080ffff &&                                                    // BUTTON: push-button + checkbox + radiobutton
                        ((bt = GetButtonType(control2->Style)) == btPush || bt == btCheck || bt == btRadio))
                {
                    // Label is above the control
#define LABEL2CONTROL_SPACING_V -2 // 2 pro space
                    int labelDY = LABEL2CONTROL_SPACING_V - 8 * (control1->TCY / 8);
                    BOOL control2IsCheckOrRadio = (DWORD)control2->ClassName == 0x0080ffff && (bt == btCheck || bt == btRadio);
                    if (control2IsCheckOrRadio)
                        labelDY += (control2->TCY - 10) / 2; // BUTTON: checkbox + radiobutton: the Y offset depends on button height (typically 10 or 12)
                    if (deltaY != labelDY &&
                        !Data.IgnoreProblem(iltIncorPlLbl, dialogData->ID, control1->ID, control2->ID))
                    {
                        *delta = deltaY - labelDY;
                        *deltaAxis = "vertically";
                        return FALSE;
                    }
#define LABEL2CHECKBOX_H_ALIGN -7 // Allow labels of push-button/checkbox/radio columns to be offset by exactly 7 dialog units (used by PictView; without the offset it looks ugly)
                    BOOL control2IsPushOrCheckOrRadio = (DWORD)control2->ClassName == 0x0080ffff && (bt == btPush || bt == btCheck || bt == btRadio);
                    BOOL control2IsEditWithoutBorder = (DWORD)control2->ClassName == 0x0081ffff && (control2->Style & WS_BORDER) == 0;
                    if (deltaX != (control2IsEditWithoutBorder ? 2 : 0) &&
                        (!control2IsPushOrCheckOrRadio || deltaX != LABEL2CHECKBOX_H_ALIGN) &&
                        !Data.IgnoreProblem(iltIncorPlLbl, dialogData->ID, control1->ID, control2->ID))
                    {
                        if (control2IsEditWithoutBorder) // An edit without a border should simply be -2 dialog units to the left of the label; otherwise it is wrong
                            *delta = deltaX - 2;
                        else
                        {
                            if (control2IsPushOrCheckOrRadio && deltaX < LABEL2CHECKBOX_H_ALIGN / 2)
                                *delta = deltaX - LABEL2CHECKBOX_H_ALIGN;
                            else
                                *delta = deltaX;
                        }
                        return FALSE;
                    }
                }
            }

            if (inRow && !inCol && deltaX < 0)
            {
                int ctrl2YOffset;
                if (IsControlForLabelPlacingTest(dialogData, control2, control1, &ctrl2YOffset))
                {
                    int distanceX = control2->TX - (control1->TX + control1->TCX);
                    int offsetY = (control1->TY + ctrl1YOffset) - (control2->TY + ctrl2YOffset);
#define LABELOFFSET_MAX 4
#define LABELDISTANCE_MAX 70
                    if (abs(offsetY) > 0 && abs(offsetY) < LABELOFFSET_MAX && distanceX < LABELDISTANCE_MAX &&
                        !Data.IgnoreProblem(iltIncorPlLbl, dialogData->ID, control1->ID, control2->ID))
                    {
                        *delta = offsetY;
                        *deltaAxis = "vertically";
                        return FALSE;
                    }
#define LABELDISTANCE_CLOSE 5
                    if (tryingNextCtrl && !control2->IsStaticText(TRUE) && offsetY == 0 &&
                        distanceX >= 0 && distanceX <= LABELDISTANCE_CLOSE)
                    {
                        return TRUE; // Obvious label+control combination; no point searching for a label of a lower control (would only find nonsense)
                    }
                }
            }
            control2 = NULL;
        }
    }
    return TRUE;
}

BOOL ControlHasStandardSize(const CDialogData* dialog, const CControl* control)
{
#define PUSHBUTTON_STANDARD_H 14
#define PUSHBUTTON_STANDARD_W 50
#define CHECKBOX_STANDARD_H1 10
#define CHECKBOX_STANDARD_H2 12
#define EDITBOX_STANDARD_H1 12
#define EDITBOX_STANDARD_H2 14
#define STATIC_STANDARD_H 8
#define PROGRESSBAR_STANDARD_H 12
#define ETCHEDHORZ_STANDARD_H 1

    if ((DWORD)control->ClassName == 0x0080ffff) // BUTTON
    {
        switch (GetButtonType(control->Style))
        {
        case btPush:
            return control->TCY == PUSHBUTTON_STANDARD_H && control->TCX >= PUSHBUTTON_STANDARD_W ||
                   wcscmp(control->OWindowName, L"") == 0 || wcscmp(control->OWindowName, L"...") == 0 // Ignore special "short" browse buttons
                   || wcscmp(control->OWindowName, L"&...") == 0;

        case btCheck:
            return control->TCY == CHECKBOX_STANDARD_H1 || control->TCY == CHECKBOX_STANDARD_H2;

        case btRadio:
            return control->TCY == CHECKBOX_STANDARD_H1 || control->TCY == CHECKBOX_STANDARD_H2;
        }
    }

    if ((DWORD)control->ClassName == 0x0082ffff) // STATIC + progress bar
    {
        if (Data.IgnoreStaticItIsProgressBar(dialog->ID, control->ID))
            return control->TCY == PROGRESSBAR_STANDARD_H;

        DWORD ss = (control->Style & SS_TYPEMASK);
        if (ss == SS_ETCHEDHORZ && control->TCY != ETCHEDHORZ_STANDARD_H) // An etched horizontal line must have height 1
            return FALSE;

        if (ss == SS_LEFT || ss == SS_RIGHT || ss == SS_CENTER || ss == SS_SIMPLE || ss == SS_LEFTNOWORDWRAP)
            return control->TCY == 1 /* horizontal lines */ || control->TCY % STATIC_STANDARD_H == 0;
        else
            return TRUE;
    }

    if ((DWORD)control->ClassName == 0x0081ffff) // EDIT
    {
        return (control->Style & (ES_READONLY | ES_MULTILINE)) || control->TCY == EDITBOX_STANDARD_H1 ||
               control->TCY == EDITBOX_STANDARD_H2;
    }

    return TRUE;
}

BOOL IsSizeDifferent(int deltaX, int deltaY)
{
#define SIZE_MAX_DIFFERENCE 6
    return (abs(deltaX) + abs(deltaY) > 0 && abs(deltaX) + abs(deltaY) <= SIZE_MAX_DIFFERENCE);
}

BOOL IsAlignmentDifferent(int delta)
{
#define ALIGNMENT_MAX_DIFFERENCE 3
    return (abs(delta) > 0 && abs(delta) <= ALIGNMENT_MAX_DIFFERENCE);
}

enum EnumControlType
{
    ectStatic,
    ectGroupBox,
    ectCheckBox,
    ectRadioButton,
    ectPushButton,
    ectComboBox,
    ectIGNORE,
};

BOOL IsEmptyString(const wchar_t* txt)
{
    while (*txt != 0 && (*txt == L' ' || *txt == L'\t' || *txt == L'\r' || *txt == L'\n'))
        txt++;
    return *txt == 0;
}

BOOL IsControlClippedAux(const CDialogData* dialog, const CControl* control, HDC hDC, LOGFONT* lf,
                         int fontHeight, EnumControlType controlType, const wchar_t* controlText,
                         BOOL multiTextIsBold, CCheckLstItem* checkLstItem, int* idealSizeX, int* idealSizeY, int cxReserve,
                         int buttonMarginsAlphaCX, int buttonMarginsSymbolCX, int buttonDropDownSymbolCX,
                         int buttonMoreSymbolCX, int buttonMarginsCY, int boxWidth,
                         int comboBoxMarginsCX, int comboBoxListMarginsCX)
{
    wchar_t buff[1000];
    wchar_t buff2[1000];
    lf->lfHeight = fontHeight;
    HFONT dlgFont = CreateFontIndirect(lf);
    HFONT dlgFontBold = NULL;
    if (dlgFont == NULL)
    {
        TRACE_E("IsControlClippedAux(): unable to create dialog font, height: " << fontHeight);
        return TRUE; // Treat as an error—report it as clipped
    }

    HFONT hOldFont = (HFONT)SelectObject(hDC, dlgFont);

    // Calculation of "dialog box units based on the current font" taken from Microsoft:
    // http://support.microsoft.com/kb/145994
    TEXTMETRIC tm;
    GetTextMetrics(hDC, &tm);
    SIZE size;
    GetTextExtentPoint32(hDC, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &size);
    int baseUnitX = (size.cx / 26 + 1) / 2;
    int baseUnitY = tm.tmHeight;

    RECT wndR;
    wndR.left = wndR.top = 0;
    wndR.right = MulDiv(control->TCX, baseUnitX, 4);
    wndR.bottom = MulDiv(control->TCY, baseUnitY, 8);
    RECT wndRBackup = wndR;

    BOOL ret = FALSE;
    BOOL progressDlgCheckRound = 1; // 1 = Copy + hr+min, 2 = Move + hr+min, 3 = Copy + min+secs, 4 = Move + min+secs, 5 is no longer needed
    BOOL setBoldFont = FALSE;
    while (1)
    {
        RECT txtR = wndR;
        RECT txtR2 = wndR;

        DWORD dtFlags = DT_CALCRECT | DT_LEFT;
        int idealSizeXAdd = 0;
        if (controlType == ectStatic)
            dtFlags |= DT_WORDBREAK | ((control->Style & SS_NOPREFIX) ? DT_NOPREFIX : 0);
        else
            dtFlags |= DT_SINGLELINE;
        wchar_t changedControlText[10000];
        if (checkLstItem != NULL && checkLstItem->Type == cltAboutDialogTitle)
        {
            if (ValidatePrintfSpecifier(controlText, L"%s"))
            {
                swprintf_s(changedControlText, controlText, L"99.99 beta 10 (PB999 x86)");
                controlText = changedControlText;
                setBoldFont = TRUE;
            }
            else
            {
                swprintf_s(buff, L"IsControlClippedAux(): unexpected format of string for about-dlg-title-check (expecting format: %%s): "
                                 L"dialog %hs, control %hs",
                           DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                OutWindow.AddLine(buff, mteError);
                ret = TRUE; // Treat as an error—report it as clipped
                break;
            }
        }
        if (checkLstItem != NULL && checkLstItem->Type == cltProgressDialogStatus)
        {
            int txtID = (progressDlgCheckRound & 0x01) ? 13805 /* IDS_PROGDLGSTATUSCOPY */ : 13806 /* IDS_PROGDLGSTATUSMOVE */;
            if (!Data.GetStringWithID(txtID, buff, _countof(buff)))
            {
            STRING_NOT_FOUND:

                swprintf_s(buff, L"IsControlClippedAux(): string ID for progress-dlg-status-check was not found: string ID %hs, dialog %hs, control %hs",
                           DataRH.GetIdentifier(txtID), DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                OutWindow.AddLine(buff, mteError);
                ret = TRUE; // Treat as an error—report it as clipped
                break;
            }
            const wchar_t* formatTxt = L"%s%s";
            if (ValidatePrintfSpecifier(buff, formatTxt))
            {
                const wchar_t* sizeTxt = L"1023 MB";
                swprintf_s(changedControlText, buff, sizeTxt, sizeTxt);
                wcscat_s(changedControlText, L", ");

                txtID = 13808 /*IDS_PROGDLGTRRATELIM*/;
                if (!Data.GetStringWithID(txtID, buff, _countof(buff)))
                    goto STRING_NOT_FOUND;
                if (!ValidatePrintfSpecifier(buff, formatTxt))
                    goto STRING_FORMAT_ERROR;
                swprintf_s(changedControlText + wcslen(changedControlText), _countof(changedControlText) - wcslen(changedControlText),
                           buff, sizeTxt, sizeTxt);
                wcscat_s(changedControlText, L", ");
                formatTxt = L"%d";
                if (progressDlgCheckRound < 3)
                {
                    txtID = 13800 /* IDS_PROGDLGHOURS */;
                    if (!Data.GetStringWithID(txtID, buff, _countof(buff)))
                        goto STRING_NOT_FOUND;
                    if (!ValidatePrintfSpecifier(buff, formatTxt))
                        goto STRING_FORMAT_ERROR;
                    swprintf_s(buff2, buff, 8); // At 9 hours we round to hours (minutes are not shown)
                    wcscat_s(buff2, L" ");
                }
                else
                    buff2[0] = 0;
                txtID = 13801 /* IDS_PROGDLGMINUTES */;
                if (!Data.GetStringWithID(txtID, buff, _countof(buff)))
                    goto STRING_NOT_FOUND;
                if (!ValidatePrintfSpecifier(buff, formatTxt))
                    goto STRING_FORMAT_ERROR;
                swprintf_s(buff2 + wcslen(buff2), _countof(buff2) - wcslen(buff2), buff,
                           progressDlgCheckRound < 3 ? 59 : 8); // When hours are shown we measure two digits; with seconds we measure only 8 minutes (9 minutes already round to minutes, so seconds are hidden and space is sufficient)
                if (progressDlgCheckRound >= 3)
                {
                    wcscat_s(buff2, L" ");
                    txtID = 13802 /* IDS_PROGDLGSECS */;
                    if (!Data.GetStringWithID(txtID, buff, _countof(buff)))
                        goto STRING_NOT_FOUND;
                    if (!ValidatePrintfSpecifier(buff, formatTxt))
                        goto STRING_FORMAT_ERROR;
                    swprintf_s(buff2 + wcslen(buff2), _countof(buff2) - wcslen(buff2), buff, 59);
                }
                txtID = 13809 /* IDS_PROGDLGTIMELEFT */;
                if (!Data.GetStringWithID(txtID, buff, _countof(buff)))
                    goto STRING_NOT_FOUND;
                formatTxt = L"%s";
                if (!ValidatePrintfSpecifier(buff, formatTxt))
                    goto STRING_FORMAT_ERROR;
                swprintf_s(changedControlText + wcslen(changedControlText), _countof(changedControlText) - wcslen(changedControlText),
                           buff, buff2);
            }
            else
            {
            STRING_FORMAT_ERROR:

                swprintf_s(buff, L"IsControlClippedAux(): unexpected format of string for progress-dlg-status-check (expecting format: %s): "
                                 L"dialog %hs, control %hs",
                           formatTxt, DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                OutWindow.AddLine(buff, mteError);
                ret = TRUE; // Treat as an error—report it as clipped
                break;
            }
            controlText = changedControlText;
        }
        if (multiTextIsBold)
            setBoldFont = TRUE;
        if (setBoldFont)
        {
            if (dlgFontBold == NULL)
            {
                LOGFONT lfBold = *lf;
                lfBold.lfWeight = FW_BOLD;
                dlgFontBold = CreateFontIndirect(&lfBold);
                if (dlgFontBold == NULL)
                {
                    TRACE_E("IsControlClippedAux(): unable to create BOLD dialog font, height: " << fontHeight);
                    ret = TRUE; // Treat as an error—report it as clipped
                    break;
                }
            }
            SelectObject(hDC, dlgFontBold);
        }
        DrawTextW(hDC, controlText, -1, &txtR, dtFlags);
        int cxReserveUsed = controlType != ectStatic || control->TCY < 16 ? cxReserve : 0; // Do not enlarge multi-line statics unnecessarily
        txtR.right += cxReserveUsed;
        if (controlType == ectPushButton)
        {
            int len = wcslen(controlText);
            bool isAlpha = len > 0 && (IsCharAlphaNumericW(controlText[0] == L'&' && len > 1 ? controlText[1] : controlText[0]) || IsCharAlphaNumericW(controlText[len - 1]));
            wndR.right -= isAlpha ? buttonMarginsAlphaCX : buttonMarginsSymbolCX; // Button margins—no idea how to get them programmatically, so they are hard-coded (Petr: increased from 6 to 12 for text buttons to catch ugly small buttons)
            wndR.bottom -= buttonMarginsCY;                                       // Button margins—no idea how to get them programmatically, so they are hard-coded
            if (checkLstItem != NULL)
                wndR.right -= checkLstItem->Type == cltDropDownButton ? buttonDropDownSymbolCX : buttonMoreSymbolCX;
            // idealSizeXAdd = 6; // Button margins—no idea how to get them programmatically, so they are hard-coded
        }
        if (controlType == ectCheckBox)
        {
            // Petr: radios needed 16, expect the same here; at worst they end up one unit wider, which is fine
            wndR.right -= boxWidth;   // Checkbox square
            idealSizeXAdd = boxWidth; // Checkbox square
        }
        if (controlType == ectRadioButton)
        {
            // jry: in other branches I found that value 15 still let clipped texts slip through
            wndR.right -= boxWidth;   // Radio button circle
            idealSizeXAdd = boxWidth; // Radio button circle
        }
        if (idealSizeX != NULL && idealSizeY != NULL &&
            !IsEmptyString(controlText) &&                   // Controls (likely statics) with empty text are not shrunk—the software is expected to fill them dynamically at runtime
            controlType != ectPushButton &&                  // Shrinking buttons is more trouble than it is worth (assumption: Ctrl+A and S in the Layout Editor, which would break buttons)
            controlType != ectGroupBox &&                    // Shrinking group boxes is more trouble than it is worth (assumption: Ctrl+A and S in the Layout Editor, which would break group boxes)
            controlType != ectComboBox &&                    // Shrinking combo boxes is more trouble than it is worth (assumption: Ctrl+A and S in the Layout Editor, which would break combos)
            (controlType != ectStatic || control->TCY < 16)) // Multi-line statics are not supported
        {
            // Perform exact calculations here without cxReserve; the caller adds reserve so we avoid duplicating it
            if (controlType == ectStatic)
                DrawTextW(hDC, controlText, -1, &txtR2, DT_CALCRECT | DT_LEFT | DT_SINGLELINE | ((control->Style & SS_NOPREFIX) ? DT_NOPREFIX : 0));
            else
            {
                txtR2 = txtR;
                txtR2.right -= cxReserveUsed;
            }

            int curIdealSizeX = ((txtR2.right + idealSizeXAdd) * 4 + baseUnitX - 1) / baseUnitX;
            if (*idealSizeX < curIdealSizeX)
                *idealSizeX = curIdealSizeX;

            int curIdealSizeY = -1;
            switch (controlType)
            {
            case ectStatic:
                curIdealSizeY = 8;
                break;

            case ectCheckBox:
            {
                if (control->TCY != 10 && control->TCY != 12)
                    curIdealSizeY = 12;
                break;
            }

            case ectRadioButton:
            {
                if (control->TCY != 10 && control->TCY != 12)
                    curIdealSizeY = 12;
                break;
            }
            }
            if (*idealSizeY < curIdealSizeY)
                *idealSizeY = curIdealSizeY;
        }
        if (controlType == ectComboBox)
            wndR.right -= (control->Style & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST ? comboBoxListMarginsCX : comboBoxMarginsCX; // Combo-box margins

        // For a static with zero width DrawTextW returns TRUE without enlarging the rectangle
        // For empty text DrawTextW grows txtR vertically
        ret |= txtR.right > wndR.right ||
               txtR.bottom > wndR.bottom && controlText[0] != 0 ||
               wndR.left == wndR.right && controlText[0] != 0;

        if (checkLstItem != NULL && checkLstItem->Type == cltProgressDialogStatus)
        {
            if (progressDlgCheckRound >= 4)
                break;               // The fifth round is unnecessary
            progressDlgCheckRound++; // Move on to the next round
        }
        else
            break;
        wndR = wndRBackup;
    }

    SelectObject(hDC, hOldFont);
    DeleteObject(dlgFont);
    if (dlgFontBold != NULL)
        DeleteObject(dlgFontBold);

    return ret;
}
/*
// Show the measured text in the main window—used for debugging issues
    static int iii = 0;
    if (iii == 0)
    {
      HWND hWnd = GetForegroundWindow();
      HDC hDC = GetWindowDC(hWnd);
      SelectObject(hDC, hFont);
      txtR.left += 500;
      txtR.right += 500;
      txtR.top += 300;
      txtR.bottom += 300;

      DrawTextW(hDC, controlText, -1, &txtR, DT_LEFT | DT_WORDBREAK);
      iii++;
    }
*/

// X-axis reserve; even if the text grows by this many points in another situation,
// Still does not cause clipping
#define CX_RESERVE_DPI_100 2
#define CX_RESERVE_DPI_125 2
#define CX_RESERVE_DPI_150 3
#define CX_RESERVE_DPI_200 4

// Hard-measured required button X margins for text buttons
#define CX_BUTTON_MARGINS_ALPHA_DPI_100 12
#define CX_BUTTON_MARGINS_ALPHA_DPI_125 15
#define CX_BUTTON_MARGINS_ALPHA_DPI_150 18
#define CX_BUTTON_MARGINS_ALPHA_DPI_200 24

// Hard-measured required button X margins for symbol buttons (e.g., "...")
#define CX_BUTTON_MARGINS_SYMBOL_DPI_100 6
#define CX_BUTTON_MARGINS_SYMBOL_DPI_125 7
#define CX_BUTTON_MARGINS_SYMBOL_DPI_150 9
#define CX_BUTTON_MARGINS_SYMBOL_DPI_200 12

// Hard-measured required drop-down button X extension (to fit the drop-down symbol on the right)
#define CX_BUTTON_DROP_DOWN_DPI_100 8
#define CX_BUTTON_DROP_DOWN_DPI_125 10
#define CX_BUTTON_DROP_DOWN_DPI_150 12
#define CX_BUTTON_DROP_DOWN_DPI_200 16

// Hard-measured required More-button X extension (to fit the "more" symbol on the right)
#define CX_BUTTON_MORE_DPI_100 14
#define CX_BUTTON_MORE_DPI_125 17
#define CX_BUTTON_MORE_DPI_150 21
#define CX_BUTTON_MORE_DPI_200 28

// Hard-measured required button Y margins
#define CY_BUTTON_MARGINS_DPI_100 4
#define CY_BUTTON_MARGINS_DPI_125 6
#define CY_BUTTON_MARGINS_DPI_150 7
#define CY_BUTTON_MARGINS_DPI_200 11

// Hard-measured required width of radio/checkbox boxes
#define CX_BOX_WIDTH_DPI_100 16
#define CX_BOX_WIDTH_DPI_125 20
#define CX_BOX_WIDTH_DPI_150 25
#define CX_BOX_WIDTH_DPI_200 27

// Hard-measured required combo-box X margins for editable combos (edit line + drop-down list)
#define CX_COMBOBOX_MARGINS_DPI_100 25
#define CX_COMBOBOX_MARGINS_DPI_125 30
#define CX_COMBOBOX_MARGINS_DPI_150 36
#define CX_COMBOBOX_MARGINS_DPI_200 46

// Hard-measured required combo-box X margins for non-editable combos (static + drop-down list)
#define CX_COMBOBOX_LIST_MARGINS_DPI_100 24
#define CX_COMBOBOX_LIST_MARGINS_DPI_125 28
#define CX_COMBOBOX_LIST_MARGINS_DPI_150 33
#define CX_COMBOBOX_LIST_MARGINS_DPI_200 41

BOOL IsControlClipped(const CDialogData* dialog, const CControl* control, HWND hDlg,
                      int* idealSizeX, int* idealSizeY, CCheckLstItem* multiTextOrComboItem,
                      CCheckLstItem* checkLstItem)
{
    if (idealSizeX != NULL)
        *idealSizeX = -1; // If we do not know the ideal size, return -1
    if (idealSizeY != NULL)
        *idealSizeY = -1; // If we do not know the ideal size, return -1
    HWND hControl = GetDlgItem(hDlg, control->ID);

    // control->TWindowName is encoded via __GetCharacterString
    // Use the text extracted directly from the control
    wchar_t controlText[100000];
    GetWindowTextW(hControl, controlText, 100000);
    controlText[99999] = 0;

    wchar_t buff[1000];
    EnumControlType controlType;
    switch ((DWORD)control->ClassName)
    {
    case 0x0080ffff:
    {
        switch (GetButtonType(control->Style))
        {
        case btPush:
            controlType = ectPushButton;
            break;
        case btCheck:
            controlType = ectCheckBox;
            break;
        case btRadio:
            controlType = ectRadioButton;
            break;
        case btGroup:
            controlType = ectGroupBox;
            break;

        default:
        {
            swprintf_s(buff, L"IsControlClipped() unsupported button type, dialog %hs, style %d",
                       DataRH.GetIdentifier(dialog->ID), (DWORD)(control->Style & 0x0000000f));
            OutWindow.AddLine(buff, mteError);
            return FALSE;
            break;
        }
        }
        break;
    }

    case 0x0082ffff:
    {
        controlType = ectStatic;
        break;
    }

    case 0x0085ffff:
    {
        if (multiTextOrComboItem == NULL || multiTextOrComboItem->Type != cltComboBox)
            return FALSE;
        controlType = ectComboBox;
        break;
    }

    default:
    {
        /*
      wchar_t cls[1000];
      if (LOWORD(control->ClassName) == 0xFFFF)
        swprintf_s(cls, L"%d", (DWORD)control->ClassName);
      else
        swprintf_s(cls, L"%s", control->ClassName);
      swprintf_s(buff, L"IsControlClipped() unsupported control class, dialog %hs, class: %s",
                 DataRH.GetIdentifier(dialog->ID), cls);
      OutWindow.AddLine(buff, mteError);
      */
        return FALSE;
    }
    }

    HFONT hFont = (HFONT)SendMessage(hControl, WM_GETFONT, 0, 0);
    if (hControl == NULL || hFont == NULL)
    {
        //    swprintf_s(buff, L"IsControlClipped() WM_GETFONT failed, dialog: %hs, control: %hs", DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
        //    OutWindow.AddLine(buff, mteError);
        return FALSE;
    }

    LOGFONT lf;
    GetObject(hFont, sizeof(LOGFONT), &lf);

    HDC hDC = HANDLES(GetDC(hControl));

    BOOL ret = FALSE;
    BOOL multiTextIsBold = multiTextOrComboItem != NULL ? multiTextOrComboItem->Type == cltMultiTextControlBold : FALSE;
    int count = multiTextOrComboItem != NULL ? multiTextOrComboItem->IDList.Count : 0;
    for (int j = -1; j < count; j++)
    {
        if (j == -1 && multiTextOrComboItem != NULL && controlText[0] == 0)
            continue;
        if (j >= 0)
        {
            if (!Data.GetStringWithID(multiTextOrComboItem->IDList[j], controlText, _countof(controlText)))
            {
                swprintf_s(buff, L"IsControlClipped(): string ID for multi-text or combobox control was not found, string ID %hs, dialog %hs, control %hs",
                           DataRH.GetIdentifier(multiTextOrComboItem->IDList[j]), DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                OutWindow.AddLine(buff, mteError);
                continue;
            }
            if (controlText[0] == 0)
                continue;
        }

        // On Win7 I noticed symbol sizes (e.g., radio/check boxes) change
        // Only for 100%, 125%, 150%, and 200% DPI; intermediate DPIs reuse symbols from the lower whole DPI
        // For lower "whole" DPIs we measure all intermediate font sizes for them,
        // That should cover every possible DPI
        for (int i = 0; i < 2; i++)
            ret |= IsControlClippedAux(dialog, control, hDC, &lf, -11 - i, controlType, controlText,
                                       multiTextIsBold, checkLstItem, idealSizeX, idealSizeY, CX_RESERVE_DPI_100,
                                       CX_BUTTON_MARGINS_ALPHA_DPI_100, CX_BUTTON_MARGINS_SYMBOL_DPI_100,
                                       CX_BUTTON_DROP_DOWN_DPI_100, CX_BUTTON_MORE_DPI_100,
                                       CY_BUTTON_MARGINS_DPI_100, CX_BOX_WIDTH_DPI_100,
                                       CX_COMBOBOX_MARGINS_DPI_100, CX_COMBOBOX_LIST_MARGINS_DPI_100);
        // if (ret) break; // Because of size-to-content we traverse all variants to measure the maximum multi-text width
        for (int i = 0; i < 3; i++)
            ret |= IsControlClippedAux(dialog, control, hDC, &lf, -13 - i, controlType, controlText,
                                       multiTextIsBold, checkLstItem, idealSizeX, idealSizeY, CX_RESERVE_DPI_125,
                                       CX_BUTTON_MARGINS_ALPHA_DPI_125, CX_BUTTON_MARGINS_SYMBOL_DPI_125,
                                       CX_BUTTON_DROP_DOWN_DPI_125, CX_BUTTON_MORE_DPI_125,
                                       CY_BUTTON_MARGINS_DPI_125, CX_BOX_WIDTH_DPI_125,
                                       CX_COMBOBOX_MARGINS_DPI_125, CX_COMBOBOX_LIST_MARGINS_DPI_125);
        // if (ret) break; // Because of size-to-content we traverse all variants to measure the maximum multi-text width
        for (int i = 0; i < 5; i++)
            ret |= IsControlClippedAux(dialog, control, hDC, &lf, -16 - i, controlType, controlText,
                                       multiTextIsBold, checkLstItem, idealSizeX, idealSizeY, CX_RESERVE_DPI_150,
                                       CX_BUTTON_MARGINS_ALPHA_DPI_150, CX_BUTTON_MARGINS_SYMBOL_DPI_150,
                                       CX_BUTTON_DROP_DOWN_DPI_150, CX_BUTTON_MORE_DPI_150,
                                       CY_BUTTON_MARGINS_DPI_150, CX_BOX_WIDTH_DPI_150,
                                       CX_COMBOBOX_MARGINS_DPI_150, CX_COMBOBOX_LIST_MARGINS_DPI_150);
        // if (ret) break; // Because of size-to-content we traverse all variants to measure the maximum multi-text width
        ret |= IsControlClippedAux(dialog, control, hDC, &lf, -21, controlType, controlText,
                                   multiTextIsBold, checkLstItem, idealSizeX, idealSizeY, CX_RESERVE_DPI_200,
                                   CX_BUTTON_MARGINS_ALPHA_DPI_200, CX_BUTTON_MARGINS_SYMBOL_DPI_200,
                                   CX_BUTTON_DROP_DOWN_DPI_200, CX_BUTTON_MORE_DPI_200,
                                   CY_BUTTON_MARGINS_DPI_200, CX_BOX_WIDTH_DPI_200,
                                   CX_COMBOBOX_MARGINS_DPI_200, CX_COMBOBOX_LIST_MARGINS_DPI_200);
        // if (ret) break; // Because of size-to-content we traverse all variants to measure the maximum multi-text width
    }

    HANDLES(ReleaseDC(hControl, hDC));

    return ret;
}

void CData::MarkChangedTextsAsTranslated()
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();

    wchar_t buff[10000];

    int i, j;
    int changes = 0;

    // display intro
    swprintf_s(buff, L"Searching for changed texts marked as Untranslated to mark them as Translated...");
    OutWindow.AddLine(buff, mteInfo);

    // Scan dialogs
    for (i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dialog = DlgData[i];
        for (j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            if (!control->ShowInLVWithControls(j))
                continue;

            if (control->State != PROGRESS_STATE_TRANSLATED &&
                wcscmp(control->OWindowName, control->TWindowName) != 0)
            {
                control->State = PROGRESS_STATE_TRANSLATED;
                changes++;
            }
        }
    }

    // Scan strings
    for (i = 0; i < StrData.Count; i++)
    {
        CStrData* strData = StrData[i];
        for (j = 0; j < 16; j++)
        {
            wchar_t* str = strData->TStrings[j];

            if (str == NULL || wcslen(str) == 0)
                continue;

            if (strData->TState[j] != PROGRESS_STATE_TRANSLATED &&
                wcscmp(strData->OStrings[j], strData->TStrings[j]) != 0)
            {
                strData->TState[j] = PROGRESS_STATE_TRANSLATED;
                changes++;
            }
        }
    }

    // Scan Windows menus
    for (i = 0; i < MenuData.Count; i++)
    {
        CMenuData* menu = MenuData[i];
        for (j = 0; j < menu->Items.Count; j++)
        {
            CMenuItem* item = &menu->Items[j];
            if (wcslen(item->TString) == 0)
                continue;

            if (item->State != PROGRESS_STATE_TRANSLATED &&
                wcscmp(item->TString, item->OString) != 0)
            {
                item->State = PROGRESS_STATE_TRANSLATED;
                changes++;
            }
        }
    }

    // display outro
    swprintf_s(buff, L"%d text(s) have been marked as Translated.", changes);
    OutWindow.AddLine(buff, mteSummary);
    PostMessage(FrameWindow.HWindow, WM_FOCLASTINOUTPUTWND, 0, 0); // The Output window might not exist yet

    if (changes > 0)
    {
        Data.SetDirty();
        Data.UpdateAllNodes(); // Update translated states in the tree view
        Data.UpdateTexts();    // Update the list view in the Texts window
    }

    OutWindow.EnablePaint(TRUE);

    SetCursor(hOldCursor);
}

void CData::ResetDialogsLayoutsToOriginal()
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();

    wchar_t buff[10000];

    int i, j;

    // display intro
    swprintf_s(buff, L"Changing all dialogs to original layout...");
    OutWindow.AddLine(buff, mteInfo);

    // Scan dialogs
    for (i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dialog = DlgData[i];
        dialog->TX = dialog->OX;
        dialog->TY = dialog->OY;
        dialog->TCX = dialog->OCX;
        dialog->TCY = dialog->OCY;
        for (j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            control->TX = control->OX;
            control->TY = control->OY;
            control->TCX = control->OCX;
            control->TCY = control->OCY;
        }
    }

    // display outro
    swprintf_s(buff, L"DONE");
    OutWindow.AddLine(buff, mteSummary);
    PostMessage(FrameWindow.HWindow, WM_FOCLASTINOUTPUTWND, 0, 0); // The Output window might not exist yet

    Data.SetDirty();
    Data.UpdateAllNodes(); // Update translated states in the tree view
    Data.UpdateTexts();    // Update the list view in the Texts window

    OutWindow.EnablePaint(TRUE);

    SetCursor(hOldCursor);
}

BOOL CData::GetItemFromCheckLst(WORD dialogID, WORD controlID, CCheckLstItem** multiTextOrComboItem,
                                CCheckLstItem** checkLstItem, BOOL anyControl)
{
    if (multiTextOrComboItem)
        *multiTextOrComboItem = NULL;
    if (checkLstItem)
        *checkLstItem = NULL;
    BOOL foundMTorC = FALSE;
    BOOL foundOtherItem = FALSE;
    for (int i = 0; i < CheckLstItems.Count; i++)
    {
        CCheckLstItem* item = CheckLstItems[i];
        if (item->DialogID == dialogID &&
            (anyControl || item->ControlID == controlID))
        {
            if (item->Type == cltMultiTextControl || item->Type == cltMultiTextControlBold || item->Type == cltComboBox)
            {
                if (multiTextOrComboItem != NULL)
                {
                    foundMTorC = TRUE;
                    *multiTextOrComboItem = item;
                }
            }
            else // These "other" tests are handled only inside IsControlClippedAux (it is not a list of texts to measure sequentially)
            {
                if (checkLstItem != NULL)
                {
                    foundOtherItem = TRUE;
                    *checkLstItem = item;
                }
            }
            if ((foundMTorC || multiTextOrComboItem == NULL) && (foundOtherItem || checkLstItem == NULL))
                break;
        }
    }
    return foundMTorC || foundOtherItem;
}

BOOL CData::IsStringWithCSV(WORD strID)
{
    for (int i = 0; i < CheckLstItems.Count; i++)
    {
        CCheckLstItem* item = CheckLstItems[i];
        if (item->Type == cltStringsWithCSV && item->ContainsDlgID(strID))
            return TRUE;
    }
    return FALSE;
}

void CData::ValidateTranslation(HWND hParent)
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();

    wchar_t buff[10000];
    wchar_t buff2[1000];
    wchar_t availableKeys[100];

    int i, j;
    int lvIndex;

    if (Config.ValidateDlgHotKeyConflicts)
    {
        // display intro
        swprintf_s(buff, L"Searching for hot key conflicts in dialogs...");
        OutWindow.AddLine(buff, mteInfo);

        CKeyConflict conflict;

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            CCheckLstItem* multiTextOrComboItem; // Check whether we even have one assigned to the dialog
            BOOL findMultiTextOrComboItems = GetItemFromCheckLst(dialog->ID, 0, &multiTextOrComboItem, NULL, TRUE);

            lvIndex = 0;
            conflict.Cleanup();
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];
                if (findMultiTextOrComboItems)
                    GetItemFromCheckLst(dialog->ID, control->ID, &multiTextOrComboItem, NULL);
                BOOL showInLV = control->ShowInLVWithControls(j);
                if (multiTextOrComboItem == NULL && !showInLV ||
                    multiTextOrComboItem != NULL &&
                        multiTextOrComboItem->Type != cltMultiTextControl &&
                        multiTextOrComboItem->Type != cltMultiTextControlBold)
                    continue;

                if (multiTextOrComboItem == NULL || control->TWindowName != NULL && control->TWindowName[0] != 0)
                    conflict.AddString(control->TWindowName, j, showInLV ? lvIndex : 0, showInLV ? 0 : control->ID);
                else
                { // For now check at least the first text (if the control has no text directly in the dialog)
                    if (!Data.GetStringWithID(multiTextOrComboItem->IDList[0], buff, _countof(buff)))
                    {
                        swprintf_s(buff, L"ValidateTranslation(): string ID for multi-text control was not found, string ID %hs, dialog %hs, control %hs",
                                   DataRH.GetIdentifier(multiTextOrComboItem->IDList[0]), DataRH.GetIdentifier(dialog->ID),
                                   DataRH.GetIdentifier(control->ID));
                        OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, showInLV ? lvIndex : 0, showInLV ? 0 : control->ID);
                    }
                    else
                    {
                        if (buff[0] != 0)
                            conflict.AddString(buff, j, showInLV ? lvIndex : 0, showInLV ? 0 : control->ID);
                    }
                }
                if (showInLV)
                    lvIndex++;
            }
            conflict.PrepareConflictIteration(dialog);
            WORD foundIndex;
            WORD foundLVIndex;
            WORD foundControlID;
            while (conflict.GetNextConflict(&foundIndex, &foundLVIndex, &foundControlID))
            {
                CControl* control = dialog->Controls[foundIndex];
                if (findMultiTextOrComboItems)
                    GetItemFromCheckLst(dialog->ID, control->ID, &multiTextOrComboItem, NULL);
                buff2[0] = 0;
                if (multiTextOrComboItem == NULL || control->TWindowName != NULL && control->TWindowName[0] != 0)
                    conflict.GetAvailableKeys(control->TWindowName, availableKeys, 100);
                else
                { // Pull the first text to validate (if the control has no text directly in the dialog)
                    if (Data.GetStringWithID(multiTextOrComboItem->IDList[0], buff2, _countof(buff2)) && buff2[0] != 0)
                    {
                        conflict.GetAvailableKeys(buff2, availableKeys, 100);
                        swprintf_s(buff2 + wcslen(buff2), _countof(buff2) - wcslen(buff2), L" (string ID %hs)",
                                   DataRH.GetIdentifier(multiTextOrComboItem->IDList[0]));
                    }
                    else
                        buff2[0] = 0;
                }
                if (availableKeys[0] == 0)
                    swprintf_s(availableKeys, L"NO AVAILABLE KEY");
                swprintf_s(buff, L"Dialog %hs: %ls (hint: %ls)", DataRH.GetIdentifier(dialog->ID),
                           buff2[0] != 0 ? buff2 : control->TWindowName, availableKeys);
                OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, foundLVIndex, foundControlID);
            }
        }
    }

    if (Config.ValidateMenuHotKeyConflicts)
    {
        // display intro
        swprintf_s(buff, L"Searching for hot key conflicts in menus...");
        OutWindow.AddLine(buff, mteInfo);

        CKeyConflict conflict;

        // Scan SalMenu
        for (i = 0; i < SalMenuSections.Count; i++)
        {
            CMenuPreview* menuPreview = new CMenuPreview();
            CSalMenuSection* section = SalMenuSections[i];
            conflict.Cleanup();

            if (section->SectionControlDialogID != 0)
            {
                // The section is linked to a dialog control; check conflicts against it
                BOOL found = FALSE;
                for (int dlgIndex = 0; dlgIndex < DlgData.Count; dlgIndex++)
                {
                    if (DlgData[dlgIndex]->ID == section->SectionControlDialogID)
                    {
                        CDialogData* dialog = DlgData[dlgIndex];
                        lvIndex = 0;
                        for (int ctrlID = 0; ctrlID < dialog->Controls.Count; ctrlID++)
                        {
                            CControl* control = dialog->Controls[ctrlID];
                            BOOL showInLV = control->ShowInLVWithControls(ctrlID);
                            if (showInLV && control->ID == section->SectionControlControlID)
                            {
                                conflict.AddString(control->TWindowName, ctrlID, lvIndex, 0, 2, (DWORD)dialog);
                                menuPreview->AddText(control->TWindowName);
                                found = TRUE;
                                break;
                            }
                            if (showInLV)
                                lvIndex++;
                        }
                        if (!found)
                        {
                            swprintf_s(buff, L"Control <%d> not found in dialog %hs!", section->SectionControlControlID,
                                       DataRH.GetIdentifier(dialog->ID));
                            OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID);
                            found = TRUE; // Do not show the same error again
                        }
                        break;
                    }
                }
                if (!found)
                {
                    swprintf_s(buff, L"Dialog <%d> not found!", section->SectionControlDialogID);
                    OutWindow.AddLine(buff, mteError);
                }
            }

            for (j = 0; j < section->StringsID.Count; j++)
            {
                WORD strID = section->StringsID[j];
                int index;
                int subIndex;
                if (FindStringWithID(strID, &index, &subIndex, &lvIndex))
                {
                    if (index > 0xffff || subIndex > 0xffff)
                        TRACE_E("Out of range!");
                    else
                    {
                        conflict.AddString(StrData[index]->TStrings[subIndex], index, lvIndex, 0, 0, subIndex, i, j);
                        menuPreview->AddText(StrData[index]->TStrings[subIndex]);
                    }
                }
                else
                {
                    swprintf_s(buff, L"%hs <%d> is not string!", DataRH.GetIdentifier(strID), strID);
                    OutWindow.AddLine(buff, mteError);
                }
            }
            if (section->SectionDialogID != 0)
            {
                // The section is linked to a dialog; check conflicts against the dialog
                CDialogData* dialog = NULL;
                for (int dlgIndex = 0; dlgIndex < DlgData.Count; dlgIndex++)
                {
                    if (DlgData[dlgIndex]->ID == section->SectionDialogID)
                    {
                        dialog = DlgData[dlgIndex];
                        break;
                    }
                }
                if (dialog == NULL)
                {
                    swprintf_s(buff, L"Dialog <%d> not found!", section->SectionDialogID);
                    OutWindow.AddLine(buff, mteError);
                }
                else
                {
                    CCheckLstItem* multiTextOrComboItem; // Check whether we even have one assigned to the dialog
                    BOOL findMultiTextOrComboItems = GetItemFromCheckLst(dialog->ID, 0, &multiTextOrComboItem, NULL, TRUE);

                    lvIndex = 0;
                    for (int ctrlID = 0; ctrlID < dialog->Controls.Count; ctrlID++)
                    {
                        CControl* control = dialog->Controls[ctrlID];

                        if (findMultiTextOrComboItems)
                            GetItemFromCheckLst(dialog->ID, control->ID, &multiTextOrComboItem, NULL);
                        BOOL showInLV = control->ShowInLVWithControls(ctrlID);
                        if (multiTextOrComboItem == NULL && !showInLV ||
                            multiTextOrComboItem != NULL &&
                                multiTextOrComboItem->Type != cltMultiTextControl &&
                                multiTextOrComboItem->Type != cltMultiTextControlBold)
                            continue;

                        if (multiTextOrComboItem == NULL || control->TWindowName != NULL && control->TWindowName[0] != 0)
                        {
                            conflict.AddString(control->TWindowName, ctrlID, showInLV ? lvIndex : 0, showInLV ? 0 : control->ID, 1, (DWORD)dialog);
                            menuPreview->AddText(control->TWindowName);
                        }
                        else
                        { // For now check at least the first text (if the control has no text directly in the dialog)
                            if (!Data.GetStringWithID(multiTextOrComboItem->IDList[0], buff, _countof(buff)))
                            {
                                swprintf_s(buff, L"ValidateTranslation(): string ID for multi-text control was not found, string ID %hs, dialog %hs, control %hs",
                                           DataRH.GetIdentifier(multiTextOrComboItem->IDList[0]), DataRH.GetIdentifier(dialog->ID),
                                           DataRH.GetIdentifier(control->ID));
                                OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, showInLV ? lvIndex : 0, showInLV ? 0 : control->ID);
                            }
                            else
                            {
                                if (buff[0] != 0)
                                {
                                    conflict.AddString(buff, ctrlID, showInLV ? lvIndex : 0, showInLV ? 0 : control->ID, 1, (DWORD)dialog);
                                    menuPreview->AddText(buff);
                                }
                            }
                        }

                        if (showInLV)
                            lvIndex++;
                    }
                }
            }

            conflict.PrepareConflictIteration();
            WORD foundIndex;
            WORD foundLVIndex;
            WORD foundControlID;
            DWORD subIndex;
            DWORD sectionIndex;
            DWORD indexInSection;
            DWORD itemType;
            BOOL foundConflict = FALSE;
            while (conflict.GetNextConflict(&foundIndex, &foundLVIndex, &foundControlID, &itemType, &subIndex, &sectionIndex, &indexInSection))
            {
                if (itemType == 0) // STRING
                {
                    WORD index = foundIndex;
                    WORD strID = ((StrData[index]->ID - 1) << 4) | (WORD)subIndex;
                    conflict.GetAvailableKeys(StrData[index]->TStrings[subIndex], availableKeys, 100);
                    if (availableKeys[0] == 0)
                        swprintf_s(availableKeys, L"NO AVAILABLE KEY");
                    swprintf_s(buff, L"SalMenu: (%hs): %ls (hint: %ls)", DataRH.GetIdentifier(strID),
                               StrData[index]->TStrings[subIndex], availableKeys);
                    OutWindow.AddLine(buff, mteError, rteString, StrData[index]->ID, foundLVIndex, foundControlID);
                    menuPreview->AddLine(OutWindow.GetLinesCount() - 1);
                    foundConflict = TRUE;
                }
                else
                {
                    if (itemType == 1) // DIALOG
                    {
                        CDialogData* dialog = (CDialogData*)subIndex;
                        CControl* control = dialog->Controls[foundIndex];
                        CCheckLstItem* multiTextOrComboItem;
                        GetItemFromCheckLst(dialog->ID, control->ID, &multiTextOrComboItem, NULL);
                        buff2[0] = 0;
                        if (multiTextOrComboItem == NULL || control->TWindowName != NULL && control->TWindowName[0] != 0)
                            conflict.GetAvailableKeys(control->TWindowName, availableKeys, 100);
                        else
                        { // Pull the first text to validate (if the control has no text directly in the dialog)
                            if (Data.GetStringWithID(multiTextOrComboItem->IDList[0], buff2, _countof(buff2)) && buff2[0] != 0)
                            {
                                conflict.GetAvailableKeys(buff2, availableKeys, 100);
                                swprintf_s(buff2 + wcslen(buff2), _countof(buff2) - wcslen(buff2), L" (string ID %hs)",
                                           DataRH.GetIdentifier(multiTextOrComboItem->IDList[0]));
                            }
                            else
                                buff2[0] = 0;
                        }
                        if (availableKeys[0] == 0)
                            swprintf_s(availableKeys, L"NO AVAILABLE KEY");
                        swprintf_s(buff, L"Dialog %hs: %ls (hint: %ls)", DataRH.GetIdentifier(dialog->ID),
                                   buff2[0] != 0 ? buff2 : control->TWindowName, availableKeys);
                        OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, foundLVIndex, foundControlID);
                    }
                    else
                    {
                        if (itemType == 2) // CONTROL in dialog
                        {
                            CDialogData* dialog = (CDialogData*)subIndex;
                            CControl* control = dialog->Controls[foundIndex];
                            conflict.GetAvailableKeys(control->TWindowName, availableKeys, 100);
                            if (availableKeys[0] == 0)
                                swprintf_s(availableKeys, L"NO AVAILABLE KEY");
                            swprintf_s(buff, L"Dialog %hs: %ls (hint: %ls)", DataRH.GetIdentifier(dialog->ID),
                                       control->TWindowName, availableKeys);
                            OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, foundLVIndex, foundControlID);
                        }
                    }
                }
            }
            if (foundConflict)
                MenuPreview.Add(menuPreview);
            else
                delete menuPreview;
        }

        // Scan Windows menus
        for (i = 0; i < MenuData.Count; i++)
        {
            CMenuData* menu = MenuData[i];
            for (int conflictGroup = 0; conflictGroup <= menu->ConflictGroupsCount; conflictGroup++)
            {
                CMenuPreview* menuPreview = new CMenuPreview();
                BOOL foundConflict = FALSE;
                lvIndex = 0;
                conflict.Cleanup();
                for (j = 0; j < menu->Items.Count; j++)
                {
                    CMenuItem* item = &menu->Items[j];

                    if (wcslen(item->TString) == 0)
                        continue;

                    if (item->ConflictGroup == conflictGroup)
                    {
                        conflict.AddString(item->TString, j, lvIndex, 0);
                        menuPreview->AddText(item->TString);
                    }

                    lvIndex++;
                }
                conflict.PrepareConflictIteration();
                WORD foundIndex;
                WORD foundLVIndex;
                WORD foundControlID;
                while (conflict.GetNextConflict(&foundIndex, &foundLVIndex, &foundControlID))
                {
                    CMenuItem* item = &menu->Items[foundIndex];
                    conflict.GetAvailableKeys(item->TString, availableKeys, 100);
                    if (availableKeys[0] == 0)
                        swprintf_s(availableKeys, L"NO AVAILABLE KEY");
                    swprintf_s(buff, L"Menu %hs: %ls (hint: %ls)", DataRH.GetIdentifier(menu->ID), item->TString, availableKeys);
                    OutWindow.AddLine(buff, mteError, rteMenu, menu->ID, foundLVIndex, foundControlID);
                    menuPreview->AddLine(OutWindow.GetLinesCount() - 1);
                    foundConflict = TRUE;
                }
                if (foundConflict)
                    MenuPreview.Add(menuPreview);
                else
                    delete menuPreview;
            }
        }
    }

    if (Config.ValidatePrintfSpecifier)
    {
        // display intro
        swprintf_s(buff, L"Searching for inconsistent format specifier...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];
            lvIndex = 0;
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                    continue;

                if (!Data.IgnoreInconFmtSpecifCtrl(dialog->ID, control->ID) &&
                    !ValidatePrintfSpecifier(control->OWindowName, control->TWindowName))
                {
                    swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), control->TWindowName);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan strings
        for (i = 0; i < StrData.Count; i++)
        {
            CStrData* strData = StrData[i];
            lvIndex = 0;
            for (j = 0; j < 16; j++)
            {
                wchar_t* str = strData->TStrings[j];

                if (str == NULL || wcslen(str) == 0)
                    continue;

                WORD strID = ((strData->ID - 1) << 4) | j;

                if (!Data.IgnoreInconFmtSpecif(strID) &&
                    !ValidatePrintfSpecifier(strData->OStrings[j], strData->TStrings[j]))
                {
                    swprintf_s(buff, L"String (%d): %ls", strID, strData->TStrings[j]);
                    OutWindow.AddLine(buff, mteError, rteString, strData->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan Windows menus
        for (i = 0; i < MenuData.Count; i++)
        {
            CMenuData* menu = MenuData[i];
            lvIndex = 0;
            for (j = 0; j < menu->Items.Count; j++)
            {
                CMenuItem* item = &menu->Items[j];
                if (wcslen(item->TString) == 0)
                    continue;

                if (!ValidatePrintfSpecifier(item->TString, item->OString))
                {
                    swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), item->TString);
                    OutWindow.AddLine(buff, mteError, rteMenu, menu->ID, lvIndex);
                }
                lvIndex++;
            }
        }
    }

    if (Config.ValidateControlChars)
    {
        // display intro
        swprintf_s(buff, L"Searching for inconsistent control characters...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];
            lvIndex = 0;
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                    continue;

                if (!ValidateControlCharacters(control->OWindowName, control->TWindowName))
                {
                    swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), control->TWindowName);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan strings
        for (i = 0; i < StrData.Count; i++)
        {
            CStrData* strData = StrData[i];
            lvIndex = 0;
            for (j = 0; j < 16; j++)
            {
                wchar_t* str = strData->TStrings[j];

                if (str == NULL || wcslen(str) == 0)
                    continue;

                WORD strID = ((strData->ID - 1) << 4) | j;

                if (!Data.IgnoreInconCtrlChars(strID) &&
                    !ValidateControlCharacters(strData->OStrings[j], strData->TStrings[j]))
                {
                    swprintf_s(buff, L"String (%d): %ls", strID, strData->TStrings[j]);
                    OutWindow.AddLine(buff, mteError, rteString, strData->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan Windows menus
        for (i = 0; i < MenuData.Count; i++)
        {
            CMenuData* menu = MenuData[i];
            lvIndex = 0;
            for (j = 0; j < menu->Items.Count; j++)
            {
                CMenuItem* item = &menu->Items[j];
                if (wcslen(item->TString) == 0)
                    continue;

                if (!ValidateControlCharacters(item->TString, item->OString))
                {
                    swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), item->TString);
                    OutWindow.AddLine(buff, mteError, rteMenu, menu->ID, lvIndex);
                }
                lvIndex++;
            }
        }
    }

    if (Config.ValidateStringsWithCSV)
    {
        // display intro
        swprintf_s(buff, L"Searching for inconsistent strings with CSV...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan strings
        for (i = 0; i < StrData.Count; i++)
        {
            CStrData* strData = StrData[i];
            lvIndex = 0;
            for (j = 0; j < 16; j++)
            {
                wchar_t* str = strData->TStrings[j];

                if (str == NULL || wcslen(str) == 0)
                    continue;

                WORD strID = ((strData->ID - 1) << 4) | j;

                if (Data.IsStringWithCSV(strID) &&
                    !ValidateStringWithCSV(strData->OStrings[j], strData->TStrings[j]))
                {
                    swprintf_s(buff, L"String (%d): %ls", strID, strData->TStrings[j]);
                    OutWindow.AddLine(buff, mteError, rteString, strData->ID, lvIndex);
                }
                lvIndex++;
            }
        }
    }

    if (Config.ValidatePluralStr)
    {
        // display intro
        swprintf_s(buff, L"Searching for inconsistent plural strings...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];
            lvIndex = 0;
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                    continue;

                if (!ValidatePluralStrings(control->OWindowName, control->TWindowName))
                {
                    swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), control->TWindowName);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan strings
        for (i = 0; i < StrData.Count; i++)
        {
            CStrData* strData = StrData[i];
            lvIndex = 0;
            for (j = 0; j < 16; j++)
            {
                wchar_t* str = strData->TStrings[j];

                if (str == NULL || wcslen(str) == 0)
                    continue;

                WORD strID = ((strData->ID - 1) << 4) | j;

                if (!ValidatePluralStrings(strData->OStrings[j], strData->TStrings[j]))
                {
                    swprintf_s(buff, L"String (%d): %ls", strID, strData->TStrings[j]);
                    OutWindow.AddLine(buff, mteError, rteString, strData->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan Windows menus
        for (i = 0; i < MenuData.Count; i++)
        {
            CMenuData* menu = MenuData[i];
            lvIndex = 0;
            for (j = 0; j < menu->Items.Count; j++)
            {
                CMenuItem* item = &menu->Items[j];
                if (wcslen(item->TString) == 0)
                    continue;

                if (!ValidatePluralStrings(item->OString, item->TString))
                {
                    swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), item->TString);
                    OutWindow.AddLine(buff, mteError, rteMenu, menu->ID, lvIndex);
                }
                lvIndex++;
            }
        }
    }

    if (Config.ValidateTextEnding)
    {
        // display intro
        swprintf_s(buff, L"Searching for inconsistent text beginnings and endings...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];
            lvIndex = 0;
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                {
                    wchar_t* o = control->OWindowName != NULL && HIWORD(control->OWindowName) != 0x0000 ? control->OWindowName : (wchar_t*)L"";
                    wchar_t* t = control->TWindowName != NULL && HIWORD(control->TWindowName) != 0x0000 ? control->TWindowName : (wchar_t*)L"";
                    if (wcscmp(o, t) != 0)
                    {
                        swprintf_s(buff, L"Dialog %hs: %hs: different untranslatable content", DataRH.GetIdentifier(dialog->ID),
                                   DataRH.GetIdentifier(control->ID));
                        OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, 0, control->ID);
                    }
                    continue;
                }

                if (!Data.IgnoreInconTxtEnds(dialog->ID, control->ID) &&
                        !ValidateTextEnding(control->OWindowName, control->TWindowName, 0) ||
                    !ValidateTextBeginning(control->OWindowName, control->TWindowName))
                {
                    swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), control->TWindowName);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan strings
        for (i = 0; i < StrData.Count; i++)
        {
            CStrData* strData = StrData[i];
            lvIndex = 0;
            for (j = 0; j < 16; j++)
            {
                wchar_t* str = strData->TStrings[j];

                if (str == NULL || wcslen(str) == 0)
                    continue;

                WORD strID = ((strData->ID - 1) << 4) | j;

                if (!Data.IgnoreInconStringEnds(strID) &&
                        !ValidateTextEnding(strData->OStrings[j], strData->TStrings[j], strID) ||
                    !Data.IgnoreInconStringBegs(strID) &&
                        !ValidateTextBeginning(strData->OStrings[j], strData->TStrings[j]))
                {
                    swprintf_s(buff, L"String (%d): %ls", strID, strData->TStrings[j]);
                    OutWindow.AddLine(buff, mteError, rteString, strData->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan Windows menus
        for (i = 0; i < MenuData.Count; i++)
        {
            CMenuData* menu = MenuData[i];
            lvIndex = 0;
            for (j = 0; j < menu->Items.Count; j++)
            {
                CMenuItem* item = &menu->Items[j];
                if (wcslen(item->TString) == 0)
                    continue;

                if (!ValidateTextEnding(item->TString, item->OString, 0) ||
                    !ValidateTextBeginning(item->TString, item->OString))
                {
                    swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), item->TString);
                    OutWindow.AddLine(buff, mteError, rteMenu, menu->ID, lvIndex);
                }
                lvIndex++;
            }
        }
    }

    if (Config.ValidateHotKeys)
    {
        // display intro
        swprintf_s(buff, L"Searching for inconsistent hot keys...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];
            lvIndex = 0;
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                    continue;

                if (j != 0 && // Control 0 is the dialog name and has no hotkeys to check
                    !ValidateHotKeys(control->OWindowName, control->TWindowName))
                {
                    swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), control->TWindowName);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan strings
        for (i = 0; i < StrData.Count; i++)
        {
            CStrData* strData = StrData[i];
            lvIndex = 0;
            for (j = 0; j < 16; j++)
            {
                wchar_t* str = strData->TStrings[j];

                if (str == NULL || wcslen(str) == 0)
                    continue;

                WORD strID = ((strData->ID - 1) << 4) | j;

                if (!Data.IgnoreInconHotKeys(strID) &&
                    !ValidateHotKeys(strData->OStrings[j], strData->TStrings[j]))
                {
                    swprintf_s(buff, L"String (%d): %ls", strID, strData->TStrings[j]);
                    OutWindow.AddLine(buff, mteError, rteString, strData->ID, lvIndex);
                }
                lvIndex++;
            }
        }

        // Scan Windows menus
        for (i = 0; i < MenuData.Count; i++)
        {
            CMenuData* menu = MenuData[i];
            lvIndex = 0;
            for (j = 0; j < menu->Items.Count; j++)
            {
                CMenuItem* item = &menu->Items[j];
                if (wcslen(item->TString) == 0)
                    continue;

                if (!ValidateHotKeys(item->TString, item->OString))
                {
                    swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), item->TString);
                    OutWindow.AddLine(buff, mteError, rteMenu, menu->ID, lvIndex);
                }
                lvIndex++;
            }
        }
    }

    if (Config.ValidateLayout)
    {
        // display intro
        swprintf_s(buff, L"Searching dialogs for overlapping controls...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];
            lvIndex = 1;
            if (dialog->Controls.Count > 0 && (dialog->Controls[0]->OWindowName == NULL || dialog->Controls[0]->OWindowName[0] == 0)) // Empty dialog title
                lvIndex = 0;
            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                    continue;

                if (ShouldValidateLayoutFor(control)) // Control we are supposed to validate (ignore group boxes)
                {
                    BOOL isCheckOrRadioBox = control->IsRadioOrCheckBox();
                    for (int k = 1; k < dialog->Controls.Count; k++)
                    {
                        CControl* control2 = dialog->Controls[k];
                        if (k > j ||                                     // Take all controls after 'j'
                            k < j && !control2->ShowInLVWithControls(k)) // Before 'j' only if we have not tested them yet (skipped because they are not translated, e.g., icons)
                        {
                            BOOL shouldValidate = ShouldValidateLayoutFor(control2);
                            if ((shouldValidate && DoesControlsOverlap(dialog->ID, control, control2) ||
                                 !shouldValidate && DoesGroupBoxLabelOverlap(isCheckOrRadioBox, control, control2)) &&
                                !IgnoreProblem(iltOverlap, dialog->ID, control->ID, control2->ID))
                            {
                                swprintf_s(buff, L"Dialog %hs: %hs (%ls)", DataRH.GetIdentifier(dialog->ID),
                                           DataRH.GetIdentifier(control->ID), control->TWindowName);
                                OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);

                                swprintf_s(buff, L"...collision with %hs: %ls", DataRH.GetIdentifier(control2->ID), control2->TWindowName);
                                int lvIndex2;
                                BOOL useLVIndex = dialog->GetControlIndexInLV(k, &lvIndex2);
                                OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex2 : 0,
                                                  useLVIndex ? 0 : control2->ID);
                            }
                        }
                    }
                }

                lvIndex++;
            }
        }
    }

    if (Config.ValidateClipping)
    {
        // display intro
        swprintf_s(buff, L"Searching dialogs for clipped texts...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            // We will measure the real dialog, so we need its template
            WORD dialogTemplate[200000];
            DWORD dialogTemplateSize = dialog->PrepareTemplate(dialogTemplate, FALSE, TRUE, FALSE);
            // Do not let the dialog be displayed
            dialog->TemplateAddRemoveStyles(dialogTemplate, 0, WS_VISIBLE);

            HWND hDlg = CreateDialogIndirectW(HInstance, (LPDLGTEMPLATE)dialogTemplate, hParent, NULL);
            if (hDlg == NULL)
            {
                DWORD err = GetLastError();
                swprintf_s(buff, L"CreateDialogIndirect() xfailed, dialog %hs, error %d", DataRH.GetIdentifier(dialog->ID), err);
                OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID);
                continue;
            }

            CCheckLstItem *multiTextOrComboItem, *checkLstItem; // Check whether we even have one assigned to the dialog
            BOOL findMulTextAndDropDownItems = GetItemFromCheckLst(dialog->ID, 0, &multiTextOrComboItem, &checkLstItem, TRUE);

            lvIndex = 0;
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];

                if (findMulTextAndDropDownItems)
                    GetItemFromCheckLst(dialog->ID, control->ID, &multiTextOrComboItem, &checkLstItem);
                BOOL showInLV = control->ShowInLVWithControls(j);
                if (multiTextOrComboItem == NULL && !showInLV)
                    continue;

                DWORD className = (DWORD)control->ClassName;
                //                STATIC     ||              BUTTON     ||      COMBO BOX
                if ((className == 0x0082ffff || className == 0x0080ffff ||
                     className == 0x0085ffff && multiTextOrComboItem != NULL && multiTextOrComboItem->Type == cltComboBox) &&
                    (multiTextOrComboItem != NULL ||
                     checkLstItem != NULL && checkLstItem->Type == cltProgressDialogStatus ||
                     control->TWindowName[0] != 0) &&
                    IsControlClipped(dialog, control, hDlg, NULL, NULL, multiTextOrComboItem, checkLstItem) &&
                    !IgnoreProblem(iltClip, dialog->ID, control->ID, 0 /* not used */))
                {
                    swprintf_s(buff, L"Dialog %hs: %hs (%ls)", DataRH.GetIdentifier(dialog->ID),
                               DataRH.GetIdentifier(control->ID),
                               control->TWindowName != NULL && control->TWindowName[0] != 0 ? control->TWindowName : L"control has variable text");
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, showInLV ? lvIndex : 0, showInLV ? 0 : control->ID);
                }

                if (showInLV)
                    lvIndex++;
            }
            DestroyWindow(hDlg);
        }
    }

    if (Config.ValidateControlsToDialog)
    {
        // display intro
        swprintf_s(buff, L"Searching controls placed too close to dialog box frame...");
        OutWindow.AddLine(buff, mteInfo);

#define CONTROL_TO_DIALOG_MIN_V_SPACING 4    // (vertical) dlg units
#define CONTROL_TO_DIALOG_MIN_H_SPACING 5    // (horizontal) dlg units
#define CONTROL_TO_DIALOG_MIN_H_SPACING_RT 3 // (horizontal for right-aligned texts) dlg units

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];

                int minVSpacing = 0;
                int minHSpacing = 0;
                if (!Data.IgnoreTooCloseToDlgFrame(dialog->ID, control->ID))
                {
                    if ((dialog->Style & (DS_CONTROL | WS_CHILD)) == 0 && // Allow embedded dialogs to have controls right at the edge
                        control->TCY > 1 && control->TCX > 1)             // Horizontal and vertical lines may extend to the dialog edges
                    {
                        minVSpacing = CONTROL_TO_DIALOG_MIN_V_SPACING;
                        minHSpacing = CONTROL_TO_DIALOG_MIN_H_SPACING;
                    }

                    if ((DWORD)control->ClassName == 0x0082ffff &&    // STATIC
                        (control->Style & SS_TYPEMASK) == SS_RIGHT && // Right-aligned text may go closer to the edge (the left side is rarely used)
                        control->TCY > 1 && control->TCX > 1)         // Horizontal and vertical lines may extend to the dialog edges
                    {
                        minHSpacing = CONTROL_TO_DIALOG_MIN_H_SPACING_RT;
                    }
                }

                int x = control->TX;
                int y = control->TY;
                int cx = control->TCX;
                int cy = control->TCY;
                if (control->IsComboBox()) // For combo boxes we want their "collapsed" size
                    cy = COMBOBOX_BASE_HEIGHT;
                if (control->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialog->ID, control->ID))
                    cx = cy = 10;

                if (x < minHSpacing || dialog->TCX - (x + cx) < minHSpacing ||
                    y < minVSpacing || dialog->TCY - (y + cy) < minVSpacing)
                {
                    if (x + cx > 0 && x < dialog->TCX && // Ignore controls that are completely outside the dialog
                        y + cy > 0 && y < dialog->TCY)
                    {
                        swprintf_s(buff, L"Dialog %hs: %hs", DataRH.GetIdentifier(dialog->ID),
                                   DataRH.GetIdentifier(control->ID));
                        int lvIndex2;
                        BOOL useLVIndex2 = dialog->GetControlIndexInLV(j, &lvIndex2);
                        OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex2 ? lvIndex2 : 0,
                                          useLVIndex2 ? 0 : control->ID);
                    }
                }
                lvIndex++;
            }
        }
    }

    if (Config.ValidateMisalignedControls)
    {
        // display intro
        swprintf_s(buff, L"Searching for slightly misaligned controls...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];

                for (int k = j + 1; k < dialog->Controls.Count; k++)
                {
                    CControl* control2 = dialog->Controls[k];
                    BOOL checkLeft;
                    BOOL checkRight;
                    BOOL checkVertical;
                    if (!Data.IgnoreProblem(iltMisaligned, dialog->ID, control->ID, control2->ID) &&
                        ShouldValidateAlignment(dialog, control, control2, &checkLeft, &checkRight, &checkVertical))
                    {
                        BOOL found = FALSE;
                        int x1 = control->TX;
                        int y1 = control->TY;
                        int cx1 = control->TCX;
                        int cy1 = control->TCY;
                        int x2 = control2->TX;
                        int y2 = control2->TY;
                        int cx2 = control2->TCX;
                        int cy2 = control2->TCY;
                        if (control->IsComboBox()) // For combo boxes we want their "collapsed" size
                            cy1 = COMBOBOX_BASE_HEIGHT;
                        if (control2->IsComboBox()) // For combo boxes we want their "collapsed" size
                            cy2 = COMBOBOX_BASE_HEIGHT;
                        if (control->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialog->ID, control->ID))
                            cx1 = cy1 = 10;
                        if (control2->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialog->ID, control2->ID))
                            cx2 = cy2 = 10;

                        if (checkLeft && IsAlignmentDifferent(x1 - x2))
                            found = TRUE;
                        if (checkVertical && IsAlignmentDifferent(y1 - y2))
                            found = TRUE;
                        if (checkRight && IsAlignmentDifferent((x1 + cx1) - (x2 + cx2)))
                            found = TRUE;
                        if (checkVertical && IsAlignmentDifferent((y1 + cy1) - (y2 + cy2)))
                            found = TRUE;

                        if (found)
                        {
                            swprintf_s(buff, L"Dialog %hs: %hs", DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                            BOOL useLVIndex = dialog->GetControlIndexInLV(j, &lvIndex);
                            OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                              useLVIndex ? 0 : control->ID);

                            swprintf_s(buff, L"...misaligned to %hs", DataRH.GetIdentifier(control2->ID));
                            useLVIndex = dialog->GetControlIndexInLV(k, &lvIndex);
                            OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                              useLVIndex ? 0 : control2->ID);
                        }
                    }
                }
            }
        }
    }

    if (Config.ValidateDifferentSizedControls)
    {
        // display intro
        swprintf_s(buff, L"Searching for slightly different sized controls...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];

                for (int k = j + 1; k < dialog->Controls.Count; k++)
                {
                    CControl* control2 = dialog->Controls[k];
                    if (!Data.IgnoreProblem(iltDiffSized, dialog->ID, control->ID, control2->ID) &&
                        ShouldValidateSize(control, control2))
                    {
                        BOOL found = FALSE;
                        int cx1 = control->TCX;
                        int cy1 = control->TCY;
                        int cx2 = control2->TCX;
                        int cy2 = control2->TCY;
                        if (control->IsComboBox()) // For combo boxes we want their "collapsed" size
                            cy1 = COMBOBOX_BASE_HEIGHT;
                        if (control2->IsComboBox()) // For combo boxes we want their "collapsed" size
                            cy2 = COMBOBOX_BASE_HEIGHT;
                        if (control->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialog->ID, control->ID))
                            cx1 = cy1 = 10;
                        if (control2->IsIcon() && Data.IgnoreIconSizeIconIsSmall(dialog->ID, control2->ID))
                            cx2 = cy2 = 10;

                        if (IsSizeDifferent(cx1 - cx2, cy1 - cy2))
                            found = TRUE;

                        if (found)
                        {
                            swprintf_s(buff, L"Dialog %hs: %hs", DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                            BOOL useLVIndex = dialog->GetControlIndexInLV(j, &lvIndex);
                            OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                              useLVIndex ? 0 : control->ID);

                            swprintf_s(buff, L"...differently sized to %hs", DataRH.GetIdentifier(control2->ID));
                            useLVIndex = dialog->GetControlIndexInLV(k, &lvIndex);
                            OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                              useLVIndex ? 0 : control2->ID);
                        }
                    }
                }
            }
        }
    }

    if (Config.ValidateDifferentSpacedControls)
    {
        // display intro
        swprintf_s(buff, L"Searching for slightly different spacing between controls...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];
                if (!Data.IgnoreDifferentSpacing(dialog->ID, control->ID) &&
                    HasDifferentSpacing(dialog, j))
                {
                    swprintf_s(buff, L"Dialog %hs: %hs", DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                    BOOL useLVIndex = dialog->GetControlIndexInLV(j, &lvIndex);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                      useLVIndex ? 0 : control->ID);
                }
            }
        }
    }

    if (Config.ValidateStandardControlsSize)
    {
        // display intro
        swprintf_s(buff, L"Searching for controls with nonstandard size...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];
                if (!Data.IgnoreNotStdSize(dialog->ID, control->ID) &&
                    !ControlHasStandardSize(dialog, control))
                {
                    swprintf_s(buff, L"Dialog %hs: %hs", DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                    BOOL useLVIndex = dialog->GetControlIndexInLV(j, &lvIndex);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                      useLVIndex ? 0 : control->ID);
                }
            }
        }
    }

    if (Config.ValidateLabelsToControlsSpacing)
    {
        // display intro
        swprintf_s(buff, L"Searching for labels placed incorrectly to associated controls...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];
                int foundIndex;
                int delta;
                const char* deltaAxis;
                if (!IsLabelCorrectlyPlaced(dialog, j, &foundIndex, &delta, &deltaAxis))
                {
                    swprintf_s(buff, L"Dialog %hs: %hs is incorrectly placed (move %hs by %+d to correct)", DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID), deltaAxis, -delta);
                    BOOL useLVIndex = dialog->GetControlIndexInLV(j, &lvIndex);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                      useLVIndex ? 0 : control->ID);

                    swprintf_s(buff, L"... to %hs", DataRH.GetIdentifier(dialog->Controls[foundIndex]->ID));
                    useLVIndex = dialog->GetControlIndexInLV(foundIndex, &lvIndex);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                      useLVIndex ? 0 : dialog->Controls[foundIndex]->ID);
                }
            }
        }
    }

    if (Config.ValidateSizesOfPropPages)
    {
        // display intro
        swprintf_s(buff, L"Searching for different sizes of property pages...");
        OutWindow.AddLine(buff, mteInfo);

        for (j = 0; j < Data.CheckLstItems.Count; j++)
        {
            CCheckLstItem* check = Data.CheckLstItems[j];
            if (check->Type == cltPropPgSameSize)
            {
                // Scan dialogs
                int maxCX = 0;
                int maxCY = 0;
                for (i = 0; i < DlgData.Count; i++)
                {
                    CDialogData* dialog = DlgData[i];
                    if (check->ContainsDlgID(dialog->ID))
                    {
                        if (dialog->TCX > maxCX)
                            maxCX = dialog->TCX;
                        if (dialog->TCY > maxCY)
                            maxCY = dialog->TCY;
                    }
                }
                for (i = 0; i < DlgData.Count; i++)
                {
                    CDialogData* dialog = DlgData[i];
                    if (check->ContainsDlgID(dialog->ID) && (dialog->TCX != maxCX || dialog->TCY != maxCY))
                    {
                        swprintf_s(buff, L"Dialog %hs: size should be CX=%d, CY=%d)", DataRH.GetIdentifier(dialog->ID), maxCX, maxCY);
                        OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID);
                    }
                }
            }
        }
    }

    if (Config.ValidateHButtonSpacing)
    {
        // display intro
        swprintf_s(buff, L"Searching for wrong horizontal button spacing...");
        OutWindow.AddLine(buff, mteInfo);

        //TRACE_E("TODO: ValidateHButtonSpacing");
        // Scan dialogs
        //for (i = 0; i < DlgData.Count; i++)
        //{
        //  CDialogData *dialog = DlgData[i];
        //  TIndirectArray<CControl> bottomButtons(10, 10, dtNoDelete);
        //  dialog->GetBottomButtons(&bottomButtons);
        //}
    }

    if (Config.ValidateDialogControlsID)
    {
        // display intro
        swprintf_s(buff, L"Searching dialog boxes for ID conflicts...");
        OutWindow.AddLine(buff, mteInfo);

        // Scan dialogs
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];

            for (j = 1; j < dialog->Controls.Count; j++) // 0 is the dialog title, so start from 1
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                    continue;

                BOOL conflict = FALSE;
                int k;
                for (k = j + 1; k < dialog->Controls.Count; k++)
                {
                    if (control->ID == dialog->Controls[k]->ID)
                    {
                        conflict = TRUE;
                        break;
                    }
                }

                if (conflict)
                {
                    swprintf_s(buff, L"Dialog %hs: child control ID conflict on %hs", DataRH.GetIdentifier(dialog->ID), DataRH.GetIdentifier(control->ID));
                    BOOL useLVIndex = dialog->GetControlIndexInLV(j, &lvIndex);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex : 0,
                                      useLVIndex ? 0 : control->ID);

                    swprintf_s(buff, L"... with %hs", DataRH.GetIdentifier(dialog->Controls[k]->ID));
                    int lvIndex2;
                    useLVIndex = dialog->GetControlIndexInLV(k, &lvIndex2);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, useLVIndex ? lvIndex2 : 0,
                                      useLVIndex ? 0 : dialog->Controls[k]->ID);
                }
            }
        }
    }

    // Find IDs without text identifiers (detect missing symbols in the .inc file)
    OutWindow.AddLine(L"Searching for unknown identifiers...", mteInfo);
    CheckIdentifiers(TRUE);

    if (Relayout.Count > 0)
    {
        swprintf_s(buff, L"You need to relayout following dialogs...");
        OutWindow.AddLine(buff, mteInfo);
        for (i = 0; i < Relayout.Count; i++)
        {
            swprintf_s(buff, L"Dialog %hs", DataRH.GetIdentifier(Relayout[i]));
            OutWindow.AddLine(buff, mteError, rteDialog, Relayout[i]);
        }
    }

    // display outro
    int count = OutWindow.GetErrorLines();
    if (count == 0)
        swprintf_s(buff, L"All validations passed OK.");
    else
        swprintf_s(buff, L"%d problem(s) have been found.", count);
    OutWindow.AddLine(buff, mteSummary);
    if (count == 0)
        PostMessage(FrameWindow.HWindow, WM_FOCLASTINOUTPUTWND, 0, 0); // The Output window might not exist yet
    else
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_WND_OUTPUT, 0); // Activate the Output window

    OutWindow.EnablePaint(TRUE);

    SetCursor(hOldCursor);
}

void CData::LookForIdConflicts()
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    {
        wchar_t buff[2 * MAX_PATH];
        swprintf_s(buff, L"Looking for ID conflicts...");
        OutWindow.AddLine(buff, mteInfo);
    }

    wchar_t buff[1000];
    OutWindow.EnablePaint(FALSE);

    // Scan dialogs
    for (int i = 0; i < DlgData.Count; i++)
    {
        TDirectArray<DWORD> ids(20, 10);

        CDialogData* dialog = DlgData[i];
        int lvIndex = 0;
        for (int j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            if (!control->ShowInLVWithControls(j))
                continue;

            if (j > 0) // j == 0 means the dialog name and ID
            {
                BOOL displayed = FALSE;
                for (int k = 0; k < ids.Count; k++)
                {
                    if (control->ID == ids[k])
                    {
                        swprintf_s(buff, L"Dialog %hs: conflict on control ID %d", DataRH.GetIdentifier(dialog->ID), control->ID);
                        OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);
                        displayed = TRUE;
                        break;
                    }
                }

                if (!displayed && control->ID == 0xFFFF)
                {
                    swprintf_s(buff, L"Dialog %hs: invalid (-1) control ID %d", DataRH.GetIdentifier(dialog->ID), control->ID);
                    OutWindow.AddLine(buff, mteError, rteDialog, dialog->ID, lvIndex);
                    displayed = TRUE;
                }

                ids.Add(control->ID);
                if (!ids.IsGood())
                {
                    TRACE_E("Low memory");
                    ids.ResetState();
                }
            }
            lvIndex++;
        }
    }

    OutWindow.EnablePaint(TRUE);
    SetCursor(hOldCursor);
}
