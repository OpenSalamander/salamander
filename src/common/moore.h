// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

// ****************************************************************************
// Boyer-Moore substring search algorithm
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
    // for patterns containing '\0'
    // the pattern buffer must be (length + 1) characters long (for string compatibility)
    void Set(const char* pattern, const int length, WORD flags);

    inline int SearchForward(const char* text, int length, int start);
    inline int SearchBackward(const char* text, int length);

protected:
    int Minimum(int a, int b) { return (a < b) ? a : b; }
    int Maximum(int a, int b) { return (a > b) ? a : b; }

    int* Fail1;            // fail array for the current character
    int* Fail2;            // fail array for substring occurrence from the right
    char* OriginalPattern; // original search pattern
    char* Pattern;         // search pattern in the corresponding flag-dependent form
    int Length;            // pattern length

private:
    BOOL Initialize(); // called only from SetFlags

    WORD Flags; // modify via SetFlags
};

// ****************************************************************************
// SearchForward
// returns the index of Pattern or -1
// text - text to search in
// length - length of the text string
// start - index of the first character, starting at 0

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

// ****************************************************************************
// SearchBackward
// returns the index of Pattern or -1
// text - text to search in
// length - length of the text string

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
