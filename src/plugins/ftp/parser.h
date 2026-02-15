// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// skips the identifier name (function, state variable, or column name),
// returns TRUE if at least one character was skipped (the identifier has at least one
// character); otherwise returns the 'emptyErrID' error in 'errorResID'
BOOL SkipIdentifier(const char*& s, const char* end, int* errorResID, int emptyErrID);

//
// ****************************************************************************
// CFTPParserParameter
//

// codes of state variables
enum CFTPParserStateVariables
{
    psvNone, // uninitialized
    psvFirstNonEmptyLine,
    psvLastNonEmptyLine,
    psvNextChar,
    psvNextWord,
    psvRestOfLine,
};

enum CFTPParserBinaryOperators
{
    pboNone,                // uninitialized
    pboEqual,               // equality
    pboNotEqual,            // inequality
    pboStrEqual,            // string equality (case insensitive)
    pboStrNotEqual,         // string inequality (case insensitive)
    pboSubStrIsInString,    // substring is in string (case insensitive)
    pboSubStrIsNotInString, // substring is not in string (case insensitive)
    pboStrEndWithString,    // string ends with string (case insensitive)
    pboStrNotEndWithString, // string does not end with string (case insensitive)

    // NOTE: type checking is in method CFTPParserFunction::MoveRightOperandToExpression()
};

enum CFTPParserParameterType
{
    pptNone,       // uninitialized
    pptColumnID,   // column identifier (-3==is_link, -2==is_hidden, -1==is_dir, from 0 it is an index in the 'columns' array)
    pptBoolean,    // boolean (true==1, false==0)
    pptString,     // string
    pptNumber,     // signed int64 (unsigned int64 is unnecessary)
    pptStateVar,   // state variable (see type CStateVariables)
    pptExpression, // expression (only format: first_operand + bin_operation + second_operand)
};

// constants for parameter values of type pptColumnID
#define COL_IND_ISLINK -3   // standard column "is_link"
#define COL_IND_ISHIDDEN -2 // standard column "is_hidden"
#define COL_IND_ISDIR -1    // standard column "is_dir"

// constants for operand types when testing correct operator usage in an expression
enum CFTPParserOperandType
{
    potNone, // uninitialized
    potBoolean,
    potString,
    potNumber,
    potDate,
    potTime,
};

// constants for parameter types when testing correct use of functions (expected
// number and type of parameters)
enum CFTPParserFuncParType
{
    pfptNone, // uninitialized
    pfptBoolean,
    pfptString,
    pfptNumber,
    pfptColumnBoolean,
    pfptColumnString,
    pfptColumnNumber,
    pfptColumnDate,
    pfptColumnTime,
};

struct CFTPParserParameter
{
    CFTPParserParameterType Type; // parameter type - according to this attribute other object attributes are valid/invalid

    union
    {
        int ColumnIndex; // value of a parameter of type pptColumnID

        BOOL Boolean; // value of a parameter of type pptBoolean

        char* String; // value of a parameter of type pptString

        __int64 Number; // value of a parameter of type pptNumber

        CFTPParserStateVariables StateVar; // value of a parameter of type pptStateVar

        struct // data for computing a parameter of type pptExpression
        {
            CFTPParserBinaryOperators BinOperator; // type of binary operation
            CFTPParserParameter** Parameters;      // array of two pointers to operands
        };
    };

    CFTPParserParameter() { Type = pptNone; }

    ~CFTPParserParameter()
    {
        if (Type == pptString && String != NULL)
            free(String);
        else
        {
            if (Type == pptExpression && Parameters != NULL)
            {
                if (Parameters[0] != NULL)
                    delete Parameters[0];
                if (Parameters[1] != NULL)
                    delete Parameters[1];
                delete[] Parameters;
            }
        }
    }

    // returns the operand type of the expression
    CFTPParserOperandType GetOperandType(TIndirectArray<CSrvTypeColumn>* columns);

    // returns the function parameter type
    CFTPParserFuncParType GetFuncParType(TIndirectArray<CSrvTypeColumn>* columns);

    // returns the value of a parameter of type pfptNumber; used during listing parsing
    // (obtaining the function parameter value)
    __int64 GetNumber()
    {
#ifdef _DEBUG
        if (Type != pptNumber)
        {
            TRACE_E("Unexpected situation in CFTPParserParameter::GetNumber(): not a number!");
            return 0;
        }
#endif
        return Number;
    }

