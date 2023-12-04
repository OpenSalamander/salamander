// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CFTPAutodetCondLexAn
//

// lexikalni symboly pro preklad autodetekcni podminky
enum CFTPAutodetCondLexElement
{
    lexNone,             // neinicializovano
    lexEOS,              // byl dosazen konec retezce
    lexUnknown,          // neznamy symbol
    lexLogOr,            // OR
    lexLogAnd,           // AND
    lexLogNegation,      // NOT
    lexLeftParenthesis,  // (
    lexRightParenthesis, // )
    lexFunction,         // funkce vcetne jednoho parametru v zavorkach
};

class CFTPAutodetCondLexAn
{
protected:
    const char* CondBeg;    // zacatek textu autodetekcni podminky
    const char* CondEnd;    // konec textu autodetekcni podminky
    const char* Cond;       // pozice zacatku aktualniho symbolu v textu autodetekcni podminky
    const char* CondSymEnd; // pozice konce aktualniho symbolu v textu autodetekcni podminky (NULL = neznamy)

    CFTPAutodetCondLexElement ActElem; // aktualni symbol (lexNone = je teprve nutne symbol najit)

    int* ErrorResID; // neni-li NULL, uklada se sem id retezce v resourcech, ktery popisuje chybu
    BOOL* LowMem;    // neni-li NULL, uklada se sem TRUE pri chybe zpusobene nedostatkem pameti
    char* ErrBuf;    // buffer o velikosti ErrBufSize pro textovy popis chyby (prioritnejsi nez ErrorResID)
    int ErrBufSize;  // velikost bufferu ErrBuf (0 = NULLovy buffer)

    CFTPAutodetCondFunction ActFunction; // data funkce (jen je-li aktualni symbol lexFunction)
    void* ActFuncAlgorithm;              // bud (CSalamanderBMSearchData *) a nebo (CSalamanderREGEXPSearchData *)

public:
    CFTPAutodetCondLexAn(const char* cond, const char* condEnd, int* errorResID, BOOL* lowMem,
                         char* errBuf, int errBufSize);
    ~CFTPAutodetCondLexAn() { ReleaseActFuncAlgorithm(); }

    // uvolni a vyNULLuje ActFuncAlgorithm
    void ReleaseActFuncAlgorithm();

    // vraci kod aktualniho lexikalniho elementu
    CFTPAutodetCondLexElement GetActualElement();

    // preskoci jeden lexikalni element
    void Match();

    // vraci data pro funkci; aktualni symbol musi byt lexFunction a muze se
    // volat jen jednou pro kazdy symbol funkce
    void GiveFunctionData(CFTPAutodetCondFunction& function, void*& algorithm);

    // vraci pozici aktualniho symbolu v textu
    int GetActualSymPos() { return (int)(Cond - CondBeg); }

    // pomocne chybove parametry (nastavuji "globalni" promenne)
    void SetErrorResID(int errorResID)
    {
        if (ErrorResID != NULL && *ErrorResID == -1 &&
            (ErrBufSize <= 0 || ErrBuf[0] == 0))
            *ErrorResID = errorResID;
    }

    void SetLowMem()
    {
        if (LowMem != NULL)
            *LowMem = TRUE;
    }
};

CFTPAutodetCondLexAn::CFTPAutodetCondLexAn(const char* cond, const char* condEnd,
                                           int* errorResID, BOOL* lowMem, char* errBuf,
                                           int errBufSize)
{
    CondBeg = cond;
    CondEnd = condEnd;
    Cond = cond;
    CondSymEnd = NULL;
    ActElem = lexNone;
    ErrorResID = errorResID;
    LowMem = lowMem;
    ActFunction = acfNone;
    ActFuncAlgorithm = NULL;
    ErrBuf = errBuf;
    ErrBufSize = errBufSize;
}

