// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// ****************************************************************************
// Boyer-Mooruv algoritmus hledani podretezce
// ****************************************************************************

#pragma once

// search flags
#define sfCaseSensitive 0x01 // 0. bit = 1
#define sfForward 0x02       // 1. bit = 1

// ****************************************************************************

class CSearchData
{
public:
    CSearchData()
    {
        Fail1 = Fail2 = NULL;
        OriginalPattern = NULL;
        Length = 0;
        Pattern = NULL;
        Flags = 0;
    }

    ~CSearchData()
    {
        if (Fail1 != NULL)
            delete[] (Fail1);
        if (Fail2 != NULL)
            delete[] (Fail2);
        if (Pattern != NULL)
            free(Pattern);
        if (OriginalPattern != NULL)
            free(OriginalPattern);
        Pattern = NULL;
        Fail1 = Fail2 = NULL;
        OriginalPattern = NULL;
        Length = 0;
    }

    int GetLength() const { return Length; }
    const char* GetPattern() const { return OriginalPattern; }

    BOOL IsGood() const { return OriginalPattern != NULL &&
                                 Pattern != NULL &&
                                 Fail1 != NULL && Fail2 != NULL; }
    void SetFlags(WORD flags);
    void Set(const char* pattern, WORD flags);
    // pro patterny obsahujici '\0'
    // buffer pattern musi mit delku (length + 1) znaku (kompatibilita se stringy)
    void Set(const char* pattern, const int length, WORD flags);

    inline int SearchForward(const char* text, int length, int start);
    inline int SearchBackward(const char* text, int length);

protected:
    int Minimum(int a, int b) { return (a < b) ? a : b; }
    int Maximum(int a, int b) { return (a > b) ? a : b; }

    int* Fail1;            // fail pole pro akt. pismeno
    int* Fail2;            // fail pole pro vyskyt substringu zprava
    char* OriginalPattern; // puvodni vzorek ke hledani
    char* Pattern;         // vzorek ke hledani v prislusnem tvaru (Flag)
    int Length;            // delka vzorku

private:
    BOOL Initialize(); // vola se jen ze SetFlags

    WORD Flags; // menit pres SetFlags
};

//
// ****************************************************************************
// SearchForward
// vraci pozici Patternu nebo -1
// text - v cem ma hledat
// length - delka stringu text
// start - prvni znak cislovano od 0
//

int CSearchData::SearchForward(const char* text, int length, int start)
{
    int l1 = Length - 1;
    int i, j = l1 + start;
    if (Flags & sfCaseSensitive)
    {
        while (j < length)
        {
            i = l1;
            while (i >= 0 && text[j] == Pattern[i])
            {
                i--;
                j--;
            }
            if (i == -1)
                return j + 1;
            j += Maximum(Fail1[text[j]], Fail2[i]);
        }
    }
    else
    {
        while (j < length)
        {
            i = l1;
            while (i >= 0 && LowerCase[text[j]] == Pattern[i])
            {
                i--;
                j--;
            }
            if (i == -1)
                return j + 1;
            j += Maximum(Fail1[LowerCase[text[j]]], Fail2[i]);
        }
    }
    return -1;
}

//
// ****************************************************************************
// SearchBackward
// vraci pozici Patternu nebo -1
// text - v cem ma hledat
// length - delka stringu text
//

int CSearchData::SearchBackward(const char* text, int length)
{
    int l1 = Length - 1;
    int l2 = length - 1;
    int i, j = l1;
    if (Flags & sfCaseSensitive)
    {
        while (j < length)
        {
            i = l1;
            while (i >= 0 && text[l2 - j] == Pattern[i])
            {
                i--;
                j--;
            }
            if (i == -1)
                return l2 - j - Length;
            j += Maximum(Fail1[text[l2 - j]], Fail2[i]);
        }
    }
    else
    {
        while (j < length)
        {
            i = l1;
            while (i >= 0 && LowerCase[text[l2 - j]] == Pattern[i])
            {
                i--;
                j--;
            }
            if (i == -1)
                return l2 - j - Length;
            j += Maximum(Fail1[LowerCase[text[l2 - j]]], Fail2[i]);
        }
    }
    return -1;
}