    // returns the column index (parameter of type pfptColumnXXX); used during listing parsing
    // (obtaining the function parameter value - storing into a column)
    int GetColumnIndex()
    {
#ifdef _DEBUG
        if (Type != pptColumnID)
        {
            TRACE_E("Unexpected situation in CFTPParserParameter::GetColumnIndex(): not a column!");
            return -1;
        }
#endif
        return ColumnIndex;
    }

    // returns the value of a parameter or state variable of type pptString; used
    // during listing parsing; (obtaining the function parameter value); can also return
    // NULL (out of memory (TRUE assigned to 'lowMemErr') or 'String' is NULL)
    const char* GetString(const char* listing, const char* listingEnd, BOOL* needDealloc,
                          BOOL* lowMemErr);

    // returns the value of a parameter, expression, column, or state variable of type
    // pptBoolean and potBoolean; used during listing parsing;
    // (obtaining the value of a function parameter or operand)
    BOOL GetBoolean(CFileData* file, BOOL* isDir, CFTPListingPluginDataInterface* dataIface,
                    TIndirectArray<CSrvTypeColumn>* columns, const char* listing,
                    const char* listingEnd, CFTPParser* actualParser);

    // returns the value of a parameter or a value from a column of type potNumber; used during
    // listing parsing (obtaining the value of an operand); returns TRUE in 'minus' (must not be NULL)
    // if it is a negative number (only a column of type stctGeneralNumber or a parameter pptNumber)
    __int64 GetNumberOperand(CFileData* file, CFTPListingPluginDataInterface* dataIface,
                             TIndirectArray<CSrvTypeColumn>* columns, BOOL* minus);

    // returns the value from a column of type potDate; used during listing parsing
    // (obtaining the value of an operand)
    void GetDateOperand(CFTPDate* date, CFileData* file,
                        CFTPListingPluginDataInterface* dataIface,
                        TIndirectArray<CSrvTypeColumn>* columns);

    // returns the value from a column of type potTime; used during listing parsing
    // (obtaining the value of an operand)
    void GetTimeOperand(CFTPTime* time, CFileData* file,
                        CFTPListingPluginDataInterface* dataIface,
                        TIndirectArray<CSrvTypeColumn>* columns);

    // returns the value of a parameter, state variable, or value from a column of type potTime;
    // used during listing parsing (obtaining the value of an operand)
    void GetStringOperand(const char** beg, const char** end, CFileData* file,
                          CFTPListingPluginDataInterface* dataIface,
                          TIndirectArray<CSrvTypeColumn>* columns,
                          const char* listing, const char* listingEnd);
};

//
// ****************************************************************************
// CFTPParserFunction
//

// function codes
enum CFTPParserFunctionCode
{
    fpfNone, // uninitialized
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

class CFTPParserFunction
{
protected:
    CFTPParserFunctionCode Function;

    TIndirectArray<CFTPParserParameter> Parameters; // list of all rule functions

public:
    CFTPParserFunction(CFTPParserFunctionCode func) : Parameters(1, 1) { Function = func; }

    BOOL IsGood() { return Parameters.IsGood(); }

    // adds a parameter of type pptColumnID; on allocation failure returns FALSE and sets 'lowMem' (if not NULL)
    // to TRUE
    BOOL AddColumnIDParameter(int columnIndex, BOOL* lowMem);

    // adds a parameter of type pptString; 'str' is an allocated string, the method ensures it is freed;
    // on allocation failure returns FALSE and sets 'lowMem' (if not NULL) to TRUE
    BOOL AddStringParameter(char* str, BOOL* lowMem);

    // adds a parameter of type pptNumber; on allocation failure returns FALSE and sets 'lowMem'
    // (if not NULL) to TRUE
    BOOL AddNumberParameter(__int64 number, BOOL* lowMem);

    // adds a parameter of type pptStateVar; on allocation failure returns FALSE and sets 'lowMem'
    // (if not NULL) to TRUE
    BOOL AddStateVarParameter(CFTPParserStateVariables var, BOOL* lowMem);

    // adds a parameter of type pptBoolean; on allocation failure returns FALSE and sets 'lowMem'
    // (if not NULL) to TRUE
    BOOL AddBooleanParameter(BOOL boolVal, BOOL* lowMem);

    // replaces the last parameter with a newly allocated parameter of type pptExpression; the replaced
    // parameter (before calling the method the last in Parameters) is used as the left operand
    // of the resulting expression; on allocation failure returns FALSE and sets 'lowMem' (if not NULL)
    // to TRUE
    BOOL AddExpressionParameter(CFTPParserBinaryOperators oper, BOOL* lowMem);