void CFTPAutodetCondLexAn::ReleaseActFuncAlgorithm()
{
    if (ActFuncAlgorithm != NULL)
    {
        switch (ActFunction)
        {
        case acfSyst_contains:
        case acfWelcome_contains:
        {
            SalamanderGeneral->FreeSalamanderBMSearchData((CSalamanderBMSearchData*)ActFuncAlgorithm);
            ActFuncAlgorithm = NULL;
            break;
        }

        case acfReg_exp_in_syst:
        case acfReg_exp_in_welcome:
        {
            SalamanderGeneral->FreeSalamanderREGEXPSearchData((CSalamanderREGEXPSearchData*)ActFuncAlgorithm);
            ActFuncAlgorithm = NULL;
            break;
        }

        default:
            TRACE_E("Unknown function in CFTPAutodetCondLexAn::ReleaseActFuncAlgorithm()!");
        }
    }
}

CFTPAutodetCondLexElement
CFTPAutodetCondLexAn::GetActualElement()
{
    if (ActElem != lexNone)
        return ActElem;

    const char* s = Cond;
    const char* end = CondEnd;
    while (s < end)
    {
        if (*s > ' ')
        {
            if (*s == '(' || *s == ')')
            {
                Cond = s;
                CondSymEnd = s + 1;
                return ActElem = (*s == ')' ? lexRightParenthesis : lexLeftParenthesis);
            }
            else
            {
                const char* beg = s;
                if (SkipIdentifier(s, end, NULL, 0))
                {
                    char id[100];
                    int idLen = (int)(s - beg);
                    lstrcpyn(id, beg, min(100, idLen + 1));

                    ActElem = lexUnknown;
                    if (idLen == 2 && _stricmp(id, "or") == 0)
                        ActElem = lexLogOr;
                    else
                    {
                        if (idLen == 3)
                        {
                            if (_stricmp(id, "and") == 0)
                                ActElem = lexLogAnd;
                            else
                            {
                                if (_stricmp(id, "not") == 0)
                                    ActElem = lexLogNegation;
                            }
                        }
                    }
                    if (ActElem != lexUnknown)
                    {
                        Cond = beg;
                        CondSymEnd = s;
                        return ActElem;
                    }

                    static const char* functionNames[] = {
                        "syst_contains",
                        "welcome_contains",
                        "reg_exp_in_syst",
                        "reg_exp_in_welcome",
                    };
                    // celkovy pocet funkci (AKTUALIZOVAT !!! + rovnat s CFTPAutodetCondFunction + doplnit i functionCodes)
                    static const int count = 4;
                    static CFTPAutodetCondFunction functionCodes[] = {
                        acfSyst_contains,
                        acfWelcome_contains,
                        acfReg_exp_in_syst,
                        acfReg_exp_in_welcome,
                    };
                    CFTPAutodetCondFunction funcType = acfNone;
                    int i;
                    for (i = 0; i < count; i++)
                    {
                        if (_stricmp(functionNames[i], id) == 0)
                        {
                            funcType = functionCodes[i];
                            break;
                        }
                    }

                    if (funcType != acfNone)
                    {
                        // jdeme hledat parametr funkce
                        while (s < end && *s <= ' ')
                            s++;
                        if (s < end && *s == '(')
                        {
                            s++;
                            while (s < end && *s <= ' ')
                                s++;
                            if (s < end && *s == '"')
                            {
                                s++;
                                const char* strBeg = s;
                                int esc = 0; // pocet escape sekvenci
                                BOOL strEndFound = FALSE;
                                while (s < end)
                                {
                                    if (*s == '"') // konec retezce
                                    {
                                        strEndFound = TRUE;
                                        break;
                                    }
                                    if (*s == '\r' || *s == '\n')
                                        break;                     // retezec nemuze obsahovat primo EOL (pouzite escape sekvence '\r' a '\n')
                                    if (*s == '\\' && s + 1 < end) // escape sekvence
                                    {
                                        s++;
                                        esc++;
                                        if (*s != '"' && *s != '\\' && *s != 't' &&
                                            *s != 'r' && *s != 'n')
                                        {
                                            Cond = s;
                                            SetErrorResID(IDS_STPAR_ERR_STRUNKNESCSEQ);
                                            break;
                                        }
                                    }
                                    s++;
                                }
                                if (strEndFound) // string je OK
                                {
                                    const char* orgStrBeg = strBeg;
                                    char* str = (char*)malloc((s - strBeg) - esc + 1);
                                    if (str != NULL)
                                    {
                                        char* t = str;
                                        while (strBeg < s)
                                        {
                                            if (*strBeg != '\\')
                                                *t++ = *strBeg++;
                                            else
                                            {
                                                if (strBeg < s)
                                                    strBeg++; // "always true"
                                                switch (*strBeg)
                                                {
                                                case '"':
                                                    *t++ = '"';
                                                    break;
                                                case '\\':
                                                    *t++ = '\\';
                                                    break;
                                                case 't':
                                                    *t++ = '\t';
                                                    break;
                                                case 'r':
                                                    *t++ = '\r';
                                                    break;
                                                case 'n':
                                                    *t++ = '\n';
                                                    break;
                                                default:
                                                    TRACE_E("Escape sequence '\\" << *strBeg << "' is not implemented!");
                                                }
                                                strBeg++;
                                            }
                                        }
                                        *t = 0;
                                        s++; // preskocime koncovou '"' retezce

                                        while (s < end && *s <= ' ')
                                            s++;
                                        if (s < end && *s == ')')
                                        {
                                            Cond = beg;
                                            CondSymEnd = s + 1;
                                            if (str[0] == 0) // prazdny vzorek = always true
                                            {
                                                free(str);
                                                ActFunction = acfAlwaysTrue;
                                                return ActElem = lexFunction;
                                            }
                                            ActFunction = funcType;
                                            BOOL setLowMemErr = TRUE;
                                            switch (ActFunction)
                                            {
                                            case acfSyst_contains:
                                            case acfWelcome_contains:
                                            {
                                                ActFuncAlgorithm = SalamanderGeneral->AllocSalamanderBMSearchData();
                                                if (ActFuncAlgorithm != NULL)
                                                {
                                                    ((CSalamanderBMSearchData*)ActFuncAlgorithm)->Set(str, SASF_FORWARD); // forward + case-insensitive
                                                    if (!((CSalamanderBMSearchData*)ActFuncAlgorithm)->IsGood())
                                                    {
                                                        SalamanderGeneral->FreeSalamanderBMSearchData((CSalamanderBMSearchData*)ActFuncAlgorithm);
                                                        ActFuncAlgorithm = NULL; // ohlasime low memory
                                                    }
                                                }
                                                break;
                                            }

                                            case acfReg_exp_in_syst:
                                            case acfReg_exp_in_welcome:
                                            {
                                                ActFuncAlgorithm = SalamanderGeneral->AllocSalamanderREGEXPSearchData();
                                                if (ActFuncAlgorithm != NULL)
                                                {
                                                    if (!((CSalamanderREGEXPSearchData*)ActFuncAlgorithm)->Set(str, SASF_FORWARD)) // forward + case-insensitive
                                                    {
                                                        Cond = orgStrBeg;
                                                        const char* err = ((CSalamanderREGEXPSearchData*)ActFuncAlgorithm)->GetLastErrorText();
                                                        if (ErrBufSize > 0)
                                                        {
                                                            if (err != NULL)
                                                                _snprintf_s(ErrBuf, ErrBufSize, _TRUNCATE, LoadStr(IDS_STPAR_ERR_INVALREGEXP), err);
                                                            else
                                                                _snprintf_s(ErrBuf, ErrBufSize, _TRUNCATE, LoadStr(IDS_STPAR_ERR_INVALREGEXP2));
                                                        }
                                                        SalamanderGeneral->FreeSalamanderREGEXPSearchData((CSalamanderREGEXPSearchData*)ActFuncAlgorithm);
                                                        ActFuncAlgorithm = NULL;
                                                        setLowMemErr = FALSE; // chyba uz je nastavena
                                                    }
                                                }
                                                break;
                                            }

                                            default:
                                            {
                                                TRACE_E("Unknown function in CFTPAutodetCondLexAn::GetActualElement()!");
                                                ActFuncAlgorithm = NULL; // koncime s "low memory"
                                            }
                                            }
                                            free(str);
                                            if (ActFuncAlgorithm != NULL)
                                                return ActElem = lexFunction;
                                            else
                                            {
                                                if (setLowMemErr)
                                                    SetLowMem();
                                            }
                                        }
                                        else
                                        {
                                            Cond = s;
                                            free(str);
                                            SetErrorResID(IDS_STPAR_ERR_MISSINGPAREND);
                                        }
                                    }
                                    else
                                        SetLowMem();
                                }
                                else
                                {
                                    Cond = s;
                                    SetErrorResID(IDS_STPAR_ERR_MISSINGSTREND);
                                }
                            }
                            else
                            {
                                Cond = s;
                                SetErrorResID(IDS_STPAR_ERR_MISSINGSTRPAR);
                            }
                        }
                        else
                        {
                            Cond = s;
                            SetErrorResID(IDS_STPAR_ERR_MISSINGFUNCPARS);
                        }
                    }
                    else
                    {
                        Cond = beg;
                        SetErrorResID(IDS_STPAR_ERR_UNKNOWNFUNC);
                    }
                }
                else
                {
                    Cond = s;
                    SetErrorResID(IDS_STPAR_ERR_UNEXPSYM);
                }
                return ActElem = lexUnknown; // unexpected symbol
            }
        }
        s++;
    }
    return ActElem = lexEOS;
}

