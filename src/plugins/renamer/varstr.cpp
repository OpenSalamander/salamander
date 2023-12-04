// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

BOOL CVarString::CVariable::SetArguments(const char* argStart, const char* argEnd,
                                         int& error, const char*& errorPos1, const char*& errorPos2)
{
    CALL_STACK_MESSAGE_NONE
    if (argStart != argEnd)
    {
        error = IDS_EXP_UNEXPECTARG;
        errorPos1 = argStart;
        errorPos2 = argEnd;
        return FALSE;
    }
    return TRUE;
}

class CPlainText : public CVarString::CVariable
{
    char* Text;
    int Length;

public:
    CPlainText(const char* start, const char* end)
    {
        Length = (int)(end - start);
        Text = new char[Length];
        memcpy(Text, start, Length);
    }

    virtual ~CPlainText()
    {
        if (Text)
            delete[] Text;
    }

    virtual int Expand(char*& string, char* end, LPVOID param)
    {
        if (end - string < Length)
            return -1;
        memcpy(string, Text, Length);
        string += Length;
        return Length;
    }
};

// ****************************************************************************
//
// CVarString
//

CVarString::CVarString() : Stack(8, 8)
{
    CALL_STACK_MESSAGE_NONE;
}

BOOL CVarString::Compile(const char* varText, int& error, int& errorPos1,
                         int& errorPos2, CVariableEntry* variables)
{
    CALL_STACK_MESSAGE5("CVarString::Compile(%s, %d, %d, %d, )", varText, error,
                        errorPos1, errorPos2);
    Stack.DestroyMembers();
    Temp.Reserve(strlen(varText));
    char* tmp = Temp.Get();
    const char* src = varText;

    while (*src != 0)
    {
        if (*src == '$')
        {
            if (*++src == 0)
            {
                // $ na konci retezce
                Error = error = IDS_EXP_TRAILINGDOLLAR;
                errorPos2 = errorPos1 = (int)(src - varText);
                return FALSE;
            }

            // test na $$ escape
            if (*src != '$')
            {
                if (*src != '(')
                {
                    // neocekavany znak, muze byt pouze $$ nebo $(
                    Error = error = IDS_EXP_UNEXPECTEDCHAR;
                    errorPos1 = (int)(src - varText);
                    errorPos2 = errorPos1 + 1;
                    return FALSE;
                }

                // jde o promenou, najdeme zaviraci zavorku
                const char* closePar = ++src;
                BOOL quote = FALSE;
                while (*closePar != 0)
                {
                    if (*closePar == '\'')
                        quote = !quote;
                    if (*closePar == ')' && !quote)
                        break;
                    closePar++;
                }

                if (*closePar != ')')
                {
                    Error = error = IDS_EXP_UNMATCHEDPAR;
                    errorPos1 = (int)(src - varText);
                    errorPos2 = (int)(closePar - varText);
                    return FALSE;
                }

                // najdeme ':' oddelujici jmeno promene od argumentu
                const char* colon = src;
                quote = FALSE;
                while (*colon != 0)
                {
                    if (*colon == '\'')
                        quote = !quote;
                    if (*colon == ')' && !quote)
                        break;
                    if (*colon == ':' && !quote)
                        break;
                    colon++;
                }

                if (colon - src == 0)
                {
                    Error = error = IDS_EXP_EMPTYSTR;
                    errorPos2 = errorPos1 = (int)(src - varText + 1);
                    return FALSE;
                }

                // najdeme promenou v seznamu
                CVariableEntry* entry = variables;
                for (; entry->Name; entry++)
                    if (SG->StrICmpEx(entry->Name, (int)strlen(entry->Name), src, (int)(colon - src)) == 0)
                        break;

                if (!entry->Name)
                {
                    Error = error = IDS_EXP_VARNOTFOUND;
                    errorPos1 = (int)(src - varText);
                    errorPos2 = (int)(colon - varText);
                    return FALSE;
                }

                CVariable* variable = entry->Alloc();
                if (*colon == ':')
                    colon++;
                const char *pos1, *pos2;
                if (!variable->SetArguments(colon, closePar, error, pos1, pos2))
                {
                    Error = error;
                    errorPos1 = (int)(pos1 - varText);
                    errorPos2 = (int)(pos2 - varText);
                    delete variable;
                    return FALSE;
                }

                if (tmp > Temp.Get())
                {
                    Stack.Add(new CPlainText(Temp.Get(), tmp));
                    tmp = Temp.Get();
                }
                Stack.Add(variable);

                src = closePar + 1;
                continue;
            }
        }

        // je to normalni text
        *tmp++ = *src++;
    }

    if (tmp > Temp.Get())
    {
        Stack.Add(new CPlainText(Temp.Get(), tmp));
        tmp = Temp.Get();
    }

    Error = 0;
    return TRUE;
}

int CVarString::Execute(char* buffer, int max, LPVOID param)
{
    CALL_STACK_MESSAGE_NONE
    char* string = buffer;
    char* end = buffer + max;

    int i;
    for (i = 0; i < Stack.Count; i++)
        if (Stack[i]->Expand(string, end, param) < 0)
            return -1;

    if (string >= end)
        return -1;

    *string = 0;

    return (int)(string - buffer);
}