    // moves the last parameter into a parameter of type pptExpression (which is the second to last);
    // returns FALSE on improper call ('lowMem' (if not NULL) is then set to TRUE) or
    // if operand types are unsuitable for the operator (writes the error text into 'errorResID'
    // (if not NULL))
    BOOL MoveRightOperandToExpression(TIndirectArray<CSrvTypeColumn>* columns,
                                      int* errorResID, BOOL* lowMem, BOOL* colAssigned);

    // checks the type and number of parameters used by this function; returns FALSE
    // if the number or type of parameters is unsuitable for the function (writes the error text into
    // 'errorResID' (if not NULL))
    BOOL CheckParameters(TIndirectArray<CSrvTypeColumn>* columns, int* errorResID,
                         BOOL* colAssigned);

    // tries to use this function on the listing text from the "pointer" position; stores parsed values
    // into 'file'+'isDir' (must not be NULL) (if the function assigns to a column); 'dataIface' is the
    // interface for working with values stored outside CFileData (for working with CFileData::PluginData);
    // 'columns' are user-defined columns; 'listing' is the "pointer" in the listing (position to start parsing),
    // after using the function it returns a new "pointer" position; 'listingEnd' is the end of the listing;
    // if a value assignment to a column occurs, the field 'emptyCol' at the column index must be set to FALSE;
    // returns TRUE when the function was used successfully, returns FALSE on errors: out of memory (returns TRUE
    // in 'lowMemErr' (must not be NULL)) or when this function cannot be used
    BOOL UseFunction(CFileData* file, BOOL* isDir, CFTPListingPluginDataInterface* dataIface,
                     TIndirectArray<CSrvTypeColumn>* columns, const char** listing,
                     const char* listingEnd, CFTPParser* actualParser, BOOL* lowMemErr,
                     DWORD* emptyCol);

protected:
    // adds a newly allocated parameter (without initialization) and returns it in 'newPar';
    // on allocation failure returns FALSE and sets 'lowMem' (if not NULL) to TRUE
    BOOL AddParameter(CFTPParserParameter*& newPar, BOOL* lowMem);
};

//
// ****************************************************************************
// CFTPParserRule
//

class CFTPParserRule
{
protected:
    TIndirectArray<CFTPParserFunction> Functions; // list of all functions of the rule

public:
    CFTPParserRule() : Functions(5, 5) {}

    BOOL IsGood() { return Functions.IsGood(); }

    // returns TRUE if the function in the rule was compiled successfully (up to the ')' symbol);
    // on error returns FALSE and sets 'errorResID' (number of the string describing
    // the error - stored in resources) or 'lowMem' (TRUE = low memory)
    BOOL CompileNewFunction(CFTPParserFunctionCode func, const char*& rules, const char* rulesEnd,
                            TIndirectArray<CSrvTypeColumn>* columns, int* errorResID,
                            BOOL* lowMem, BOOL* colAssigned);