void CFTPAutodetCondLexAn::Match()
{
    if (CondSymEnd != NULL)
    {
        Cond = CondSymEnd;
        CondSymEnd = NULL;
        ActElem = lexNone;
        if (ActFunction != acfNone)
        {
            TRACE_E("Unexpected situation in CFTPAutodetCondLexAn::Match() - function data were not used!");
            if (ActFuncAlgorithm != NULL)
                ReleaseActFuncAlgorithm();
            ActFunction = acfNone;
        }
    }
    else
        TRACE_E("Incorrect use of CFTPAutodetCondLexAn::Match() - CondSymEnd is not set!");
}

void CFTPAutodetCondLexAn::GiveFunctionData(CFTPAutodetCondFunction& function, void*& algorithm)
{
    if (ActElem == lexFunction && ActFunction != acfNone)
    {
        function = ActFunction; // data funkce (jen je-li aktualni symbol lexFunction)
        algorithm = ActFuncAlgorithm;
        ActFunction = acfNone; // odted jiz data nejsou nase
        ActFuncAlgorithm = NULL;
    }
    else
    {
        TRACE_E("Incorrect use of CFTPAutodetCondLexAn::GiveFunctionData()!");
        function = acfNone;
        algorithm = NULL;
    }
}

CFTPAutodetCondNode* FTP_AC_ExpOr(CFTPAutodetCondLexAn& lexAn);
CFTPAutodetCondNode* FTP_AC_ExpOrRest(CFTPAutodetCondNode* left, CFTPAutodetCondLexAn& lexAn);
CFTPAutodetCondNode* FTP_AC_ExpAnd(CFTPAutodetCondLexAn& lexAn);
CFTPAutodetCondNode* FTP_AC_ExpAndRest(CFTPAutodetCondNode* left, CFTPAutodetCondLexAn& lexAn);
CFTPAutodetCondNode* FTP_AC_ExpNot(CFTPAutodetCondLexAn& lexAn);
CFTPAutodetCondNode* FTP_AC_Term(CFTPAutodetCondLexAn& lexAn);

