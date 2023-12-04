// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// preskoci mezery, tabelatory a EOLy, vraci TRUE pokud 's' nedospelo ke konci retezce ('end')
BOOL SkipWSAndEOLs(const char*& s, const char* end)
{
    while (s < end && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n'))
        s++;
    return s < end;
}

// preskoci zbytek radky i s EOLem, vraci TRUE pokud 's' nedospelo ke konci retezce ('end')
BOOL SkipRestOfLine(const char*& s, const char* end)
{
    while (s < end && *s != '\r' && *s != '\n')
        s++;
    if (s < end && *s == '\r')
        s++;
    if (s < end && *s == '\n')
        s++;
    return s < end;
}

CFTPParser* CompileParsingRules(const char* rules, TIndirectArray<CSrvTypeColumn>* columns,
                                int* errorPos, int* errorResID, BOOL* lowMem)
{
    CALL_STACK_MESSAGE1("CompileParsingRules()");
    if (errorPos != NULL)
        *errorPos = -1;
    if (errorResID != NULL)
        *errorResID = -1;
    if (lowMem != NULL)
        *lowMem = FALSE;

    BOOL* colAssigned = new BOOL[columns->Count]; // predalokovane pole pro sledovani do jakych sloupcu uz se priradilo
    if (colAssigned == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (lowMem != NULL)
            *lowMem = TRUE;
        return NULL;
    }
    CFTPParser* parser = new CFTPParser;
    if (parser == NULL || !parser->IsGood())
    {
        if (colAssigned != NULL)
            delete[] colAssigned;
        if (parser != NULL)
            delete parser;
        else
            TRACE_E(LOW_MEMORY);
        if (lowMem != NULL)
            *lowMem = TRUE;
        return NULL;
    }

    BOOL error = FALSE;
    const char* rulesBeg = rules;
    const char* rulesEnd = rules + strlen(rules);
    while (rules < rulesEnd)
    {
        if (SkipWSAndEOLs(rules, rulesEnd))
        {
            if (*rules == '#')
                SkipRestOfLine(rules, rulesEnd);
            else
            {
                if (*rules == '*')
                {
                    int i;
                    for (i = 0; i < columns->Count; i++)
                        colAssigned[i] = FALSE; // zadny sloupec neni prirazeny
                    if (!parser->CompileNewRule(rules, rulesEnd, columns, errorResID, lowMem, colAssigned))
                    {
                        error = TRUE;
                        break;
                    }
                }
                else // neocekavany symbol
                {
                    if (errorResID != NULL)
                        *errorResID = IDS_STPAR_ERR_UNEXPSYM;
                    error = TRUE;
                    break;
                }
            }
        }
    }

    if (error)
    {
        if (errorPos != NULL)
            *errorPos = (int)(rules - rulesBeg);
        delete parser;
        parser = NULL;
    }
    if (colAssigned != NULL)
        delete[] colAssigned;
    return parser;
}

//
// ****************************************************************************
// CFTPParser
//

BOOL SkipIdentifier(const char*& s, const char* end, int* errorResID, int emptyErrID)
{
    if (s < end && (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z' || *s == '_'))
    {
        s++;
        while (s < end && (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z' ||
                           *s >= '0' && *s <= '9' || *s == '_'))
            s++;
        return TRUE;
    }
    else
    {
        if (errorResID != NULL)
            *errorResID = emptyErrID;
        return FALSE;
    }
}

CFTPParserFunctionCode FindFunctionCode(const char* name)
{
    static const char* functionNames[] = {
        "skip_white_spaces",
        "white_spaces",
        "white_spaces_and_line_ends",
        "rest_of_line",
        "word",
        "number",
        "positive_number",
        "number_with_separators",
        "month_3",
        "month_txt",
        "month",
        "day",
        "year",
        "time",
        "year_or_time",
        "all",
        "all_to",
        "all_up_to",
        "unix_link",
        "unix_device",
        "if",
        "assign",
        "cut_white_spaces_end",
        "cut_white_spaces_start",
        "cut_white_spaces",
        "back",
        "add_string_to_column",
        "cut_end_of_string",
        "skip_to_number",
    };

    // celkovy pocet funkci (AKTUALIZOVAT !!! + rovnat s CFTPParserFunction + doplnit i functionCodes)
    static const int count = 29;

    static CFTPParserFunctionCode functionCodes[] = {
        fpfSkip_white_spaces,
        fpfWhite_spaces,
        fpfWhite_spaces_and_line_ends,
        fpfRest_of_line,
        fpfWord,
        fpfNumber,
        fpfPositiveNumber,
        fpfNumber_with_separators,
        fpfMonth_3,
        fpfMonth_txt,
        fpfMonth,
        fpfDay,
        fpfYear,
        fpfTime,
        fpfYear_or_time,
        fpfAll,
        fpfAll_to,
        fpfAll_up_to,
        fpfUnix_link,
        fpfUnix_device,
        fpfIf,
        fpfAssign,
        fpfCut_white_spaces_end,
        fpfCut_white_spaces_start,
        fpfCut_white_spaces,
        fpfBack,
        fpfAdd_string_to_column,
        fpfCut_end_of_string,
        fpfSkip_to_number,
    };
    int i;
    for (i = 0; i < count; i++)
        if (_stricmp(functionNames[i], name) == 0)
            return functionCodes[i];
    return fpfNone;
}

BOOL CFTPParser::CompileNewRule(const char*& rules, const char* rulesEnd,
                                TIndirectArray<CSrvTypeColumn>* columns,
                                int* errorResID, BOOL* lowMem, BOOL* colAssigned)
{
    CALL_STACK_MESSAGE1("CFTPParser::CompileNewRule()");
    rules++; // vime ze prisla '*', preskocime ji
    CFTPParserRule* ruleObj = new CFTPParserRule;
    if (ruleObj == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }
    Rules.Add(ruleObj);
    if (!Rules.IsGood())
    {
        delete ruleObj;
        Rules.ResetState();
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }

    BOOL funcExpected = FALSE; // TRUE = je ocekavane volani funkce (za ',' v pravidle)
    while (rules < rulesEnd)
    {
        if (SkipWSAndEOLs(rules, rulesEnd))
        {
            if (*rules == '#')
                SkipRestOfLine(rules, rulesEnd);
            else
            {
                if (*rules == ';')
                {
                    if (funcExpected)
                    {
                        if (errorResID != NULL)
                            *errorResID = IDS_STPAR_ERR_FUNCEXPECTED;
                        return FALSE;
                    }
                    rules++;
                    return TRUE; // timto je pravidlo zkompilovano
                }
                else // tady musi zacinat funkce
                {
                    const char* beg = rules;
                    if (SkipIdentifier(rules, rulesEnd, errorResID, IDS_STPAR_ERR_INVALIDFUNCNAME))
                    {
                        funcExpected = FALSE; // uz jsme se funkce dockali
                        char functionName[100];
                        lstrcpyn(functionName, beg, (int)min(100, rules - beg + 1));
                        CFTPParserFunctionCode func = FindFunctionCode(functionName);
                        if (func == fpfNone)
                        {
                            if (errorResID != NULL)
                                *errorResID = IDS_STPAR_ERR_UNKNOWNFUNC;
                            return FALSE;
                        }
                        BOOL parFound = FALSE;
                        while (rules < rulesEnd)
                        {
                            if (SkipWSAndEOLs(rules, rulesEnd))
                            {
                                if (*rules == '#')
                                    SkipRestOfLine(rules, rulesEnd);
                                else
                                {
                                    if (*rules == '(') // tady zacinaji parametry funkce
                                    {
                                        parFound = TRUE;
                                        if (!ruleObj->CompileNewFunction(func, rules, rulesEnd, columns, errorResID, lowMem, colAssigned))
                                            return FALSE;

                                        while (rules < rulesEnd)
                                        {
                                            if (SkipWSAndEOLs(rules, rulesEnd))
                                            {
                                                if (*rules == '#')
                                                    SkipRestOfLine(rules, rulesEnd);
                                                else
                                                {
                                                    if (*rules == ',')
                                                    {
                                                        rules++;
                                                        funcExpected = TRUE;
                                                    }
                                                    else
                                                    {
                                                        if (*rules != ';')
                                                        {
                                                            if (errorResID != NULL)
                                                                *errorResID = IDS_STPAR_ERR_UNEXPSYM;
                                                            return FALSE;
                                                        }
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        if (!parFound)
                        {
                            if (errorResID != NULL)
                                *errorResID = IDS_STPAR_ERR_MISSINGFUNCPARS;
                            return FALSE;
                        }
                    }
                    else
                        return FALSE;
                }
            }
        }
    }
    if (errorResID != NULL)
        *errorResID = IDS_STPAR_ERR_MISSINGRULEEND;
    return FALSE;
}

//
// ****************************************************************************
// CFTPParserRule
//

BOOL FindColumnIndex(const char* columnName, TIndirectArray<CSrvTypeColumn>* columns,
                     int* columnIndex, int* errorResID)
{
    if (_stricmp(columnName, "is_dir") == 0)
        *columnIndex = COL_IND_ISDIR;
    else
    {
        if (_stricmp(columnName, "is_hidden") == 0)
            *columnIndex = COL_IND_ISHIDDEN;
        else
        {
            if (_stricmp(columnName, "is_link") == 0)
                *columnIndex = COL_IND_ISLINK;
            else
            {
                int i;
                for (i = 0; i < columns->Count; i++)
                {
                    if (_stricmp(columnName, HandleNULLStr(columns->At(i)->ID)) == 0)
                    {
                        if (columns->At(i)->Type == stctExt || columns->At(i)->Type == stctType)
                        {
                            if (errorResID != NULL)
                                *errorResID = IDS_STPAR_ERR_GENERCOLUMN;
                            return FALSE;
                        }
                        *columnIndex = i;
                        return TRUE;
                    }
                }
                if (errorResID != NULL)
                    *errorResID = IDS_STPAR_ERR_UNKNOWNCOLUMN;
                return FALSE; // nenalezeno = neznamy sloupec
            }
        }
    }
    return TRUE;
}

BOOL FindStateVarOrBool(const char* name, CFTPParserStateVariables* var, int* boolVal)
{
    static const char* names[] = {
        "false",
        "true",
        "first_nonempty_line",
        "last_nonempty_line",
        "next_char",
        "next_word",
        "rest_of_line",
    };

    // celkovy pocet funkci (AKTUALIZOVAT !!! + rovnat s CFTPParserStateVariables + doplnit i stateVarCodes)
    static const int count = 7;

    static CFTPParserStateVariables stateVarCodes[] = {
        psvFirstNonEmptyLine,
        psvLastNonEmptyLine,
        psvNextChar,
        psvNextWord,
        psvRestOfLine,
    };
    int i;
    for (i = 0; i < count; i++)
    {
        if (_stricmp(names[i], name) == 0)
        {
            if (i < 2)
                *boolVal = (i == 1);
            else
                *var = stateVarCodes[i - 2];
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CFTPParserRule::CompileNewFunction(CFTPParserFunctionCode func, const char*& rules,
                                        const char* rulesEnd, TIndirectArray<CSrvTypeColumn>* columns,
                                        int* errorResID, BOOL* lowMem, BOOL* colAssigned)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE2("CFTPParserRule::CompileNewFunction(%d)", (int)func);
    rules++; // vime ze prisla '(', preskocime ji
    CFTPParserFunction* funcObj = new CFTPParserFunction(func);
    if (funcObj == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }
    Functions.Add(funcObj);
    if (!Functions.IsGood())
    {
        delete funcObj;
        Functions.ResetState();
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }

    BOOL parExpected = FALSE;          // TRUE pokud je ocekavan dalsi parametr (za ',' v parametrech)
    BOOL canBeExpression = TRUE;       // FALSE pokud se jiz vyraz nacita (umime jen jednoduche vyrazy)
    BOOL rightOperandExpected = FALSE; // TRUE pokud je ocekavan pravy operand (ve vyrazu za bin. operatorem)
    while (rules < rulesEnd)
    {
        if (SkipWSAndEOLs(rules, rulesEnd))
        {
            if (*rules == '#')
                SkipRestOfLine(rules, rulesEnd);
            else
            {
                if (*rules == ')' || *rules == ',')
                {
                    if (rightOperandExpected)
                    {
                        if (errorResID != NULL)
                            *errorResID = IDS_STPAR_ERR_RIGHTPAREXPECT;
                        return FALSE;
                    }
                    if (*rules == ',' || parExpected)
                    {
                        if (errorResID != NULL)
                            *errorResID = IDS_STPAR_ERR_PAREXPECTED;
                        return FALSE;
                    }
                    rules++;
                    // provedeme jeste kontrolu typu a poctu parametru pouzivanych zkompilovanou funkci
                    if (!funcObj->CheckParameters(columns, errorResID, colAssigned))
                        return FALSE;
                    return TRUE; // timto je funkce zkompilovana
                }
                else // tady musi zacinat parametr
                {
                    parExpected = FALSE; // uz jsme se parametru dockali

                    if (*rules == '<') // idenfitikator sloupce nebo prvni cast vyrazu
                    {
                        rules++;
                        BOOL colNameFound = FALSE;
                        int columnIndex = -50; // jen pro ladici ucely
                        while (rules < rulesEnd)
                        {
                            if (SkipWSAndEOLs(rules, rulesEnd))
                            {
                                if (*rules == '#')
                                    SkipRestOfLine(rules, rulesEnd);
                                else
                                {
                                    const char* beg = rules;
                                    if (SkipIdentifier(rules, rulesEnd, errorResID, IDS_STPAR_ERR_INVALIDCOLUMNID))
                                    {
                                        char columnName[STC_ID_MAX_SIZE];
                                        lstrcpyn(columnName, beg, (int)min(STC_ID_MAX_SIZE, rules - beg + 1));
                                        if (!FindColumnIndex(columnName, columns, &columnIndex, errorResID))
                                            return FALSE;

                                        // preskocime jeste ukoncujici '>'
                                        BOOL colEndFound = FALSE;
                                        while (rules < rulesEnd)
                                        {
                                            if (SkipWSAndEOLs(rules, rulesEnd))
                                            {
                                                if (*rules == '#')
                                                    SkipRestOfLine(rules, rulesEnd);
                                                else
                                                {
                                                    if (*rules == '>')
                                                    {
                                                        rules++;
                                                        colEndFound = TRUE;
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                        if (!colEndFound)
                                        {
                                            if (errorResID != NULL)
                                                *errorResID = IDS_STPAR_ERR_MISSINGCOLIDEND;
                                            return FALSE;
                                        }
                                        colNameFound = TRUE;
                                        break;
                                    }
                                    else
                                        return FALSE;
                                }
                            }
                        }
                        if (!colNameFound)
                        {
                            if (errorResID != NULL)
                                *errorResID = IDS_STPAR_ERR_MISSINGCOLUMNID;
                            return FALSE;
                        }
                        if (!funcObj->AddColumnIDParameter(columnIndex, lowMem))
                            return FALSE;
                    }
                    else // retezec, boolean, stavova promenna, cislo nebo prvni cast vyrazu
                    {
                        if (*rules == '"') // retezec nebo prvni cast vyrazu
                        {
                            rules++;
                            const char* beg = rules;
                            int esc = 0; // pocet escape sekvenci
                            BOOL strEndFound = FALSE;
                            while (rules < rulesEnd)
                            {
                                if (*rules == '"') // konec retezce
                                {
                                    strEndFound = TRUE;
                                    break;
                                }
                                if (*rules == '\r' || *rules == '\n')
                                    break;                                  // retezec nemuze obsahovat primo EOL (pouzite escape sekvence '\r' a '\n')
                                if (*rules == '\\' && rules + 1 < rulesEnd) // escape sekvence
                                {
                                    rules++;
                                    esc++;
                                    if (*rules != '"' && *rules != '\\' && *rules != 't' &&
                                        *rules != 'r' && *rules != 'n')
                                    {
                                        if (errorResID != NULL)
                                            *errorResID = IDS_STPAR_ERR_STRUNKNESCSEQ;
                                        return FALSE;
                                    }
                                }
                                rules++;
                            }
                            if (!strEndFound)
                            {
                                if (errorResID != NULL)
                                    *errorResID = IDS_STPAR_ERR_MISSINGSTREND;
                                return FALSE;
                            }

                            char* str = (char*)malloc((rules - beg) - esc + 1);
                            if (str == NULL)
                            {
                                TRACE_E(LOW_MEMORY);
                                if (lowMem != NULL)
                                    *lowMem = TRUE;
                                return FALSE;
                            }
                            char* s = str;
                            while (beg < rules)
                            {
                                if (*beg != '\\')
                                    *s++ = *beg++;
                                else
                                {
                                    if (beg < rules)
                                        beg++; // "always true"
                                    switch (*beg)
                                    {
                                    case '"':
                                        *s++ = '"';
                                        break;
                                    case '\\':
                                        *s++ = '\\';
                                        break;
                                    case 't':
                                        *s++ = '\t';
                                        break;
                                    case 'r':
                                        *s++ = '\r';
                                        break;
                                    case 'n':
                                        *s++ = '\n';
                                        break;
                                    default:
                                        TRACE_E("Escape sequence '\\" << *beg << "' is not implemented!");
                                    }
                                    beg++;
                                }
                            }
                            *s = 0;
                            rules++; // preskocime koncovou '"' retezce
                            if (!funcObj->AddStringParameter(str, lowMem))
                                return FALSE;
                        }
                        else // boolean, stavova promenna, cislo nebo prvni cast vyrazu
                        {
                            if (*rules >= '0' && *rules <= '9' || *rules == '-' || *rules == '+') // cislo nebo prvni cast vyrazu
                            {
                                __int64 num = 0;
                                BOOL minus = FALSE;
                                if (*rules == '+')
                                    rules++;
                                else
                                {
                                    if (*rules == '-')
                                    {
                                        minus = TRUE;
                                        rules++;
                                    }
                                }
                                if (rules < rulesEnd && *rules >= '0' && *rules <= '9')
                                {
                                    num = *rules++ - '0';
                                    while (rules < rulesEnd && *rules >= '0' && *rules <= '9')
                                    {
                                        num = num * 10 + (*rules - '0');
                                        rules++;
                                    }
                                    if (minus)
                                        num = -num;
                                    if (!funcObj->AddNumberParameter(num, lowMem))
                                        return FALSE;
                                }
                                else
                                {
                                    if (errorResID != NULL)
                                        *errorResID = IDS_STPAR_ERR_INVALIDNUMBER;
                                    return FALSE;
                                }
                            }
                            else // boolean, stavova promenna nebo prvni cast vyrazu
                            {
                                const char* beg = rules;
                                if (SkipIdentifier(rules, rulesEnd, errorResID, IDS_STPAR_ERR_INVALVARORBOOL))
                                {
                                    char name[100];
                                    lstrcpyn(name, beg, (int)min(100, rules - beg + 1));
                                    CFTPParserStateVariables var = psvNone;
                                    int boolVal = -1;
                                    if (!FindStateVarOrBool(name, &var, &boolVal))
                                    {
                                        if (errorResID != NULL)
                                            *errorResID = IDS_STPAR_ERR_UNKNOWNSTVAR;
                                        return FALSE;
                                    }
                                    if (var != psvNone && !funcObj->AddStateVarParameter(var, lowMem) ||
                                        boolVal != -1 && !funcObj->AddBooleanParameter(boolVal != 0, lowMem))
                                    {
                                        return FALSE;
                                    }
                                }
                                else
                                    return FALSE;
                            }
                        }
                    }

                    if (rightOperandExpected) // slo o pravy operand vyrazu?
                    {
                        if (!funcObj->MoveRightOperandToExpression(columns, errorResID,
                                                                   lowMem, colAssigned))
                        {
                            return FALSE;
                        }
                        rightOperandExpected = FALSE;
                    }

                    // podivame se po dalsim parametru nebo po operatoru vyrazu
                    while (rules < rulesEnd)
                    {
                        if (SkipWSAndEOLs(rules, rulesEnd))
                        {
                            if (*rules == '#')
                                SkipRestOfLine(rules, rulesEnd);
                            else
                            {
                                // zjistime jestli nejde o vyraz (najdeme-li operator, je to vyraz)
                                CFTPParserBinaryOperators oper = pboNone;
                                int opCodeLen = 2;
                                if (rules + 1 < rulesEnd)
                                {
                                    if (_strnicmp(rules, "==", 2) == 0)
                                        oper = pboEqual;
                                    else
                                    {
                                        if (_strnicmp(rules, "!=", 2) == 0)
                                            oper = pboNotEqual;
                                        else
                                        {
                                            if (_strnicmp(rules, "in", 2) == 0)
                                                oper = pboSubStrIsInString;
                                            else
                                            {
                                                if (_strnicmp(rules, "eq", 2) == 0)
                                                    oper = pboStrEqual;
                                                else
                                                {
                                                    opCodeLen = 6;
                                                    if (rules + 5 < rulesEnd)
                                                    {
                                                        if (_strnicmp(rules, "not_eq", 6) == 0)
                                                            oper = pboStrNotEqual;
                                                        else
                                                        {
                                                            if (_strnicmp(rules, "not_in", 6) == 0)
                                                                oper = pboSubStrIsNotInString;
                                                            else
                                                            {
                                                                opCodeLen = 8;
                                                                if (rules + 7 < rulesEnd)
                                                                {
                                                                    if (_strnicmp(rules, "end_with", 8) == 0)
                                                                        oper = pboStrEndWithString;
                                                                    else
                                                                    {
                                                                        opCodeLen = 12;
                                                                        if (rules + 11 < rulesEnd)
                                                                        {
                                                                            if (_strnicmp(rules, "not_end_with", 12) == 0)
                                                                                oper = pboStrNotEndWithString;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (oper != pboNone)
                                    {
                                        if (!canBeExpression)
                                        {
                                            if (errorResID != NULL)
                                                *errorResID = IDS_STPAR_ERR_UNSUPEXPRESSION;
                                            return FALSE;
                                        }

                                        rules += opCodeLen;
                                        if (!funcObj->AddExpressionParameter(oper, lowMem))
                                            return FALSE;
                                        rightOperandExpected = TRUE;
                                    }
                                    canBeExpression = FALSE;
                                }
                                if (oper == pboNone)
                                {
                                    if (*rules == ',')
                                    {
                                        rules++;
                                        parExpected = TRUE;
                                        canBeExpression = TRUE;
                                    }
                                    else
                                    {
                                        if (*rules != ')')
                                        {
                                            if (errorResID != NULL)
                                                *errorResID = IDS_STPAR_ERR_UNEXPSYORUNKNOP;
                                            return FALSE;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    if (errorResID != NULL)
        *errorResID = IDS_STPAR_ERR_MISSINGPAREND;
    return FALSE;
}

//
// ****************************************************************************
// CFTPParserFunction
//

BOOL CFTPParserFunction::AddParameter(CFTPParserParameter*& newPar, BOOL* lowMem)
{
    newPar = new CFTPParserParameter;
    if (newPar == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }
    Parameters.Add(newPar);
    if (!Parameters.IsGood())
    {
        delete newPar;
        Parameters.ResetState();
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }
    return TRUE;
}

BOOL CFTPParserFunction::AddColumnIDParameter(int columnIndex, BOOL* lowMem)
{
    CFTPParserParameter* par;
    if (AddParameter(par, lowMem))
    {
        par->Type = pptColumnID;
        par->ColumnIndex = columnIndex;
        return TRUE;
    }
    return FALSE;
}

BOOL CFTPParserFunction::AddStringParameter(char* str, BOOL* lowMem)
{
    CFTPParserParameter* par;
    if (AddParameter(par, lowMem))
    {
        par->Type = pptString;
        par->String = str;
        return TRUE;
    }
    else
        free(str);
    return FALSE;
}

BOOL CFTPParserFunction::AddNumberParameter(__int64 number, BOOL* lowMem)
{
    CFTPParserParameter* par;
    if (AddParameter(par, lowMem))
    {
        par->Type = pptNumber;
        par->Number = number;
        return TRUE;
    }
    return FALSE;
}

BOOL CFTPParserFunction::AddStateVarParameter(CFTPParserStateVariables var, BOOL* lowMem)
{
    CFTPParserParameter* par;
    if (AddParameter(par, lowMem))
    {
        par->Type = pptStateVar;
        par->StateVar = var;
        return TRUE;
    }
    return FALSE;
}

BOOL CFTPParserFunction::AddBooleanParameter(BOOL boolVal, BOOL* lowMem)
{
    CFTPParserParameter* par;
    if (AddParameter(par, lowMem))
    {
        par->Type = pptBoolean;
        par->Boolean = boolVal;
        return TRUE;
    }
    return FALSE;
}

BOOL CFTPParserFunction::AddExpressionParameter(CFTPParserBinaryOperators oper, BOOL* lowMem)
{
    if (Parameters.Count == 0)
    {
        TRACE_E("Unexpected situation in CFTPParserFunction::AddExpressionParameter().");
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }
    CFTPParserParameter** arr = new CFTPParserParameter*[2];
    if (arr == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }
    arr[0] = Parameters[Parameters.Count - 1];
    arr[1] = NULL; // zatim, pravy parametr nacteme az casem
    Parameters.Detach(Parameters.Count - 1);
    if (!Parameters.IsGood())
        Parameters.ResetState(); // detach vzdy funguje, ale muze hlasit chybu pameti

    CFTPParserParameter* par;
    if (AddParameter(par, lowMem))
    {
        par->Type = pptExpression;
        par->BinOperator = oper;
        par->Parameters = arr;
        return TRUE;
    }
    else
    {
        delete arr[0];
        delete[] arr;
    }
    return FALSE;
}

BOOL CFTPParserFunction::MoveRightOperandToExpression(TIndirectArray<CSrvTypeColumn>* columns,
                                                      int* errorResID, BOOL* lowMem, BOOL* colAssigned)
{
    CFTPParserParameter* oper;
    if (Parameters.Count < 2 || (oper = Parameters[Parameters.Count - 2])->Type != pptExpression ||
        oper->Parameters[1] != NULL || oper->Parameters[0] == NULL)
    {
        TRACE_E("Unexpected situation in CFTPParserFunction::MoveRightOperandToExpression().");
        if (lowMem != NULL)
            *lowMem = TRUE;
        return FALSE;
    }
    oper->Parameters[1] = Parameters[Parameters.Count - 1];
    Parameters.Detach(Parameters.Count - 1);
    if (!Parameters.IsGood())
        Parameters.ResetState(); // detach vzdy funguje, ale muze hlasit chybu pameti

    BOOL ok = FALSE;
    CFTPParserOperandType left = oper->Parameters[0]->GetOperandType(columns);
    CFTPParserOperandType right = oper->Parameters[1]->GetOperandType(columns);
    if (left != potNone && right != potNone)
    {
        switch (oper->BinOperator)
        {
        case pboEqual:
        case pboNotEqual:
        {
            ok = (left == right);
            break;
        }

        case pboStrEqual:
        case pboStrNotEqual:
        case pboSubStrIsInString:
        case pboSubStrIsNotInString:
        case pboStrEndWithString:
        case pboStrNotEndWithString:
        {
            ok = (left == potString && right == potString);
            break;
        }

        default:
            TRACE_E("Unexpected operator in CFTPParserFunction::MoveRightOperandToExpression()");
            break;
        }
    }
    int errCode = IDS_STPAR_ERR_UNEXPTYPESINEXP;
    if (ok) // jeste otestujeme jestli necte sloupce, do kterych se zatim nepriradila zadna hodnota
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            if (oper->Parameters[i]->Type == pptColumnID)
            {
                int index = oper->Parameters[i]->ColumnIndex;
                if (index >= 0 && index < columns->Count) // neni to is_dir, is_hidden ani is_link
                {
                    CSrvTypeColumnTypes type = columns->At(index)->Type;
                    if (type == stctDate || type == stctGeneralDate)
                    {
                        if (colAssigned[index] != 7 /* day + month + year */)
                            ok = FALSE;
                    }
                    else
                    {
                        if (!colAssigned[index])
                            ok = FALSE;
                    }
                    if (!ok)
                    {
                        errCode = IDS_STPAR_ERR_BADUSEOFCOLID;
                        break; // koncime, jedna chyba staci
                    }
                }
            }
        }
    }
    if (!ok && errorResID != NULL)
        *errorResID = errCode;
    return ok;
}

BOOL IsMonthThreeLetterString(const char* str, int* errCode)
{
    if (str != NULL && strlen(str) == 47)
    {
        int i;
        for (i = 0; i < 11; i++) // test na 11 mezer (za poslednim mesicem neni)
        {
            if (str[3] > ' ')
            {
                *errCode = IDS_STPAR_ERR_BADMONTH3STR;
                return FALSE;
            }
            str += 4;
        }
        return TRUE;
    }
    *errCode = IDS_STPAR_ERR_BADMONTH3STR;
    return FALSE;
}

BOOL IsMonthTxtLetterString(const char* str, int* errCode)
{
    if (str != NULL)
    {
        int i;
        for (i = 0; i < 12; i++) // test na 12 mesicu oddelenych mezerami
        {
            if (*str != 0 && *str != ' ')
                str++;
            else
            {
                *errCode = IDS_STPAR_ERR_BADMONTHTXTSTR;
                return FALSE;
            }
            while (*str != 0 && *str != ' ')
                str++;
            if (i < 11 && *str == 0 || i == 11 && *str == ' ')
            {
                *errCode = IDS_STPAR_ERR_BADMONTHTXTSTR;
                return FALSE;
            }
            if (i < 11)
                str++;
        }
        return TRUE;
    }
    *errCode = IDS_STPAR_ERR_BADMONTHTXTSTR;
    return FALSE;
}

BOOL CFTPParserFunction::CheckParameters(TIndirectArray<CSrvTypeColumn>* columns, int* errorResID,
                                         BOOL* colAssigned)
{
    BOOL ok = FALSE;
    CFTPParserFuncParType first_par = pfptNone;
    CFTPParserFuncParType second_par = pfptNone;
    CFTPParserFuncParType third_par = pfptNone;
    if (Parameters.Count > 0)
        first_par = Parameters[0]->GetFuncParType(columns);
    if (Parameters.Count > 1)
        second_par = Parameters[1]->GetFuncParType(columns);
    if (Parameters.Count > 2)
        third_par = Parameters[2]->GetFuncParType(columns);
    int errCode = IDS_STPAR_ERR_BADPARINFUNC;
    switch (Function)
    {
    // bez parametru
    case fpfSkip_white_spaces:
    case fpfSkip_to_number:
    case fpfWhite_spaces_and_line_ends:
        ok = Parameters.Count == 0;
        break;

    // bez parametru nebo cislo
    case fpfWhite_spaces:
    {
        ok = Parameters.Count == 0 ||
             Parameters.Count == 1 && first_par == pfptNumber;
        break;
    }

    // bez parametru nebo sloupec_retezec
    case fpfRest_of_line:
    case fpfWord:
    {
        ok = Parameters.Count == 0;
        if (Parameters.Count == 1 && first_par == pfptColumnString)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // sloupec_cislo
    case fpfNumber:
    case fpfPositiveNumber:
    {
        if (Parameters.Count == 1 && first_par == pfptColumnNumber)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // sloupec_cislo a retezec
    case fpfNumber_with_separators:
    {
        if (Parameters.Count == 2 && first_par == pfptColumnNumber && second_par == pfptString)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // sloupec_datum + pripadne retezec s mesici
    case fpfMonth_3:
    {
        if (Parameters.Count == 1 && first_par == pfptColumnDate ||
            Parameters.Count == 2 && first_par == pfptColumnDate && second_par == pfptString &&
                Parameters[1]->Type == pptString && IsMonthThreeLetterString(Parameters[1]->String, &errCode))
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] |= 2 /* month */;
        }
        break;
    }

    // sloupec_datum + pripadne retezec s mesici
    case fpfMonth_txt:
    {
        if (Parameters.Count == 1 && first_par == pfptColumnDate ||
            Parameters.Count == 2 && first_par == pfptColumnDate && second_par == pfptString &&
                Parameters[1]->Type == pptString && IsMonthTxtLetterString(Parameters[1]->String, &errCode))
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] |= 2 /* month */;
        }
        break;
    }

    // sloupec_datum
    case fpfMonth:
    case fpfDay:
    case fpfYear:
    {
        if (Parameters.Count == 1 && first_par == pfptColumnDate)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] |= (Function == fpfDay ? 1 /* day */ : (Function == fpfYear ? 4 /* year */ : 2 /* month */));
        }
        break;
    }

    // sloupec_cas
    case fpfTime:
    {
        if (Parameters.Count == 1 && first_par == pfptColumnTime)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // sloupec_datum a sloupec_cas
    case fpfYear_or_time:
    {
        if (Parameters.Count == 2 && first_par == pfptColumnDate && second_par == pfptColumnTime)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] |= 4 /* year */;
            // i = Parameters[1]->ColumnIndex;  // sloupec s casem se nemusi naplnit, pokud je k dispozici jen rok
            // if (i >= 0 && i < columns->Count) colAssigned[i] = TRUE;
        }
        break;
    }

    // cislo nebo sloupec_retezec a cislo
    case fpfAll:
    {
        ok = Parameters.Count == 1 && first_par == pfptNumber;
        if (Parameters.Count == 2 && first_par == pfptColumnString && second_par == pfptNumber)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // retezec nebo sloupec_retezec a retezec
    case fpfAll_to:
    {
        ok = Parameters.Count == 1 && first_par == pfptString;
        if (Parameters.Count == 2 && first_par == pfptColumnString && second_par == pfptString)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // sloupec_retezec a retezec
    case fpfAll_up_to:
    {
        if (Parameters.Count == 2 && first_par == pfptColumnString && second_par == pfptString)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // sloupec_is_dir, sloupec_name, sloupec_retezec
    case fpfUnix_link:
    {
        if (Parameters.Count == 3 && first_par == pfptColumnBoolean &&
            second_par == pfptColumnString && third_par == pfptColumnString &&
            Parameters[0]->ColumnIndex == COL_IND_ISDIR && // sloupec_is_dir
            Parameters[1]->ColumnIndex >= 0 && Parameters[1]->ColumnIndex < columns->Count &&
            columns->At(Parameters[1]->ColumnIndex)->Type == stctName) // sloupec_name
        {
            ok = TRUE;
            int i = Parameters[1]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
            // i = Parameters[2]->ColumnIndex;  // cil linku nemusi byt pristupny (muze zustat prazdny)
            // if (i >= 0 && i < columns->Count) colAssigned[i] = TRUE;
        }
        break;
    }

    // sloupec_retezec
    case fpfUnix_device:
    {
        if (Parameters.Count == 1 && first_par == pfptColumnString)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count)
                colAssigned[i] = TRUE;
        }
        break;
    }

    // vyraz_boolean
    case fpfIf:
    {
        ok = Parameters.Count == 1 && (first_par == pfptBoolean || first_par == pfptColumnBoolean);
        if (ok && first_par == pfptColumnBoolean)
        {
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count && !colAssigned[i]) // cteni ze sloupce bez prirazene hodnoty
            {
                ok = FALSE;
                errCode = IDS_STPAR_ERR_BADUSEOFCOLID;
            }
        }

        break;
    }

    // sloupec, vyraz_hodnota (typ sloupce musi odpovidat hodnote)
    case fpfAssign:
    {
        if (Parameters.Count == 2)
        {
            ok = first_par == pfptColumnBoolean && (first_par == second_par || second_par == pfptBoolean) ||
                 first_par == pfptColumnString && (first_par == second_par || second_par == pfptString) ||
                 first_par == pfptColumnNumber && (first_par == second_par || second_par == pfptNumber) ||
                 first_par == pfptColumnDate && first_par == second_par ||
                 first_par == pfptColumnTime && first_par == second_par;
            if (ok)
            {
                if (second_par == pfptColumnBoolean ||
                    second_par == pfptColumnString ||
                    second_par == pfptColumnNumber ||
                    second_par == pfptColumnTime)
                {
                    int i = Parameters[1]->ColumnIndex;
                    if (i >= 0 && i < columns->Count && !colAssigned[i]) // cteni ze sloupce bez prirazene hodnoty
                    {
                        ok = FALSE;
                        errCode = IDS_STPAR_ERR_BADUSEOFCOLID;
                    }
                }
                else
                {
                    if (second_par == pfptColumnDate)
                    {
                        int i = Parameters[1]->ColumnIndex;
                        if (i >= 0 && i < columns->Count && colAssigned[i] != 7 /* day + month + year */) // cteni ze sloupce bez prirazene hodnoty
                        {
                            ok = FALSE;
                            errCode = IDS_STPAR_ERR_BADUSEOFCOLID;
                        }
                    }
                }
            }
        }
        break;
    }

    // sloupec_retezec
    case fpfCut_white_spaces_end:
    case fpfCut_white_spaces_start:
    case fpfCut_white_spaces:
    {
        if (Parameters.Count == 1 && first_par == pfptColumnString)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count && !colAssigned[i]) // cteni ze sloupce bez prirazene hodnoty
            {
                ok = FALSE;
                errCode = IDS_STPAR_ERR_BADUSEOFCOLID;
            }
        }
        break;
    }

    // cislo
    case fpfBack:
        ok = Parameters.Count == 1 && first_par == pfptNumber;
        break;

    // sloupec_retezec, vyraz_hodnota_retezec
    case fpfAdd_string_to_column:
    {
        if (Parameters.Count == 2 &&
            first_par == pfptColumnString && (first_par == second_par || second_par == pfptString))
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            int j = (second_par == pfptColumnString ? Parameters[1]->ColumnIndex : -1000);
            if (i >= 0 && i < columns->Count && !colAssigned[i] || // cteni ze sloupce bez prirazene hodnoty
                j >= 0 && j < columns->Count && !colAssigned[j])
            {
                ok = FALSE;
                errCode = IDS_STPAR_ERR_BADUSEOFCOLID;
            }
        }
        break;
    }

    case fpfCut_end_of_string:
    {
        if (Parameters.Count == 2 && first_par == pfptColumnString && second_par == pfptNumber)
        {
            ok = TRUE;
            int i = Parameters[0]->ColumnIndex;
            if (i >= 0 && i < columns->Count && !colAssigned[i]) // cteni ze sloupce bez prirazene hodnoty
            {
                ok = FALSE;
                errCode = IDS_STPAR_ERR_BADUSEOFCOLID;
            }
        }
        break;
    }

    default:
        TRACE_E("Unexpected function type in CFTPParserFunction::CheckParameters()!");
        break;
    }
    if (!ok && errorResID != NULL)
        *errorResID = errCode;
    return ok;
}

//
// ****************************************************************************
// CFTPParserParameter
//

CFTPParserOperandType
CFTPParserParameter::GetOperandType(TIndirectArray<CSrvTypeColumn>* columns)
{
    if (Type == pptExpression)
    {
        TRACE_E("Unexpected situation in CFTPParserParameter::GetOperandType()!");
        return potNone;
    }
    else
    {
        if (Type == pptColumnID) // u sloupce zjistime typ podle typu hodnot ve sloupci
        {
            if (ColumnIndex >= 0 && ColumnIndex < columns->Count)
            {
                CSrvTypeColumnTypes t = columns->At(ColumnIndex)->Type;
                switch (t)
                {
                // case stctExt:   // chyba uz byla hlasena (nemelo by sem vubec prijit)
                // case stctType:  // chyba uz byla hlasena (nemelo by sem vubec prijit)
                case stctName:
                case stctGeneralText:
                    return potString;

                case stctSize:
                case stctGeneralNumber:
                    return potNumber;

                case stctDate:
                case stctGeneralDate:
                    return potDate;

                case stctTime:
                case stctGeneralTime:
                    return potTime;
                }
            }
            else
            {
                if (ColumnIndex == COL_IND_ISDIR || ColumnIndex == COL_IND_ISHIDDEN ||
                    ColumnIndex == COL_IND_ISLINK)
                {
                    return potBoolean;
                }
            }
            TRACE_E("Unexpected situation 2 in CFTPParserParameter::GetOperandType()!");
            return potNone;
        }
        else
        {
            if (Type == pptStateVar)
            {
                switch (StateVar)
                {
                case psvFirstNonEmptyLine:
                case psvLastNonEmptyLine:
                    return potBoolean;

                case psvNextChar:
                case psvNextWord:
                case psvRestOfLine:
                    return potString;
                }
                TRACE_E("Unexpected situation 3 in CFTPParserParameter::GetOperandType()!");
                return potNone;
            }
            else
            {
                switch (Type)
                {
                case pptBoolean:
                    return potBoolean;
                case pptString:
                    return potString;
                case pptNumber:
                    return potNumber;

                default:
                {
                    TRACE_E("Unexpected situation 4 in CFTPParserParameter::GetOperandType()!");
                    return potNone;
                }
                }
            }
        }
    }
}

CFTPParserFuncParType
CFTPParserParameter::GetFuncParType(TIndirectArray<CSrvTypeColumn>* columns)
{
    if (Type == pptExpression)
    {
        return pfptBoolean; // vsechny vyrazy zatim maji vysledny typ boolean

        // POZOR: pokud se pridaji vyrazy dalsich typu, je treba prislusne upravit
        // CFTPParserParameter::GetXXX()
    }
    else
    {
        if (Type == pptColumnID) // u sloupce zjistime typ podle typu hodnot ve sloupci
        {
            if (ColumnIndex >= 0 && ColumnIndex < columns->Count)
            {
                CSrvTypeColumnTypes t = columns->At(ColumnIndex)->Type;
                switch (t)
                {
                // case stctExt:   // chyba uz byla hlasena (nemelo by sem vubec prijit)
                // case stctType:  // chyba uz byla hlasena (nemelo by sem vubec prijit)
                case stctName:
                case stctGeneralText:
                    return pfptColumnString;

                case stctSize:
                case stctGeneralNumber:
                    return pfptColumnNumber;

                case stctDate:
                case stctGeneralDate:
                    return pfptColumnDate;

                case stctTime:
                case stctGeneralTime:
                    return pfptColumnTime;
                }
            }
            else
            {
                if (ColumnIndex == COL_IND_ISDIR || ColumnIndex == COL_IND_ISHIDDEN ||
                    ColumnIndex == COL_IND_ISLINK)
                {
                    return pfptColumnBoolean;
                }
            }
            TRACE_E("Unexpected situation in CFTPParserParameter::GetFuncParType()!");
            return pfptNone;
        }
        else
        {
            if (Type == pptStateVar)
            {
                switch (StateVar)
                {
                case psvFirstNonEmptyLine:
                case psvLastNonEmptyLine:
                    return pfptBoolean;

                case psvNextChar:
                case psvNextWord:
                case psvRestOfLine:
                    return pfptString;

                    // POZOR: pokud se pridaji stavove promene dalsich typu, je treba prislusne upravit
                    // CFTPParserParameter::GetXXX()
                }
                TRACE_E("Unexpected situation 2 in CFTPParserParameter::GetFuncParType()!");
                return pfptNone;
            }
            else
            {
                switch (Type)
                {
                case pptBoolean:
                    return pfptBoolean;
                case pptString:
                    return pfptString;
                case pptNumber:
                    return pfptNumber;
                }
                TRACE_E("Unexpected situation 3 in CFTPParserParameter::GetFuncParType()!");
                return pfptNone;
            }
        }
    }
}

char* GetStringStateVariable(CFTPParserStateVariables stateVar, const char* listing,
                             const char* listingEnd, BOOL* needDealloc, BOOL* lowMemErr)
{
    switch (stateVar)
    {
    case psvNextChar:
    case psvNextWord:
    case psvRestOfLine:
    {
        const char* s = listing;
        if (stateVar == psvNextChar)
        {
            if (s < listingEnd)
                s++;
        }
        else
        {
            if (stateVar == psvNextWord)
            {
                while (s<listingEnd&& * s> ' ')
                    s++;
            }
            else // psvRestOfLine
            {
                while (s < listingEnd && *s != '\r' && *s != '\n')
                    s++;
            }
        }
        char* str = (char*)malloc((s - listing) + 1);
        if (str == NULL)
        {
            TRACE_E(LOW_MEMORY);
            *lowMemErr = TRUE;
            break;
        }
        memcpy(str, listing, s - listing);
        str[s - listing] = 0;
        *needDealloc = TRUE;
        return str;
    }

    default:
        TRACE_E("GetStringStateVariable(): unknown state variable!");
        break;
    }
    *needDealloc = FALSE;
    return NULL;
}

const char*
CFTPParserParameter::GetString(const char* listing, const char* listingEnd, BOOL* needDealloc,
                               BOOL* lowMemErr)
{
#ifdef _DEBUG
    if (Type != pptString &&
        (Type != pptStateVar ||
         StateVar != psvNextWord && StateVar != psvNextChar && StateVar != psvRestOfLine))
    {
        TRACE_E("Unexpected situation in CFTPParserParameter::GetString(): not a string!");
        return NULL;
    }
#endif
    if (Type == pptString)
    {
        *needDealloc = FALSE;
        return String;
    }
    else
    {
        if (Type == pptStateVar)
            return GetStringStateVariable(StateVar, listing, listingEnd, needDealloc, lowMemErr);
        *needDealloc = FALSE;
        return NULL;
    }
}

BOOL CFTPParserParameter::GetBoolean(CFileData* file, BOOL* isDir,
                                     CFTPListingPluginDataInterface* dataIface,
                                     TIndirectArray<CSrvTypeColumn>* columns, const char* listing,
                                     const char* listingEnd, CFTPParser* actualParser)
{
#ifdef _DEBUG
    if (Type != pptBoolean && Type != pptExpression &&
        (Type != pptColumnID || ColumnIndex != COL_IND_ISDIR && ColumnIndex != COL_IND_ISHIDDEN &&
                                    ColumnIndex != COL_IND_ISLINK) &&
        (Type != pptStateVar || StateVar != psvFirstNonEmptyLine && StateVar != psvLastNonEmptyLine))
    {
        TRACE_E("Unexpected situation in CFTPParserParameter::GetBoolean(): not a boolean!");
        return FALSE;
    }
#endif
    if (Type == pptBoolean)
        return Boolean;
    else
    {
        if (Type == pptColumnID)
        {
            switch (ColumnIndex)
            {
            case COL_IND_ISDIR:
                return *isDir;
            case COL_IND_ISHIDDEN:
                return file->Hidden != 0;
            case COL_IND_ISLINK:
                return file->IsLink != 0;
            }
        }
        else
        {
            if (Type == pptStateVar)
            {
                switch (ColumnIndex)
                {
                case psvFirstNonEmptyLine:
                    return actualParser->FirstNonEmptyBeg <= listing && listing < actualParser->FirstNonEmptyEnd;

                case psvLastNonEmptyLine:
                    return actualParser->LastNonEmptyBeg <= listing && listing < actualParser->LastNonEmptyEnd;
                }
            }
            else
            {
                if (Type == pptExpression)
                {
                    CFTPParserParameter* left = Parameters[0];
                    CFTPParserParameter* right = Parameters[1];
                    if (left != NULL && right != NULL)
                    {
                        CFTPParserOperandType leftType = left->GetOperandType(columns);
                        CFTPParserOperandType rightType = right->GetOperandType(columns);
                        if (leftType != potNone && leftType == rightType) // zatim mame jen operace
                        {
                            switch (leftType)
                            {
                            case potBoolean:
                            {
                                BOOL l = left->GetBoolean(file, isDir, dataIface, columns, listing,
                                                          listingEnd, actualParser);
                                BOOL r = right->GetBoolean(file, isDir, dataIface, columns, listing,
                                                           listingEnd, actualParser);
                                switch (BinOperator)
                                {
                                case pboEqual:
                                    return l == r;
                                case pboNotEqual:
                                    return l != r;
                                default:
                                    TRACE_E("Unexpected boolean operator in CFTPParserParameter::GetBoolean()");
                                    return FALSE;
                                }
                            }

                            case potString:
                            {
                                const char* lBeg;
                                const char* lEnd;
                                left->GetStringOperand(&lBeg, &lEnd, file, dataIface, columns, listing, listingEnd);
                                const char* rBeg;
                                const char* rEnd;
                                right->GetStringOperand(&rBeg, &rEnd, file, dataIface, columns, listing, listingEnd);
                                switch (BinOperator)
                                {
                                case pboEqual:
                                    return lEnd - lBeg == rEnd - rBeg && strncmp(lBeg, rBeg, lEnd - lBeg) == 0;
                                case pboNotEqual:
                                    return lEnd - lBeg != rEnd - rBeg || strncmp(lBeg, rBeg, lEnd - lBeg) != 0;

                                case pboStrEqual:
                                    return lEnd - lBeg == rEnd - rBeg &&
                                           SalamanderGeneral->StrNICmp(lBeg, rBeg, (int)(lEnd - lBeg)) == 0;
                                case pboStrNotEqual:
                                    return lEnd - lBeg != rEnd - rBeg ||
                                           SalamanderGeneral->StrNICmp(lBeg, rBeg, (int)(lEnd - lBeg)) != 0;

                                case pboSubStrIsInString:
                                case pboSubStrIsNotInString:
                                {
                                    BOOL found = FALSE;
                                    if (lBeg < lEnd) // neprazdny vzorek pro hledani
                                    {
                                        const char* s = rBeg;
                                        while (s < rEnd)
                                        {
                                            if (LowerCase[*lBeg] == LowerCase[*s]) // shoduje se prvni pismeno hledaneho vzorku
                                            {                                      // hledame 'lBeg' v 's' (tim nejhloupejsim algoritmem - O(m*n), ovsem v realnych pripadech temer O(1))
                                                const char* m = lBeg + 1;
                                                const char* t = s + 1;
                                                while (m < lEnd && t < rEnd && LowerCase[*m] == LowerCase[*t])
                                                {
                                                    m++;
                                                    t++;
                                                }
                                                if (m == lEnd) // nalezeno
                                                {
                                                    found = TRUE;
                                                    break;
                                                }
                                            }
                                            s++;
                                        }
                                    }
                                    else
                                        found = TRUE;
                                    if (BinOperator == pboSubStrIsInString)
                                        return found;
                                    else
                                        return !found;
                                }

                                case pboStrEndWithString:
                                    return lEnd - lBeg >= rEnd - rBeg &&
                                           SalamanderGeneral->StrNICmp(lEnd - (rEnd - rBeg), rBeg, (int)(rEnd - rBeg)) == 0;
                                case pboStrNotEndWithString:
                                    return lEnd - lBeg < rEnd - rBeg ||
                                           SalamanderGeneral->StrNICmp(lEnd - (rEnd - rBeg), rBeg, (int)(rEnd - rBeg)) != 0;

                                default:
                                    TRACE_E("Unexpected string operator in CFTPParserParameter::GetBoolean()");
                                    return FALSE;
                                }
                            }

                            case potNumber:
                            {
                                BOOL lMinus;
                                __int64 l = left->GetNumberOperand(file, dataIface, columns, &lMinus);
                                BOOL rMinus;
                                __int64 r = right->GetNumberOperand(file, dataIface, columns, &rMinus);
                                switch (BinOperator)
                                {
                                case pboEqual:
                                    return lMinus == rMinus && l == r;
                                case pboNotEqual:
                                    return lMinus != rMinus || l != r;
                                default:
                                    TRACE_E("Unexpected number operator in CFTPParserParameter::GetBoolean()");
                                    return FALSE;
                                }
                            }

                            case potDate:
                            {
                                CFTPDate l;
                                left->GetDateOperand(&l, file, dataIface, columns);
                                CFTPDate r;
                                right->GetDateOperand(&r, file, dataIface, columns);
                                switch (BinOperator)
                                {
                                case pboEqual:
                                    return l.Day == r.Day && l.Month == r.Month && l.Year == r.Year;
                                case pboNotEqual:
                                    return l.Day != r.Day || l.Month != r.Month || l.Year != r.Year;
                                default:
                                    TRACE_E("Unexpected date operator in CFTPParserParameter::GetBoolean()");
                                    return FALSE;
                                }
                            }

                            case potTime:
                            {
                                CFTPTime l;
                                left->GetTimeOperand(&l, file, dataIface, columns);
                                CFTPTime r;
                                right->GetTimeOperand(&r, file, dataIface, columns);
                                switch (BinOperator)
                                {
                                case pboEqual:
                                    return l.Hour == r.Hour && l.Minute == r.Minute &&
                                           l.Second == r.Second && l.Millisecond == r.Millisecond;
                                case pboNotEqual:
                                    return l.Hour != r.Hour || l.Minute != r.Minute ||
                                           l.Second != r.Second || l.Millisecond != r.Millisecond;
                                default:
                                    TRACE_E("Unexpected time operator in CFTPParserParameter::GetBoolean()");
                                    return FALSE;
                                }
                            }

                            default:
                            {
                                TRACE_E("Unexpected situation 4 in CFTPParserParameter::GetBoolean(): unknown type of operands!");
                                return FALSE;
                            }
                            }
                        }
                        else
                            TRACE_E("Unexpected situation 3 in CFTPParserParameter::GetBoolean()");
                    }
                    else
                        TRACE_E("Unexpected situation 2 in CFTPParserParameter::GetBoolean()");
                }
            }
        }
    }
    return FALSE;
}

__int64
CFTPParserParameter::GetNumberOperand(CFileData* file, CFTPListingPluginDataInterface* dataIface,
                                      TIndirectArray<CSrvTypeColumn>* columns, BOOL* minus)
{
#ifdef _DEBUG
    if (Type != pptNumber &&
        (Type != pptColumnID || ColumnIndex < 0 || ColumnIndex >= columns->Count ||
         columns->At(ColumnIndex)->Type != stctSize &&
             columns->At(ColumnIndex)->Type != stctGeneralNumber))
    {
        TRACE_E("Unexpected situation in CFTPParserParameter::GetNumberOperand(): not a number!");
        return 0;
    }
#endif
    if (Type == pptNumber)
    {
        *minus = Number < 0;
        return Number;
    }
    else
    {
        if (Type == pptColumnID)
        {
            if (columns->At(ColumnIndex)->Type == stctSize)
            {
                *minus = FALSE;
                return (__int64)(file->Size.Value);
            }
            else
            {
                __int64 num = dataIface->GetNumberFromColumn(*file, ColumnIndex);
                // if (num == INT64_EMPTYNUMBER) num = 0;  // hodnota "prazdne hodnoty" je nula // nemuze nastat, vylitne chyba kompilace parseru: pouziti hodnoty pred jeji inicializaci
                *minus = num < 0;
                return num;
            }
        }
        *minus = FALSE;
        return 0;
    }
}

void CFTPParserParameter::GetDateOperand(CFTPDate* date, CFileData* file,
                                         CFTPListingPluginDataInterface* dataIface,
                                         TIndirectArray<CSrvTypeColumn>* columns)
{
#ifdef _DEBUG
    if (Type != pptColumnID || ColumnIndex < 0 || ColumnIndex >= columns->Count ||
        columns->At(ColumnIndex)->Type != stctDate &&
            columns->At(ColumnIndex)->Type != stctGeneralDate)
    {
        TRACE_E("Unexpected situation in CFTPParserParameter::GetDateOperand(): not a date!");
        memset(date, 0, sizeof(CFTPDate));
        return;
    }
#endif
    if (columns->At(ColumnIndex)->Type == stctDate)
        *date = *((CFTPDate*)&(file->LastWrite.dwLowDateTime));
    else
        dataIface->GetDateFromColumn(*file, ColumnIndex, date);
}

void CFTPParserParameter::GetTimeOperand(CFTPTime* time, CFileData* file,
                                         CFTPListingPluginDataInterface* dataIface,
                                         TIndirectArray<CSrvTypeColumn>* columns)
{
#ifdef _DEBUG
    if (Type != pptColumnID || ColumnIndex < 0 || ColumnIndex >= columns->Count ||
        columns->At(ColumnIndex)->Type != stctTime &&
            columns->At(ColumnIndex)->Type != stctGeneralTime)
    {
        TRACE_E("Unexpected situation in CFTPParserParameter::GetTimeOperand(): not a time!");
        memset(time, 0, sizeof(CFTPTime));
        return;
    }
#endif
    if (columns->At(ColumnIndex)->Type == stctTime)
        *time = *((CFTPTime*)&(file->LastWrite.dwHighDateTime));
    else
        dataIface->GetTimeFromColumn(*file, ColumnIndex, time);
}

void CFTPParserParameter::GetStringOperand(const char** beg, const char** end, CFileData* file,
                                           CFTPListingPluginDataInterface* dataIface,
                                           TIndirectArray<CSrvTypeColumn>* columns,
                                           const char* listing, const char* listingEnd)
{
#ifdef _DEBUG
    if (Type != pptString &&
        (Type != pptStateVar || StateVar != psvNextWord && StateVar != psvNextChar && StateVar != psvRestOfLine) &&
        (Type != pptColumnID || ColumnIndex < 0 || ColumnIndex >= columns->Count ||
         columns->At(ColumnIndex)->Type != stctName &&
             columns->At(ColumnIndex)->Type != stctGeneralText))
    {
        TRACE_E("Unexpected situation in CFTPParserParameter::GetStringOperand(): not a string!");
        *beg = NULL;
        *end = NULL;
        return;
    }
#endif
    if (Type == pptString)
    {
        *beg = String;
        *end = String + (String == NULL ? 0 : strlen(String));
    }
    else
    {
        if (Type == pptStateVar)
        {
            const char* s = listing;
            if (StateVar == psvNextChar)
            {
                if (s < listingEnd)
                    s++;
            }
            else
            {
                if (StateVar == psvNextWord)
                {
                    while (s<listingEnd&& * s> ' ')
                        s++;
                }
                else // psvRestOfLine
                {
                    while (s < listingEnd && *s != '\r' && *s != '\n')
                        s++;
                }
            }
            *beg = listing;
            *end = s;
        }
        else
        {
            if (columns->At(ColumnIndex)->Type == stctName)
            {
                *beg = file->Name;
                *end = file->Name + (file->Name == NULL ? 0 : strlen(file->Name));
            }
            else
            {
                *beg = dataIface->GetStringFromColumn(*file, ColumnIndex);
                *end = *beg + (*beg == NULL ? 0 : strlen(*beg));
            }
        }
    }
}