    // tries to use this rule on the listing text from the "pointer" position; returns parsed data in 'file'
    // (must not be NULL) (it is a file or directory only if emptyCol[0]==FALSE - see 'emptyCol' description);
    // returns TRUE in 'isDir' (must not be NULL) if a directory was parsed and FALSE if a file was parsed;
    // 'dataIface' is the interface for working with values stored outside CFileData (for working with
    // CFileData::PluginData); 'columns' are user-defined columns; 'listing' is the "pointer" in the listing
    // (position to start parsing); when the rule is used successfully, upon returning from the method it points
    // to the beginning of the next (first unprocessed) line; 'listingEnd' is the end of the listing;
    // the 'emptyCol' array initially contains TRUE for each column; if a value assignment to a column occurs,
    // the array at the column index must be set to FALSE; returns TRUE when the rule is used successfully,
    // returns FALSE on errors: out of memory (returns TRUE in 'lowMemErr' (must not be NULL)) or when this rule
    // cannot be used
    BOOL UseRule(CFileData* file, BOOL* isDir, CFTPListingPluginDataInterface* dataIface,
                 TIndirectArray<CSrvTypeColumn>* columns, const char** listing,
                 const char* listingEnd, CFTPParser* actualParser, BOOL* lowMemErr,
                 DWORD* emptyCol);
};

//
// ****************************************************************************
// CFTPParser
//

// bits for the date column index in 'emptyCol' (see CFTPParser::GetNextItemFromListing)
#define DATE_MASK_DAY 0x02                  // the date has the day set (starting at 0x02 because TRUE = 0x01)
#define DATE_MASK_MONTH 0x04                // the date has the month set
#define DATE_MASK_YEAR 0x08                 // the date has the year set
#define DATE_MASK_DATE 0x0E                 // the date is set completely
#define DATE_MASK_YEARCORRECTIONNEEDED 0x10 // the year in the date still needs to be corrected (used with "year_or_time")
#define DATE_MASK_TIME 0x100                // the time is set completely (it is a different column, it could be 0x02, but we use 0x10 for easier error detection)

// constants for CFTPParser::AllowedLanguagesMask
#define PARSER_LANG_ALL 0xFFFF
#define PARSER_LANG_ENGLISH 0x0001
#define PARSER_LANG_GERMAN 0x0002
#define PARSER_LANG_NORWEIGAN 0x0004
#define PARSER_LANG_SWEDISH 0x0008

class CFTPParser
{
protected:
    TIndirectArray<CFTPParserRule> Rules; // list of all rules of the parser

public:                             // helper variables used while parsing the listing:
    int ActualYear;                 // year from today's date (used by the "year_or_time" function)
    int ActualMonth;                // month from today's date (used by the "year_or_time" function)
    int ActualDay;                  // day from today's date (used by the "year_or_time" function)
    const char* FirstNonEmptyBeg;   // start of the first non-empty line
    const char* FirstNonEmptyEnd;   // end of the first non-empty line
    const char* LastNonEmptyBeg;    // start of the last non-empty line
    const char* LastNonEmptyEnd;    // end of the last non-empty line
    const char* ListingBeg;         // beginning of the listing
    BOOL ListingIncomplete;         // TRUE if the listing is incomplete
    BOOL SkipThisLineItIsIncomlete; // TRUE only if the rule processing detected that the listing is incomplete - skip the trailing part of the listing processed by this rule
    DWORD AllowedLanguagesMask;     // allowed languages for functions month_3 and month_txt (bit combination of PARSER_LANG_XXX constants) - purpose: to avoid mixing languages when detecting months

public:
    CFTPParser() : Rules(5, 5)
    {
        ActualYear = 0;
        ActualMonth = 0;
        ActualDay = 0;
        ListingBeg = FirstNonEmptyBeg = FirstNonEmptyEnd = LastNonEmptyBeg = LastNonEmptyEnd = NULL;
        ListingIncomplete = FALSE;
        AllowedLanguagesMask = PARSER_LANG_ALL;
    }

    BOOL IsGood() { return Rules.IsGood(); }

    // returns TRUE if the rule was compiled successfully (up to the ';' symbol);
    // on error returns FALSE and sets 'errorResID' (number of the string describing
    // the error - stored in resources) or 'lowMem' (TRUE = low memory)
    BOOL CompileNewRule(const char*& rules, const char* rulesEnd,
                        TIndirectArray<CSrvTypeColumn>* columns,
                        int* errorResID, BOOL* lowMem, BOOL* colAssigned);

    // parser initialization - must be called before parsing (before calling GetNextItemFromListing);
    // 'listingBeg' is the start of the listing (pointer to the first character of the complete listing);
    // 'listingEnd' is the end of the listing; 'actualYear'+'actualMonth'+'actualDay' is year+month+day
    // from today's date; 'listingIncomplete' is TRUE if the listing text is incomplete
    // (its download was interrupted)
    void BeforeParsing(const char* listingBeg, const char* listingEnd, int actualYear,
                       int actualMonth, int actualDay, BOOL listingIncomplete);