//
// ****************************************************************************
// CompileAutodetectCond
//

CFTPAutodetCondNode* CompileAutodetectCond(const char* cond, int* errorPos, int* errorResID,
                                           BOOL* lowMem, char* errBuf, int errBufSize)
{
    CALL_STACK_MESSAGE1("CompileAutodetectCond()");
    if (errorPos != NULL)
        *errorPos = -1;
    if (errorResID != NULL)
        *errorResID = -1;
    if (lowMem != NULL)
        *lowMem = FALSE;
    if (errBufSize > 0)
        errBuf[0] = 0;

    CFTPAutodetCondLexAn lexAn(cond, cond + strlen(cond), errorResID, lowMem, errBuf, errBufSize);
    if (lexAn.GetActualElement() != lexEOS) // neprazdna podminka -> jdeme prekladat
    {
        CFTPAutodetCondNode* node = FTP_AC_ExpOr(lexAn);
        if (node != NULL && lexAn.GetActualElement() != lexEOS) // pokud neni prelozeny cely string, jde o chybu (pravidlo "empty" z gramatiky je "vybalene" na tomto miste)
        {
            lexAn.SetErrorResID(IDS_STPAR_ERR_UNEXPSYM);
            delete node;
            node = NULL;
        }
        if (node == NULL && errorPos != NULL)
            *errorPos = lexAn.GetActualSymPos();
        return node;
    }
    else // prazdna podminka -> always true
    {
        CFTPAutodetCondNode* node = new CFTPAutodetCondNode;
        if (node != NULL)
        {
            node->Type = acntAlwaysTrue;
            return node;
        }
        else
        {
            TRACE_E(LOW_MEMORY);
            if (lowMem != NULL)
                *lowMem = TRUE;
            return NULL;
        }
    }
}

