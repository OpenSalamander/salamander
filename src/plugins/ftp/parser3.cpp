// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CFTPAutodetCondLexAn
//

// lexical symbols for translating the autodetection condition
enum CFTPAutodetCondLexElement
{
    lexNone,             // not initialized
    lexEOS,              // end of string reached
    lexUnknown,          // unknown symbol
    lexLogOr,            // OR
    lexLogAnd,           // AND
    lexLogNegation,      // NOT
    lexLeftParenthesis,  // (
    lexRightParenthesis, // )
    lexFunction,         // function including one parameter in parentheses
};

class CFTPAutodetCondLexAn
{
protected:
    const char* CondBeg;    // beginning of the autodetection condition text
    const char* CondEnd;    // end of the autodetection condition text
    const char* Cond;       // start position of the current symbol in the autodetection condition text
    const char* CondSymEnd; // end position of the current symbol in the autodetection condition text (NULL = unknown)

    CFTPAutodetCondLexElement ActElem; // current symbol (lexNone = it is still necessary to find the symbol)

    int* ErrorResID; // if not NULL, stores the id of the string in resources that describes the error
    BOOL* LowMem;    // if not NULL, stores TRUE for an error caused by lack of memory
    char* ErrBuf;    // buffer of size ErrBufSize for the textual description of the error (higher priority than ErrorResID)
    int ErrBufSize;  // size of the ErrBuf buffer (0 = NULL buffer)

    CFTPAutodetCondFunction ActFunction; // function data (only if the current symbol is lexFunction)
    void* ActFuncAlgorithm;              // either (CSalamanderBMSearchData*) or (CSalamanderREGEXPSearchData*)

public:
    CFTPAutodetCondLexAn(const char* cond, const char* condEnd, int* errorResID, BOOL* lowMem,
                         char* errBuf, int errBufSize);
    ~CFTPAutodetCondLexAn() { ReleaseActFuncAlgorithm(); }

    // release ActFuncAlgorithm and set it to NULL
    void ReleaseActFuncAlgorithm();

    // return the code of the current lexical element
    CFTPAutodetCondLexElement GetActualElement();

    // skip one lexical element
    void Match();

    // return data for the function; the current symbol must be lexFunction and it can be
    // called only once for each function symbol
    void GiveFunctionData(CFTPAutodetCondFunction& function, void*& algorithm);

    // return the position of the current symbol in the text
    int GetActualSymPos() { return (int)(Cond - CondBeg); }

    // auxiliary error parameters (they set the "global" variables)
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
                    // total number of functions (UPDATE !!! + keep in sync with CFTPAutodetCondFunction + also update functionCodes)
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
                        // search for the function parameter
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
                                int esc = 0; // number of escape sequences
                                BOOL strEndFound = FALSE;
                                while (s < end)
                                {
                                    if (*s == '"') // end of string
                                    {
                                        strEndFound = TRUE;
                                        break;
                                    }
                                    if (*s == '\r' || *s == '\n')
                                        break;                     // the string cannot contain EOL directly (use escape sequences '\r' and '\n')
                                    if (*s == '\\' && s + 1 < end) // escape sequence
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
                                if (strEndFound) // string is OK
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
                                        s++; // skip the closing '"' of the string

                                        while (s < end && *s <= ' ')
                                            s++;
                                        if (s < end && *s == ')')
                                        {
                                            Cond = beg;
                                            CondSymEnd = s + 1;
                                            if (str[0] == 0) // empty pattern = always true
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
                                                        ActFuncAlgorithm = NULL; // report low memory
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
                                                        setLowMemErr = FALSE; // the error is already set
                                                    }
                                                }
                                                break;
                                            }

                                            default:
                                            {
                                                TRACE_E("Unknown function in CFTPAutodetCondLexAn::GetActualElement()!");
                                                ActFuncAlgorithm = NULL; // we end with "low memory"
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
        function = ActFunction; // function data (only if the current symbol is lexFunction)
        algorithm = ActFuncAlgorithm;
        ActFunction = acfNone; // from now on the data are no longer ours
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
    if (lexAn.GetActualElement() != lexEOS) // non-empty condition -> start parsing
    {
        CFTPAutodetCondNode* node = FTP_AC_ExpOr(lexAn);
        if (node != NULL && lexAn.GetActualElement() != lexEOS) // if the entire string is not parsed, it is an error (the "empty" rule from the grammar is "unpacked" here)
        {
            lexAn.SetErrorResID(IDS_STPAR_ERR_UNEXPSYM);
            delete node;
            node = NULL;
        }
        if (node == NULL && errorPos != NULL)
            *errorPos = lexAn.GetActualSymPos();
        return node;
    }
    else // empty condition -> always true
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
// functions for compiling the autodetection condition
//

// allocates a new node for an operator; 'type' can only be 'acntOr' or 'acntAnd';
// 'left'+'right' are operands; returns the allocated node; on lack of memory
// deallocates 'left' and 'right' and returns NULL
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

// allocates a new node for negation; 'operand' is the operand of negation; returns the allocated
// node; on lack of memory deallocates 'operand' and returns NULL
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

// allocates a new node for a function loaded from 'lexAn'; returns the allocated node or
// NULL when memory is insufficient
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
        return NULL; // error, abort
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
            return NULL; // error, abort
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
        return NULL; // error, abort
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
            return NULL; // error, abort
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
            return NULL; // error, abort
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
            return NULL; // error, abort
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
                return TRUE; // searching an empty pattern = always true
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