    // parses one file or directory from the listing; returns the result in 'file' (must not be NULL);
    // returns TRUE in 'isDir' (must not be NULL) if it is a directory, FALSE if it is a file;
    // 'dataIface' is the interface for working with values stored outside CFileData (for working with
    // CFileData::PluginData); 'columns' are user-defined columns; 'listing' is the "pointer" in the listing
    // (position to start parsing); upon returning from the method it points to the beginning of the next
    // (first unprocessed) line; 'listingEnd' is the end of the listing;
    // returns a pointer to the buffer '*listing' in 'itemStart' (if not NULL) at
    // the beginning of the line from which the returned file/directory was parsed;
    // 'emptyCol' is a preallocated helper array (at least columns->Count elements);
    // returns TRUE on success, returns FALSE if the end of the listing was reached (then
    // '*listing' equals 'listingEnd') or if this parser cannot process the listing
    // (then '*listing' does not equal 'listingEnd') or on memory allocation failure
    // (then 'lowMem' (if not NULL) is TRUE)
    BOOL GetNextItemFromListing(CFileData* file, BOOL* isDir,
                                CFTPListingPluginDataInterface* dataIface,
                                TIndirectArray<CSrvTypeColumn>* columns,
                                const char** listing, const char* listingEnd,
                                const char** itemStart, BOOL* lowMem, DWORD* emptyCol);
};

//
// ****************************************************************************
// CFTPAutodetCondNode
//

// function types in the autodetection condition
enum CFTPAutodetCondFunction
{
    acfNone,
    acfAlwaysTrue, // searching for an empty string = always true
    acfSyst_contains,
    acfWelcome_contains,
    acfReg_exp_in_syst,
    acfReg_exp_in_welcome,
};

// types for nodes of the autodetection condition
enum CFTPAutodetCondNodeType
{
    acntNone,       // uninitialized
    acntAlwaysTrue, // empty condition = always true
    acntOr,         // OR
    acntAnd,        // AND
    acntNot,        // NOT
    acntFunc,       // function with a string-search algorithm (see CFTPAutodetCondFunction)
};

class CFTPAutodetCondNode
{
public:
    CFTPAutodetCondNodeType Type; // node type - according to this attribute other object attributes are valid/invalid

    union
    {
        struct // left and right operands for acntOr and acntAnd
        {
            CFTPAutodetCondNode* Left;
            CFTPAutodetCondNode* Right;
        };

        CFTPAutodetCondNode* NegNode; // operand for acntNot

        struct // function type and search algorithm (Moore/RegExp) for acntFunc
        {
            CFTPAutodetCondFunction Function;
            void* Algorithm; // either (CSalamanderBMSearchData *) or (CSalamanderREGEXPSearchData *)
        };
    };

public:
    CFTPAutodetCondNode() { Type = acntNone; }
    ~CFTPAutodetCondNode();

    // evaluates the autodetection condition based on the 'welcomeReply' and 'systReply' strings;
    // returns the result of the condition expression (TRUE/FALSE)
    BOOL Evaluate(const char* welcomeReply, int welcomeReplyLen, const char* systReply,
                  int systReplyLen);
};

// allocates the parser and creates its structure according to the 'rules' string (parsing rules);
// 'columns' are defined columns; on error returns NULL; returns TRUE in 'lowMem' (if not NULL)
// if the error was caused by lack of memory; returns the offset of a syntactic or semantic error
// inside 'rules' in 'errorPos' (if not NULL) (-1=unknown error position), and at the same time
// returns the number of the string describing the error in 'errorResID' (if not NULL)
// (stored in resources; -1=no error description)
CFTPParser* CompileParsingRules(const char* rules, TIndirectArray<CSrvTypeColumn>* columns,
                                int* errorPos, int* errorResID, BOOL* lowMem);

// loads the autodetection condition from the 'cond' string and stores it in an allocated tree,
// whose root it returns; on error returns NULL; returns TRUE in 'lowMem' (if not NULL) if the error
// was caused by lack of memory; returns the offset of a syntactic error inside 'cond' (-1=unknown error position)
// in 'errorPos' (if not NULL); simultaneously returns the number of the string describing the error in 'errorResID'
// (if not NULL) (stored in resources; -1=no error description) and returns a textual error description in the
// 'errBuf'+'errBufSize' buffer (has higher priority than 'errorResID')
CFTPAutodetCondNode* CompileAutodetectCond(const char* cond, int* errorPos, int* errorResID,
                                           BOOL* lowMem, char* errBuf, int errBufSize);

// fills empty values into empty columns; on error returns TRUE in 'err';
// 'file'+'isDir'+'dataIface' are the data of the processed item (file/directory);
// 'columns' are the columns in the panel; returns TRUE in 'lowMem' (if not NULL)
// when the error is caused by lack of memory; 'emptyCol' is an array of DWORDs for all
// columns, TRUE at an index means the column is empty; if 'emptyCol' is NULL,
// only the name is filled (Name+NameLen+Hidden), the rest is empty and filled in
// from empty-values; 'actualYear'+'actualMonth'+'actualDay' is year+month+day
// from today's date (used for correcting the year of a date produced by
// "year_or_time")
void FillEmptyValues(BOOL& err, CFileData* file, BOOL isDir,
                     CFTPListingPluginDataInterface* dataIface,
                     TIndirectArray<CSrvTypeColumn>* columns,
                     BOOL* lowMem, DWORD* emptyCol, int actualYear,
                     int actualMonth, int actualDay);