//
// ****************************************************************************
// funkce pro kompilaci autodetekcni podminky
//

// alokuje novy uzel pro operator; 'type' muze byt jen 'acntOr' nebo 'acntAnd';
// 'left'+'right' jsou operandy; vraci alokovany uzel; pri nedostatku pameti
// dealokuje 'left' a 'right' a vraci NULL
CFTPAutodetCondNode* CreateNewACOperNode(CFTPAutodetCondLexAn& lexAn, CFTPAutodetCondNodeType type,
                                         CFTPAutodetCondNode* left, CFTPAutodetCondNode* right)
{
    CFTPAutodetCondNode* node = new CFTPAutodetCondNode;
    if (node == NULL)
    {
        TRACE_E(LOW_MEMORY);
        lexAn.SetLowMem();
        if (left != NULL)
            delete left;
        if (right != NULL)
            delete right;
        return NULL;
    }
    node->Type = type;
    node->Left = left;
    node->Right = right;
    return node;
}

// alokuje novy uzel pro negaci; 'operand' je operand negace; vraci alokovany
// uzel; pri nedostatku pameti dealokuje 'operand' a vraci NULL
CFTPAutodetCondNode* CreateNewACNotNode(CFTPAutodetCondLexAn& lexAn, CFTPAutodetCondNode* operand)
{
    CFTPAutodetCondNode* node = new CFTPAutodetCondNode;
    if (node == NULL)
    {
        TRACE_E(LOW_MEMORY);
        lexAn.SetLowMem();
        if (operand != NULL)
            delete operand;
        return NULL;
    }
    node->Type = acntNot;
    node->NegNode = operand;
    return node;
}

// alokuje novy uzel pro funkci nactenou z 'lexAn'; vraci alokovany uzel nebo
// NULL pri nedostatku pameti
CFTPAutodetCondNode* CreateNewACFuncNode(CFTPAutodetCondLexAn& lexAn)
{
    CFTPAutodetCondNode* node = new CFTPAutodetCondNode;
    if (node == NULL)
    {
        TRACE_E(LOW_MEMORY);
        lexAn.SetLowMem();
        return NULL;
    }
    node->Type = acntFunc;
    lexAn.GiveFunctionData(node->Function, node->Algorithm);
    return node;
}

CFTPAutodetCondNode* FTP_AC_ExpOr(CFTPAutodetCondLexAn& lexAn)
{
    CFTPAutodetCondNode* left = FTP_AC_ExpAnd(lexAn);
    if (left == NULL)
        return NULL; // chyba, koncime
    return FTP_AC_ExpOrRest(left, lexAn);
}

CFTPAutodetCondNode* FTP_AC_ExpOrRest(CFTPAutodetCondNode* left, CFTPAutodetCondLexAn& lexAn)
{
    switch (lexAn.GetActualElement())
    {
    case lexLogOr:
    {
        lexAn.Match();
        CFTPAutodetCondNode* right = FTP_AC_ExpOr(lexAn);
        if (right == NULL)
        {
            delete left;
            return NULL; // chyba, koncime
        }
        return CreateNewACOperNode(lexAn, acntOr, left, right);
    }

    case lexEOS:
    case lexRightParenthesis:
        return left;

    default:
    {
        lexAn.SetErrorResID(IDS_STPAR_ERR_UNEXPSYM);
        delete left;
        return NULL; // unexpected symbol
    }
    }
}

CFTPAutodetCondNode* FTP_AC_ExpAnd(CFTPAutodetCondLexAn& lexAn)
{
    CFTPAutodetCondNode* left = FTP_AC_ExpNot(lexAn);
    if (left == NULL)
        return NULL; // chyba, koncime
    return FTP_AC_ExpAndRest(left, lexAn);
}

CFTPAutodetCondNode* FTP_AC_ExpAndRest(CFTPAutodetCondNode* left, CFTPAutodetCondLexAn& lexAn)
{
    switch (lexAn.GetActualElement())
    {
    case lexLogAnd:
    {
        lexAn.Match();
        CFTPAutodetCondNode* right = FTP_AC_ExpAnd(lexAn);
        if (right == NULL)
        {
            delete left;
            return NULL; // chyba, koncime
        }
        return CreateNewACOperNode(lexAn, acntAnd, left, right);
    }

    case lexLogOr:
    case lexEOS:
    case lexRightParenthesis:
        return left;

    default:
    {
        lexAn.SetErrorResID(IDS_STPAR_ERR_UNEXPSYM);
        delete left;
        return NULL; // unexpected symbol
    }
    }
}

CFTPAutodetCondNode* FTP_AC_ExpNot(CFTPAutodetCondLexAn& lexAn)
{
    switch (lexAn.GetActualElement())
    {
    case lexLogNegation:
    {
        lexAn.Match();
        CFTPAutodetCondNode* operand = FTP_AC_Term(lexAn);
        if (operand == NULL)
            return NULL; // chyba, koncime
        return CreateNewACNotNode(lexAn, operand);
    }

    case lexFunction:
    case lexLeftParenthesis:
        return FTP_AC_Term(lexAn);

    default:
    {
        lexAn.SetErrorResID(IDS_STPAR_ERR_UNEXPSYM);
        return NULL; // unexpected symbol
    }
    }
}

CFTPAutodetCondNode* FTP_AC_Term(CFTPAutodetCondLexAn& lexAn)
{
    switch (lexAn.GetActualElement())
    {
    case lexFunction:
    {
        CFTPAutodetCondNode* node = CreateNewACFuncNode(lexAn);
        lexAn.Match();
        return node;
    }

    case lexLeftParenthesis:
    {
        lexAn.Match();
        CFTPAutodetCondNode* node = FTP_AC_ExpOr(lexAn);
        if (node == NULL)
            return NULL; // chyba, koncime
        if (lexAn.GetActualElement() == lexRightParenthesis)
            lexAn.Match();
        else // unexpected symbol
        {
            lexAn.SetErrorResID(IDS_STPAR_ERR_MISSINGRIGHTPAR);
            delete node;
            return NULL;
        }
        return node;
    }

    default:
    {
        lexAn.SetErrorResID(IDS_STPAR_ERR_UNEXPSYM);
        return NULL; // unexpected symbol
    }
    }
}

//
// ****************************************************************************
// CFTPAutodetCondNode
//

CFTPAutodetCondNode::~CFTPAutodetCondNode()
{
    switch (Type)
    {
    case acntOr:
    case acntAnd:
    {
        if (Left != NULL)
            delete Left;
        if (Right != NULL)
            delete Right;
        break;
    }

    case acntNot:
    {
        if (NegNode != NULL)
            delete NegNode;
        break;
    }

    case acntFunc:
    {
        if (Algorithm != NULL)
        {
            switch (Function)
            {
            case acfSyst_contains:
            case acfWelcome_contains:
            {
                SalamanderGeneral->FreeSalamanderBMSearchData((CSalamanderBMSearchData*)Algorithm);
                break;
            }

            case acfReg_exp_in_syst:
            case acfReg_exp_in_welcome:
            {
                SalamanderGeneral->FreeSalamanderREGEXPSearchData((CSalamanderREGEXPSearchData*)Algorithm);
                break;
            }

            default:
                TRACE_E("Unknown function in CFTPAutodetCondNode::~CFTPAutodetCondNode()!");
            }
        }
        break;
    }
    }
}

BOOL CFTPAutodetCondNode::Evaluate(const char* welcomeReply, int welcomeReplyLen, const char* systReply,
                                   int systReplyLen)
{
    switch (Type)
    {
    case acntAlwaysTrue:
        return TRUE;

    case acntOr:
    {
        if (Left != NULL && Right != NULL)
            return Left->Evaluate(welcomeReply, welcomeReplyLen, systReply, systReplyLen) ||
                   Right->Evaluate(welcomeReply, welcomeReplyLen, systReply, systReplyLen);
        return FALSE;
    }

    case acntAnd:
    {
        if (Left != NULL && Right != NULL)
            return Left->Evaluate(welcomeReply, welcomeReplyLen, systReply, systReplyLen) &&
                   Right->Evaluate(welcomeReply, welcomeReplyLen, systReply, systReplyLen);
        return FALSE;
    }

    case acntNot:
    {
        if (NegNode != NULL)
            return !(NegNode->Evaluate(welcomeReply, welcomeReplyLen, systReply, systReplyLen));
        return FALSE;
    }

    case acntFunc:
    {
        if (Algorithm != NULL)
        {
            switch (Function)
            {
            case acfSyst_contains:
                return (((CSalamanderBMSearchData*)Algorithm)->SearchForward(systReply, systReplyLen, 0) != -1);

            case acfWelcome_contains:
                return (((CSalamanderBMSearchData*)Algorithm)->SearchForward(welcomeReply, welcomeReplyLen, 0) != -1);

            case acfReg_exp_in_syst:
            {
                ((CSalamanderREGEXPSearchData*)Algorithm)->SetLine(systReply, systReply + systReplyLen);
                int foundLen;
                return (((CSalamanderREGEXPSearchData*)Algorithm)->SearchForward(0, foundLen) != -1);
            }

            case acfReg_exp_in_welcome:
            {
                ((CSalamanderREGEXPSearchData*)Algorithm)->SetLine(welcomeReply, welcomeReply + welcomeReplyLen);
                int foundLen;
                return (((CSalamanderREGEXPSearchData*)Algorithm)->SearchForward(0, foundLen) != -1);
            }

            default:
            {
                TRACE_E("Unknown function in CFTPAutodetCondNode::Evaluate()!");
                return FALSE;
            }
            }
        }
        else
        {
            if (Function == acfAlwaysTrue)
                return TRUE; // hledani prazdneho vzorku = always true
            else
                TRACE_E("Unknown function without algorithm in CFTPAutodetCondNode::Evaluate()!");
        }
        return FALSE;
    }

    default:
    {
        TRACE_E("Unknown type of node in CFTPAutodetCondNode::Evaluate()!");
        return FALSE;
    }
    }
}
