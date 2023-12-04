// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

int DialogWidth;
int DialogHeight;
BOOL Maximized;

CRenamerOptions LastOptions;
char LastMask[MAX_GROUPMASK];
BOOL LastSubdirs;
BOOL LastRemoveSourcePath;

LOGFONT ManualModeLogFont;
BOOL UseCustomFont;
BOOL ConfirmESCClose;

HACCEL HAccels;

DWORD Silent;

char* MaskHistory[MAX_HISTORY_ENTRIES];
char* NewNameHistory[MAX_HISTORY_ENTRIES];
char* SearchHistory[MAX_HISTORY_ENTRIES];
char* ReplaceHistory[MAX_HISTORY_ENTRIES];
char* CommandHistory[MAX_HISTORY_ENTRIES];

// ****************************************************************************
//
// CRenamerDialog
//

MENU_TEMPLATE_ITEM MenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_ALL, 0, -1, 0, NULL},

        // TRANSLATOR_INSERT: Dialog: IDD_RENAME

        // File
        {MNTT_PB, IDS_MENU_FILE, MNTS_ALL, CMD_FILEPOPUP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_RENAME, MNTS_ALL, IDOK, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VALIDATE, MNTS_ALL, CMD_VALIDATE, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_UNDO, MNTS_ALL, CMD_UNDO, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_ALL, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EXIT, MNTS_ALL, IDCANCEL, -1, 0, NULL},
        {MNTT_PE},

        // Edit
        {MNTT_PB, IDS_MENU_EDIT, MNTS_ALL, CMD_EDITPOPUP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_STORELIST, MNTS_ALL, CMD_STORELIST, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_LOADLIST, MNTS_ALL, CMD_LOADLIST, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDITLIST, MNTS_ALL, CMD_EDIT, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILTER, MNTS_ALL, CMD_FILTER, -1, 0, NULL},
        {MNTT_PE},

        // View
        {MNTT_PB, IDS_MENU_VIEW, MNTS_ALL, CMD_VIEWPOPUP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_SORTOLD, MNTS_ALL, CMD_SORTOLD, -1, 0, NULL},
        //{MNTT_IT, IDS_MENU_SORTNEW,   MNTS_ALL, CMD_SORTNEW,  -1, 0,  NULL},
        {MNTT_IT, IDS_MENU_SORTSIZE, MNTS_ALL, CMD_SORTSIZE, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_SORTTIME, MNTS_ALL, CMD_SORTTIME, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_SORTPATH, MNTS_ALL, CMD_SORTPATH, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_ALL, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_ALLFILES, MNTS_ALL, CMD_ALLFILES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_RENAMED, MNTS_ALL, CMD_RENAMED, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_NOTRENAMED, MNTS_ALL, CMD_NOTRENAMED, -1, 0, NULL},
        {MNTT_PE},

        // Options
        {MNTT_PB, IDS_MENU_OPTIONS, MNTS_ALL, CMD_OPTIONSPOPUP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILENAMES, MNTS_ALL, CMD_FILENAMES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_RELATIVEPATH, MNTS_ALL, CMD_RELATIVEPATH, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FULLPATH, MNTS_ALL, CMD_FULLPATH, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_ALL, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_REMOVESOURCEPATH, MNTS_ALL, CMD_RMSOURCEPATH, -1, 0, NULL},
        // {MNTT_SP, -1,      MNTS_ALL, 0,    -1, 0,  NULL},
        //   // Manual Mode Font
        //   {MNTT_PB, IDS_MENU_FONT,   MNTS_ALL, CMD_FONTPOPUP,  -1, 0,  NULL},
        //   {MNTT_IT, IDS_MENU_CUSTOMFONT, MNTS_ALL, CMD_FONT, -1, 0,  NULL},
        //   {MNTT_IT, IDS_MENU_AUTOFONT, MNTS_ALL, CMD_AUTOFONT, -1, 0,  NULL},
        //   {MNTT_PE},
        {MNTT_SP, -1, MNTS_ALL, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_LAST, MNTS_ALL, CMD_LAST, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_RESET, MNTS_ALL, CMD_RESET, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_ALL, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_ADVANCED, MNTS_ALL, CMD_ADVANCED, -1, 0, NULL},
        {MNTT_PE},

        // Help
        {MNTT_PB, IDS_MENU_HELP, MNTS_ALL, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_BATCHRENDLG, MNTS_ALL, IDHELP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_REGEXP, MNTS_ALL, CMD_HELPREGEXP, -1, 0, NULL},
        {MNTT_PE},

        {MNTT_PE}};

MENU_TEMPLATE_ITEM RenameButtonMenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_ALL, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_RENAME, MNTS_ALL, IDOK, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VALIDATE, MNTS_ALL, CMD_VALIDATE, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_UNDO, MNTS_ALL, CMD_UNDO, -1, 0, NULL},
        {MNTT_PE}};

BOOL GetCounterParamFormat(HWND parent, char* buffer);
BOOL GetTimeParamFormat(HWND parent, char* buffer);
BOOL GetDateParamFormat(HWND parent, char* buffer);
BOOL GetLowerCaseParam(HWND parent, char* buffer);
BOOL GetUpperCaseParam(HWND parent, char* buffer);
BOOL GetMixedCaseParam(HWND parent, char* buffer);
BOOL GetStripDiaParam(HWND parent, char* buffer);
BOOL GetLowerCaseCutParam(HWND parent, char* buffer);
BOOL GetUpperCaseCutParam(HWND parent, char* buffer);
BOOL GetMixedCaseCutParam(HWND parent, char* buffer);
BOOL GetStripDiaCutParam(HWND parent, char* buffer);
BOOL GetCutParam(HWND parent, char* buffer);

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM NewNameLowerHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_VAR_ORIGINALNAME
  {MNTT_IT, IDS_VAR_DRIVE
  {MNTT_IT, IDS_VAR_PATH
  {MNTT_IT, IDS_VAR_RELATIVEPATH
  {MNTT_IT, IDS_VAR_NAME
  {MNTT_IT, IDS_VAR_NAMEPART
  {MNTT_IT, IDS_VAR_EXTPART
  {MNTT_IT, IDS_VAR_CUT
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem NewNameLowerHelpMenu[] =
    {
        {IDS_VAR_ORIGINALNAME, VarOriginalName, GetLowerCaseParam, NULL},
        {IDS_VAR_DRIVE, VarDrive, GetLowerCaseParam, NULL},
        {IDS_VAR_PATH, VarPath, GetLowerCaseParam, NULL},
        {IDS_VAR_RELATIVEPATH, VarRelativePath, GetLowerCaseParam, NULL},
        {IDS_VAR_NAME, VarName, GetLowerCaseParam, NULL},
        {IDS_VAR_NAMEPART, VarNamePart, GetLowerCaseParam, NULL},
        {IDS_VAR_EXTPART, VarExtPart, GetLowerCaseParam, NULL},
        {IDS_VAR_CUT, NULL, GetLowerCaseCutParam, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM NewNameUpperHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_VAR_ORIGINALNAME
  {MNTT_IT, IDS_VAR_DRIVE
  {MNTT_IT, IDS_VAR_PATH
  {MNTT_IT, IDS_VAR_RELATIVEPATH
  {MNTT_IT, IDS_VAR_NAME
  {MNTT_IT, IDS_VAR_NAMEPART
  {MNTT_IT, IDS_VAR_EXTPART
  {MNTT_IT, IDS_VAR_CUT
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem NewNameUpperHelpMenu[] =
    {
        {IDS_VAR_ORIGINALNAME, VarOriginalName, GetUpperCaseParam, NULL},
        {IDS_VAR_DRIVE, VarDrive, GetUpperCaseParam, NULL},
        {IDS_VAR_PATH, VarPath, GetUpperCaseParam, NULL},
        {IDS_VAR_RELATIVEPATH, VarRelativePath, GetUpperCaseParam, NULL},
        {IDS_VAR_NAME, VarName, GetUpperCaseParam, NULL},
        {IDS_VAR_NAMEPART, VarNamePart, GetUpperCaseParam, NULL},
        {IDS_VAR_EXTPART, VarExtPart, GetUpperCaseParam, NULL},
        {IDS_VAR_CUT, NULL, GetUpperCaseCutParam, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM NewNameMixedHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_VAR_ORIGINALNAME
  {MNTT_IT, IDS_VAR_DRIVE
  {MNTT_IT, IDS_VAR_PATH
  {MNTT_IT, IDS_VAR_RELATIVEPATH
  {MNTT_IT, IDS_VAR_NAME
  {MNTT_IT, IDS_VAR_NAMEPART
  {MNTT_IT, IDS_VAR_EXTPART
  {MNTT_IT, IDS_VAR_CUT
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem NewNameMixedHelpMenu[] =
    {
        {IDS_VAR_ORIGINALNAME, VarOriginalName, GetMixedCaseParam, NULL},
        {IDS_VAR_DRIVE, VarDrive, GetMixedCaseParam, NULL},
        {IDS_VAR_PATH, VarPath, GetMixedCaseParam, NULL},
        {IDS_VAR_RELATIVEPATH, VarRelativePath, GetMixedCaseParam, NULL},
        {IDS_VAR_NAME, VarName, GetMixedCaseParam, NULL},
        {IDS_VAR_NAMEPART, VarNamePart, GetMixedCaseParam, NULL},
        {IDS_VAR_EXTPART, VarExtPart, GetMixedCaseParam, NULL},
        {IDS_VAR_CUT, NULL, GetMixedCaseCutParam, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM NewNameStripDiaHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_VAR_ORIGINALNAME
  {MNTT_IT, IDS_VAR_DRIVE
  {MNTT_IT, IDS_VAR_PATH
  {MNTT_IT, IDS_VAR_RELATIVEPATH
  {MNTT_IT, IDS_VAR_NAME
  {MNTT_IT, IDS_VAR_NAMEPART
  {MNTT_IT, IDS_VAR_EXTPART
  {MNTT_IT, IDS_VAR_CUT
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem NewNameStripDiaHelpMenu[] =
    {
        {IDS_VAR_ORIGINALNAME, VarOriginalName, GetStripDiaParam, NULL},
        {IDS_VAR_DRIVE, VarDrive, GetStripDiaParam, NULL},
        {IDS_VAR_PATH, VarPath, GetStripDiaParam, NULL},
        {IDS_VAR_RELATIVEPATH, VarRelativePath, GetStripDiaParam, NULL},
        {IDS_VAR_NAME, VarName, GetStripDiaParam, NULL},
        {IDS_VAR_NAMEPART, VarNamePart, GetStripDiaParam, NULL},
        {IDS_VAR_EXTPART, VarExtPart, GetStripDiaParam, NULL},
        {IDS_VAR_CUT, NULL, GetStripDiaCutParam, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM NewNameHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_VAR_ORIGINALNAME
  {MNTT_IT, IDS_VAR_DRIVE
  {MNTT_IT, IDS_VAR_PATH
  {MNTT_IT, IDS_VAR_RELATIVEPATH
  {MNTT_IT, IDS_VAR_NAME
  {MNTT_IT, IDS_VAR_NAMEPART
  {MNTT_IT, IDS_VAR_EXTPART
  {MNTT_IT, IDS_VAR_CUT
  {MNTT_IT, IDS_VAR_LOWER
  {MNTT_IT, IDS_VAR_UPPER
  {MNTT_IT, IDS_VAR_MIXED
  {MNTT_IT, IDS_VAR_STRIPDIA
  {MNTT_IT, IDS_VAR_SIZE
  {MNTT_IT, IDS_VAR_TIME
  {MNTT_IT, IDS_VAR_DATE
  {MNTT_IT, IDS_VAR_COUNTER
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem NewNameHelpMenu[] =
    {
        {IDS_VAR_ORIGINALNAME, VarOriginalName, NULL, NULL},
        {IDS_VAR_DRIVE, VarDrive, NULL, NULL},
        {IDS_VAR_PATH, VarPath, NULL, NULL},
        {IDS_VAR_RELATIVEPATH, VarRelativePath, NULL, NULL},
        {IDS_VAR_NAME, VarName, NULL, NULL},
        {IDS_VAR_NAMEPART, VarNamePart, NULL, NULL},
        {IDS_VAR_EXTPART, VarExtPart, NULL, NULL},
        {IDS_VAR_CUT, NULL, GetCutParam, NULL},
        {-1, NULL, NULL, NULL},
        {IDS_VAR_LOWER, NULL, NULL, NewNameLowerHelpMenu},
        {IDS_VAR_UPPER, NULL, NULL, NewNameUpperHelpMenu},
        {IDS_VAR_MIXED, NULL, NULL, NewNameMixedHelpMenu},
        {IDS_VAR_STRIPDIA, NULL, NULL, NewNameStripDiaHelpMenu},
        {-1, NULL, NULL, NULL},
        {IDS_VAR_SIZE, VarSize, NULL, NULL},
        {IDS_VAR_TIME, VarTime, GetTimeParamFormat, NULL},
        {IDS_VAR_DATE, VarDate, GetDateParamFormat, NULL},
        {IDS_VAR_COUNTER, VarCounter, GetCounterParamFormat, NULL},
        {0, NULL, NULL, NULL}};

BOOL GetREPositiveSet(HWND parent, char* buffer);
BOOL GetRENegativeSet(HWND parent, char* buffer);

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM LiteralsHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_LEFTPAREN
  {MNTT_IT, IDS_RE_RIGHTPAREN
  {MNTT_IT, IDS_RE_LEFTBRACKET
  {MNTT_IT, IDS_RE_CIRCUMFLEX
  {MNTT_IT, IDS_RE_DOLLAR
  {MNTT_IT, IDS_RE_DOT
  {MNTT_IT, IDS_RE_PIPE
  {MNTT_IT, IDS_RE_ASTERISK
  {MNTT_IT, IDS_RE_PLUS
  {MNTT_IT, IDS_RE_QUESTIONMARK
  {MNTT_IT, IDS_RE_BACKSLASH
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem LiteralsHelpMenu[] =
    {
        {IDS_RE_LEFTPAREN, "\\(", NULL, NULL},
        {IDS_RE_RIGHTPAREN, "\\)", NULL, NULL},
        {IDS_RE_LEFTBRACKET, "\\[", NULL, NULL},
        {IDS_RE_CIRCUMFLEX, "\\^", NULL, NULL},
        {IDS_RE_DOLLAR, "\\$", NULL, NULL},
        {IDS_RE_DOT, "\\.", NULL, NULL},
        {IDS_RE_PIPE, "\\|", NULL, NULL},
        {IDS_RE_ASTERISK, "\\*", NULL, NULL},
        {IDS_RE_PLUS, "\\+", NULL, NULL},
        {IDS_RE_QUESTIONMARK, "\\?", NULL, NULL},
        {IDS_RE_BACKSLASH, "\\\\", NULL, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM AnyFromHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_POSIX_ALPHA
  {MNTT_IT, IDS_RE_POSIX_ALNUM
  {MNTT_IT, IDS_RE_POSIX_ASCII
  {MNTT_IT, IDS_RE_POSIX_CNTRL
  {MNTT_IT, IDS_RE_POSIX_DIGIT
  {MNTT_IT, IDS_RE_POSIX_GRAPH
  {MNTT_IT, IDS_RE_POSIX_LOWER
  {MNTT_IT, IDS_RE_POSIX_PRINT
  {MNTT_IT, IDS_RE_POSIX_PUNCT
  {MNTT_IT, IDS_RE_POSIX_SPACE
  {MNTT_IT, IDS_RE_POSIX_UPPER
  {MNTT_IT, IDS_RE_POSIX_WORD
  {MNTT_IT, IDS_RE_POSIX_XDIGIT
  {MNTT_IT, IDS_RE_DEFINESET
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem AnyFromHelpMenu[] =
    {
        {IDS_RE_POSIX_ALPHA, "[[:alpha:]]", NULL, NULL},
        {IDS_RE_POSIX_ALNUM, "[[:alnum:]]", NULL, NULL},
        {IDS_RE_POSIX_ASCII, "[[:ascii:]]", NULL, NULL},
        {IDS_RE_POSIX_CNTRL, "[[:cntrl:]]", NULL, NULL},
        {IDS_RE_POSIX_DIGIT, "[[:digit:]]", NULL, NULL},
        {IDS_RE_POSIX_GRAPH, "[[:graph:]]", NULL, NULL},
        {IDS_RE_POSIX_LOWER, "[[:lower:]]", NULL, NULL},
        {IDS_RE_POSIX_PRINT, "[[:print:]]", NULL, NULL},
        {IDS_RE_POSIX_PUNCT, "[[:punct:]]", NULL, NULL},
        {IDS_RE_POSIX_SPACE, "[[:space:]]", NULL, NULL},
        {IDS_RE_POSIX_UPPER, "[[:upper:]]", NULL, NULL},
        {IDS_RE_POSIX_WORD, "[[:word:]]", NULL, NULL},
        {IDS_RE_POSIX_XDIGIT, "[[:xdigit:]]", NULL, NULL},
        {IDS_RE_DEFINESET, NULL, GetREPositiveSet, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM AnyButHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_POSIX_ALPHA
  {MNTT_IT, IDS_RE_POSIX_ALNUM
  {MNTT_IT, IDS_RE_POSIX_ASCII
  {MNTT_IT, IDS_RE_POSIX_CNTRL
  {MNTT_IT, IDS_RE_POSIX_DIGIT
  {MNTT_IT, IDS_RE_POSIX_GRAPH
  {MNTT_IT, IDS_RE_POSIX_LOWER
  {MNTT_IT, IDS_RE_POSIX_PRINT
  {MNTT_IT, IDS_RE_POSIX_PUNCT
  {MNTT_IT, IDS_RE_POSIX_SPACE
  {MNTT_IT, IDS_RE_POSIX_UPPER
  {MNTT_IT, IDS_RE_POSIX_WORD
  {MNTT_IT, IDS_RE_POSIX_XDIGIT
  {MNTT_IT, IDS_RE_DEFINESET
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem AnyButHelpMenu[] =
    {
        {IDS_RE_POSIX_ALPHA, "[[:^alpha:]]", NULL, NULL},
        {IDS_RE_POSIX_ALNUM, "[[:^alnum:]]", NULL, NULL},
        {IDS_RE_POSIX_ASCII, "[[:^ascii:]]", NULL, NULL},
        {IDS_RE_POSIX_CNTRL, "[[:^cntrl:]]", NULL, NULL},
        {IDS_RE_POSIX_DIGIT, "[[:^digit:]]", NULL, NULL},
        {IDS_RE_POSIX_GRAPH, "[[:^graph:]]", NULL, NULL},
        {IDS_RE_POSIX_LOWER, "[[:^lower:]]", NULL, NULL},
        {IDS_RE_POSIX_PRINT, "[[:^print:]]", NULL, NULL},
        {IDS_RE_POSIX_PUNCT, "[[:^punct:]]", NULL, NULL},
        {IDS_RE_POSIX_SPACE, "[[:^space:]]", NULL, NULL},
        {IDS_RE_POSIX_UPPER, "[[:^upper:]]", NULL, NULL},
        {IDS_RE_POSIX_WORD, "[[:^word:]]", NULL, NULL},
        {IDS_RE_POSIX_XDIGIT, "[[:^xdigit:]]", NULL, NULL},
        {IDS_RE_DEFINESET, NULL, GetRENegativeSet, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM ZeroWidthAssertions[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_BOL
  {MNTT_IT, IDS_RE_EOL
  {MNTT_IT, IDS_RE_WORDB
  {MNTT_IT, IDS_RE_NONWORDB
  {MNTT_IT, IDS_RE_PLOOKA
  {MNTT_IT, IDS_RE_NLOOKA
  {MNTT_IT, IDS_RE_PLOOKB
  {MNTT_IT, IDS_RE_NLOOKB
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem ZeroWidthAssertions[] =
    {
        {IDS_RE_BOL, "^", NULL, NULL},
        {IDS_RE_EOL, "$", NULL, NULL},
        {IDS_RE_WORDB, "\\b", NULL, NULL},
        {IDS_RE_NONWORDB, "\\B", NULL, NULL},
        {IDS_RE_PLOOKA, "(?=)", NULL, NULL},
        {IDS_RE_NLOOKA, "(?!)", NULL, NULL},
        {IDS_RE_PLOOKB, "(?<=)", NULL, NULL},
        {IDS_RE_NLOOKB, "(?<!)", NULL, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM IterationHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_ITERATION_GREEDY
  {MNTT_IT, IDS_RE_ITERATION_UNGREEDY
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem IterationHelpMenu[] =
    {
        {IDS_RE_ITERATION_GREEDY, "*", NULL, NULL},
        {IDS_RE_ITERATION_UNGREEDY, "*?", NULL, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM PositiveIterationHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_ITERATION_GREEDY
  {MNTT_IT, IDS_RE_ITERATION_UNGREEDY
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem PositiveIterationHelpMenu[] =
    {
        {IDS_RE_ITERATION_GREEDY, "+", NULL, NULL},
        {IDS_RE_ITERATION_UNGREEDY, "+?", NULL, NULL},
        {0, NULL, NULL, NULL}};

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM RegExpHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_ANYCHAR
  {MNTT_IT, IDS_RE_ANYFROM
  {MNTT_IT, IDS_RE_ANYBUT
  {MNTT_IT, IDS_RE_ZEROWIDTHASSERT
  {MNTT_IT, IDS_RE_OR
  {MNTT_IT, IDS_RE_ITERATION
  {MNTT_IT, IDS_RE_POSITIVEITERATION
  {MNTT_IT, IDS_RE_ONEORZERO
  {MNTT_IT, IDS_RE_ZEROORONE
  {MNTT_IT, IDS_RE_SUBEXPRESSION
  {MNTT_IT, IDS_RE_NOTCAPTURESUBEXP
  {MNTT_IT, IDS_RE_LITERALS
  {MNTT_IT, IDS_RE_WORDCHAR
  {MNTT_IT, IDS_RE_NONWORDCHAR
  {MNTT_IT, IDS_RE_SPACE
  {MNTT_IT, IDS_RE_NONSPACE
  {MNTT_IT, IDS_RE_DIGIT
  {MNTT_IT, IDS_RE_NONDIGIT
  {MNTT_IT, IDS_RE_NUMBER
  {MNTT_IT, IDS_RE_HEXNUMBER
  {MNTT_IT, IDS_RE_WORD
  {MNTT_IT, IDS_RE_PATHCOMPONENT
  {MNTT_IT, IDS_RE_FILENAME
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem RegExpHelpMenu[] =
    {
        {IDS_RE_ANYCHAR, ".", NULL, NULL},
        {IDS_RE_ANYFROM, NULL, NULL, AnyFromHelpMenu},
        {IDS_RE_ANYBUT, NULL, NULL, AnyButHelpMenu},
        {IDS_RE_ZEROWIDTHASSERT, NULL, NULL, ZeroWidthAssertions},
        {IDS_RE_OR, "|", NULL, NULL},
        {IDS_RE_ITERATION, NULL, NULL, IterationHelpMenu},
        {IDS_RE_POSITIVEITERATION, NULL, NULL, PositiveIterationHelpMenu},
        {IDS_RE_ONEORZERO, "?", NULL, NULL},
        {IDS_RE_ZEROORONE, "??", NULL, NULL},
        {IDS_RE_SUBEXPRESSION, "()", NULL, NULL},
        {IDS_RE_NOTCAPTURESUBEXP, "(?:)", NULL, NULL},
        {-1, NULL, NULL, NULL},
        {IDS_RE_LITERALS, NULL, NULL, LiteralsHelpMenu},
        {IDS_RE_WORDCHAR, "\\w", NULL, NULL},
        {IDS_RE_NONWORDCHAR, "\\W", NULL, NULL},
        {IDS_RE_SPACE, "\\s", NULL, NULL},
        {IDS_RE_NONSPACE, "\\S", NULL, NULL},
        {IDS_RE_DIGIT, "\\d", NULL, NULL},
        {IDS_RE_NONDIGIT, "\\D", NULL, NULL},
        {-1, NULL, NULL, NULL},
        {IDS_RE_NUMBER, "\\d+", NULL, NULL},
        {IDS_RE_HEXNUMBER, "[:xdigit:]+", NULL, NULL},
        {IDS_RE_WORD, "\\w+", NULL, NULL},
        {IDS_RE_PATHCOMPONENT, "[^\\\\]+", NULL, NULL},
        {IDS_RE_FILENAME, "([^\\\\]+?)(?:\\.([^\\\\.]+))?$", NULL, NULL},
        {0, NULL, NULL, NULL}};

BOOL GetRESubExp(HWND parent, char* buffer);

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM ReplaceHelpMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RE_DOLLAR
  {MNTT_IT, IDS_RE_MATCHEDSUBEXP
  {MNTT_PE, 0
};
*/
CVarStrHelpMenuItem ReplaceHelpMenu[] =
    {
        {IDS_RE_DOLLAR, "$$", NULL, NULL},
        {IDS_RE_MATCHEDSUBEXP, NULL, GetRESubExp, NULL},
        {0, NULL, NULL, NULL}};

BOOL GetCounterParamFormat(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return CAddCounterDialog(parent, buffer).Execute() == IDOK;
}

BOOL GetTimeParamFormat(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, buffer, 1024);
}

BOOL GetDateParamFormat(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, buffer, 1024);
}

BOOL GetLowerCaseParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(buffer, "lower");
    return TRUE;
}

BOOL GetUpperCaseParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(buffer, "upper");
    return TRUE;
}

BOOL GetMixedCaseParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(buffer, "mixed");
    return TRUE;
}

BOOL GetStripDiaParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(buffer, "stripdia");
    return TRUE;
}

BOOL GetCutParamAux(HWND parent, char* buffer, const char* casePar)
{
    CALL_STACK_MESSAGE_NONE
    const char* from = VarOriginalName;
    int left = 0, leftSign = 1, right = 0, rightSign = -1;
    if (CAddCutDialog(parent, &from,
                      &left, &leftSign,
                      &right, &rightSign)
            .Execute() == IDOK)
    {
        SalPrintf(buffer, 1024, "%s:%s%s%d,%s%d",
                  from, casePar,
                  leftSign < 0 ? "-" : "", left,
                  rightSign < 0 ? "-" : "", right);
        return TRUE;
    }
    return FALSE;
}

BOOL GetLowerCaseCutParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return GetCutParamAux(parent, buffer, "lower,");
}

BOOL GetUpperCaseCutParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return GetCutParamAux(parent, buffer, "upper,");
}

BOOL GetMixedCaseCutParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return GetCutParamAux(parent, buffer, "mixed,");
}

BOOL GetStripDiaCutParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return GetCutParamAux(parent, buffer, "stripdia,");
}

BOOL GetCutParam(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    return GetCutParamAux(parent, buffer, "");
}

BOOL GetREPositiveSet(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(buffer, "[");
    if (CDefineClassDialog(parent, buffer + 1).Execute() == IDOK)
    {
        strcat(buffer, "]");
        return TRUE;
    }
    return FALSE;
}

BOOL GetRENegativeSet(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(buffer, "[^");
    if (CDefineClassDialog(parent, buffer + 2).Execute() == IDOK)
    {
        strcat(buffer, "]");
        return TRUE;
    }
    return FALSE;
}

BOOL GetRESubExp(HWND parent, char* buffer)
{
    CALL_STACK_MESSAGE_NONE
    int i = 0;
    CChangeCase changeCase = ccDontChange;
    if (CSubexpressionDialog(parent, &i, &changeCase).Execute() == IDOK)
    {
        const char* caseStr = "";
        switch (changeCase)
        {
        case ccLower:
            caseStr = ":lower";
            break;
        case ccUpper:
            caseStr = ":upper";
            break;
        case ccMixed:
            caseStr = ":mixed";
            break;
        case ccStripDia:
            caseStr = ":stripdia";
            break;
        }
        sprintf(buffer, "$(%d%s)", i, caseStr);
        return TRUE;
    }
    return FALSE;
}

CRenamerDialog::CRenamerDialog(HWND parent)
    : CDialog(HLanguage, IDD_RENAME, IDD_RENAME, parent),
      RenamedFiles(32, 64),
      NotRenamedFiles(32, 64),
      SourceFiles(32, 64),
      UndoStack(1, 64)
{
    CALL_STACK_MESSAGE1("CRenamerDialog::CRenamerDialog()");
    ZeroOnDestroy = NULL;
    Preview = NULL;
    MainMenu = NULL;
    MenuBar = NULL;

    ManualModeHFont = NULL;
    FocusHWnd = NULL;
    SortBy = -1;
    TransferDontSaveHistory = FALSE;

    SalMaskGroup = SG->AllocSalamanderMaskGroup();

    if (!SG->GetFilterFromPanel(PANEL_SOURCE, DefMask, MAX_GROUPMASK))
        strcpy(DefMask, "*.*");
    strcpy(Mask, DefMask);
    Subdirs = FALSE;
    ProcessRenamed = FALSE;
    ProcessNotRenamed = TRUE;
    RemoveSourcePath = FALSE;
    ManualMode = FALSE;

    SourceFilesValid = FALSE;
    SourceFilesNeedUpdate = TRUE;
    LastUpdateTime = 0;
    WaitCursor = NULL;
    CloseOnEnable = FALSE;

    Undoing = FALSE;

    // nacteme selection z panelu
    LoadSelection();
}

CRenamerDialog::~CRenamerDialog()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::~CRenamerDialog()");
    SG->FreeSalamanderMaskGroup(SalMaskGroup);
}

BOOL CRenamerDialog::Init()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::Init()");
    // priradim oknu ikonku
    SendMessage(HWindow, WM_SETICON, ICON_BIG,
                (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_RENAMER)));

    // pripojime nasi tridu na kombace
    MaskEdit = new CComboboxEdit();
    NewName = new CComboboxEdit(TRUE);
    SearchFor = new CComboboxEdit(TRUE);
    ReplaceWith = new CComboboxEdit(TRUE);
    ManualEdit = new CNotifyEdit();
    if (!NewName || !SearchFor || !ReplaceWith || !ManualEdit)
        return FALSE;
    MaskEdit->AttachToWindow(GetWindow(GetDlgItem(HWindow, IDC_MASK), GW_CHILD));
    NewName->AttachToWindow(GetWindow(GetDlgItem(HWindow, IDC_NEWNAME), GW_CHILD));
    SearchFor->AttachToWindow(GetWindow(GetDlgItem(HWindow, IDC_SEARCH), GW_CHILD));
    ReplaceWith->AttachToWindow(GetWindow(GetDlgItem(HWindow, IDC_REPLACE), GW_CHILD));
    ManualEdit->AttachToWindow(GetDlgItem(HWindow, IDE_MANUAL));

    // konstrukce listview
    Preview = new CPreviewWindow(this);
    if (Preview == NULL)
    {
        TRACE_E("Low memory");
        return FALSE;
    }

    Preview->AttachToControl(HWindow, IDL_PREVIEW);
    ListView_SetExtendedListViewStyle(Preview->HWindow, LVS_EX_FULLROWSELECT);

    // vytvorime menu
    MainMenu = SalGUI->CreateMenuPopup();
    if (MainMenu == NULL)
        return FALSE;
    MainMenu->LoadFromTemplate(HLanguage, MenuTemplate, NULL, NULL, NULL);

    MenuBar = SalGUI->CreateMenuBar(MainMenu, HWindow);
    if (MenuBar == NULL)
        return FALSE;
    MenuBar->CreateWnd(HWindow);
    SetWindowPos(MenuBar->GetHWND(), NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);

    // nactu paramatry pro layoutovani okna
    GetLayoutParams();

    if (DialogWidth != -1 && DialogHeight != -1)
    {
        SetWindowPos(HWindow, NULL, 0, 0, DialogWidth, DialogHeight, SWP_NOMOVE | SWP_NOZORDER);
        if (Maximized)
            ShowWindow(HWindow, SW_MAXIMIZE);
    }

    LayoutControls();
    ShowControls();
    Preview->InitColumns();
    Preview->SetItemCount(0, 0, 0);

    // attachnu SalGUI kontroly na buttony
    SalGUI->AttachButton(HWindow, IDOK, BTF_DROPDOWN);
    SalGUI->ChangeToArrowButton(HWindow, IDB_NEWNAME);
    SalGUI->ChangeToArrowButton(HWindow, IDB_SEARCH);
    SalGUI->ChangeToArrowButton(HWindow, IDB_REPLACE);

    UpdateManualModeFont();
    SendMessage(ManualEdit->HWindow, EM_LIMITTEXT, 0, 0);
    SendMessage(ManualEdit->HWindow, EM_FMTLINES, FALSE, 0);

    SG->MultiMonCenterWindow(HWindow, SG->GetMainWindowHWND(), FALSE);

    TransferData(ttDataToWindow);

    ReloadSourceFiles();
    return TRUE;
}

void CRenamerDialog::Destroy()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::Destroy()");
    // ulozime si rozmery okna
    WINDOWPLACEMENT wndpl;
    wndpl.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(HWindow, &wndpl))
    {
        DialogWidth = wndpl.rcNormalPosition.right - wndpl.rcNormalPosition.left;
        DialogHeight = wndpl.rcNormalPosition.bottom - wndpl.rcNormalPosition.top;
        Maximized = wndpl.showCmd == SW_SHOWMAXIMIZED;
    }
    if (MenuBar)
    {
        SalGUI->DestroyMenuBar(MenuBar);
        MenuBar = NULL;
    }
    if (MainMenu)
    {
        SalGUI->DestroyMenuPopup(MainMenu);
        MainMenu = NULL;
    }

    // uvolnime handle, jinak by ho ListView vzalo s sebou do pekel
    if (Preview)
        ListView_SetImageList(Preview->HWindow, NULL, LVSIL_SMALL);

    // uvolnime data nez se zavre okno, jinak by se mohlo stat, ze by nas
    // thread salamander sestrelil driv nez zkonci
    RenamedFiles.DestroyMembers();
    NotRenamedFiles.DestroyMembers();
    SourceFiles.DestroyMembers();
    if (Preview)
        Preview->SetItemCount(0, 0, 0);

    if (ZeroOnDestroy != NULL)
        *ZeroOnDestroy = NULL;
}

void CRenamerDialog::GetDlgItemWindowRect(HWND wnd, LPRECT rect)
{
    CALL_STACK_MESSAGE_NONE
    GetWindowRect(wnd, rect);
    POINT p;
    p.x = rect->left;
    p.y = rect->top;
    ScreenToClient(HWindow, &p);
    rect->left = p.x;
    rect->top = p.y;
    p.x = rect->right;
    p.y = rect->bottom;
    ScreenToClient(HWindow, &p);
    rect->right = p.x;
    rect->bottom = p.y;
}

void CRenamerDialog::GetDlgItemWindowRect(int id, LPRECT rect)
{
    CALL_STACK_MESSAGE_NONE
    GetDlgItemWindowRect(GetDlgItem(HWindow, id), rect);
}

BOOL CALLBACK
CRenamerDialog::MoveControls(HWND hwnd, LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CRenamerDialog::MoveControls(, 0x%IX)", lParam);
    CRenamerDialog* rd = (CRenamerDialog*)lParam;
    if (hwnd != rd->MenuBar->GetHWND() && GetParent(hwnd) == rd->HWindow)
    {
        RECT r;
        rd->GetDlgItemWindowRect(hwnd, &r);
        SetWindowPos(hwnd, NULL, r.left, r.top + rd->MenuDY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }
    return TRUE;
}

void CRenamerDialog::GetLayoutParams()
{
    CALL_STACK_MESSAGE1("CFindDialog::GetLayoutParams()");
    RECT wr;
    GetWindowRect(HWindow, &wr);
    MinDlgW = wr.right - wr.left;
    MinDlgH = wr.bottom - wr.top;

    MenuDY = MenuBar->GetNeededHeight();
    RECT cr;
    GetClientRect(HWindow, &cr);

    // posuneme vsechny kontroly v okne o vysku menu
    EnumChildWindows(HWindow, MoveControls, (LPARAM)this);

    RECT r;
    GetDlgItemWindowRect(IDOK, &r);
    RenameRX = cr.right - r.left;
    RenameY = r.top;

    GetDlgItemWindowRect(IDC_NEWNAME, &r);
    CombosLX = r.left;
    CombosRX = cr.right - r.right - 1;
    CombosDY = 0; // napocitame pozdeji, tady je zapocitana i velikost dropdown

    GetDlgItemWindowRect(IDB_NEWNAME, &r);
    ArrowRX = cr.right - r.left;
    NameArrowY = r.top;

    GetDlgItemWindowRect(IDB_SEARCH, &r);
    SearchArrowY = r.top;

    GetDlgItemWindowRect(IDB_REPLACE, &r);
    ReplaceArrowY = r.top;

    GetDlgItemWindowRect(IDS_MANSEP, &r);
    NameRuleLX = r.left;
    RuleRX = cr.right - r.right - 1;
    RuleDY = r.bottom - r.top;

    GetDlgItemWindowRect(IDS_REPLACESEP, &r);
    ReplaceRuleLX = r.left;

    GetDlgItemWindowRect(IDS_CHANGECASESEP, &r);
    CaseRuleLX = r.left;

    GetDlgItemWindowRect(IDE_MANUAL, &r);
    ManualLX = r.left;
    ManualDY = r.bottom - r.top;

    GetDlgItemWindowRect(IDS_COUNT, &r);
    CountRX = cr.right - r.left;
    CountY = r.top;

    GetDlgItemWindowRect(IDL_PREVIEW, &r);
    PreviewY = r.top;
}

void CRenamerDialog::LayoutControls()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::LayoutControls()");
    if (CombosDY == 0)
    {
        RECT r;
        GetWindowRect(GetDlgItem(HWindow, IDC_NEWNAME), &r);
        CombosDY = r.bottom - r.top;
    }

    RECT cr;
    GetClientRect(HWindow, &cr);

    HDWP hdwp = BeginDeferWindowPos(15);
    if (hdwp != NULL)
    {
        // natahnu menu
        hdwp = DeferWindowPos(hdwp, MenuBar->GetHWND(), NULL, 0, 0, cr.right, MenuDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // natahnu menu
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDS_MENUSEP), NULL, 0, 0, cr.right, RuleDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // umistim tlacitko Rename
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDOK), NULL,
                              cr.right - RenameRX, RenameY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // natahnu rule Manual Mode
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDS_MANSEP), NULL,
                              0, 0, cr.right - NameRuleLX - RuleRX, RuleDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // natahnu combobox New Name
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_NEWNAME), NULL,
                              0, 0, cr.right - CombosLX - CombosRX, CombosDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // umistim arrow button New Name
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_NEWNAME), NULL,
                              cr.right - ArrowRX, NameArrowY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // natahnu rule Replace
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDS_REPLACESEP), NULL,
                              0, 0, cr.right - ReplaceRuleLX - RuleRX, RuleDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // natahnu combobox Search For
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_SEARCH), NULL,
                              0, 0, cr.right - CombosLX - CombosRX, CombosDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // umistim arrow button Search For
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_SEARCH), NULL,
                              cr.right - ArrowRX, SearchArrowY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // natahnu combobox Reaplace
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDC_REPLACE), NULL,
                              0, 0, cr.right - CombosLX - CombosRX, CombosDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // umistim arrow button Replace
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDB_REPLACE), NULL,
                              cr.right - ArrowRX, ReplaceArrowY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // natahnu rule Case
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDS_CHANGECASESEP), NULL,
                              0, 0, cr.right - CaseRuleLX - RuleRX, RuleDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // umistim text Count
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDS_COUNT), NULL,
                              cr.right - CountRX, CountY, 0, 0,
                              SWP_NOZORDER | SWP_NOSIZE);

        // natahnu manual edit control
        hdwp = DeferWindowPos(hdwp, ManualEdit->HWindow, NULL,
                              0, 0, cr.right - ManualLX - RuleRX, ManualDY,
                              SWP_NOZORDER | SWP_NOMOVE);

        // natahnu list view
        hdwp = DeferWindowPos(hdwp, GetDlgItem(HWindow, IDL_PREVIEW), NULL,
                              0, 0, cr.right, cr.bottom - PreviewY,
                              SWP_NOZORDER | SWP_NOMOVE);

        EndDeferWindowPos(hdwp);
    }
}

void CRenamerDialog::UpdateMenuItems()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::UpdateMenuItems()");
    CGUIMenuPopupAbstract* subMenu = MainMenu->GetSubMenu(CMD_FILEPOPUP, FALSE);
    if (subMenu)
    {
        subMenu->EnableItem(CMD_UNDO, FALSE, UndoStack.Count);
    }

    subMenu = MainMenu->GetSubMenu(CMD_EDITPOPUP, FALSE);
    if (subMenu)
    {
        subMenu->EnableItem(CMD_STORELIST, FALSE, ManualMode);
        subMenu->EnableItem(CMD_LOADLIST, FALSE, ManualMode);
        subMenu->EnableItem(CMD_EDIT, FALSE, ManualMode);
        subMenu->EnableItem(CMD_FILTER, FALSE, ManualMode);
    }

    subMenu = MainMenu->GetSubMenu(CMD_VIEWPOPUP, FALSE);
    if (subMenu)
    {
        subMenu->CheckRadioItem(CMD_SORTOLD, CMD_SORTPATH, Preview->GetSortCommand(SortBy), FALSE);
        int cmd = CMD_ALLFILES;
        if (ProcessRenamed && !ProcessNotRenamed)
            cmd = CMD_RENAMED;
        if (!ProcessRenamed && ProcessNotRenamed)
            cmd = CMD_NOTRENAMED;
        subMenu->CheckRadioItem(CMD_ALLFILES, CMD_NOTRENAMED, cmd, FALSE);
    }

    subMenu = MainMenu->GetSubMenu(CMD_OPTIONSPOPUP, FALSE);
    if (subMenu)
    {
        if (RootLen == 0)
            subMenu->EnableItem(CMD_RELATIVEPATH, FALSE, FALSE);
        subMenu->CheckRadioItem(CMD_FILENAMES, CMD_FULLPATH,
                                CMD_FILENAMES + RenamerOptions.Spec, FALSE);
        subMenu->CheckItem(CMD_RMSOURCEPATH, FALSE, RemoveSourcePath);
    }

    // subMenu = subMenu->GetSubMenu(CMD_FONTPOPUP, FALSE);
    // if (subMenu)
    //   subMenu->CheckItem(CMD_AUTOFONT, FALSE, !UseCustomFont);
}

void CRenamerDialog::UpdateManualModeFont()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::UpdateManualModeFont()");
    SendMessage(ManualEdit->HWindow, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), !UseCustomFont);
    if (UseCustomFont)
    {
        if (ManualModeHFont)
            DeleteObject(ManualModeHFont);
        ManualModeHFont = CreateFontIndirect(&ManualModeLogFont);
        SendMessage(ManualEdit->HWindow, WM_SETFONT, (WPARAM)ManualModeHFont, UseCustomFont);
    }
}

void CRenamerDialog::ShowControls()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::ShowControls()");
    DWORD show = ManualMode ? SW_HIDE : SW_SHOW;
    DWORD hide = ManualMode ? SW_SHOW : SW_HIDE;

    ShowWindow(GetDlgItem(HWindow, IDS_LABELNEWNAME), show);
    ShowWindow(GetDlgItem(HWindow, IDC_NEWNAME), show);
    ShowWindow(GetDlgItem(HWindow, IDB_NEWNAME), show);

    ShowWindow(GetDlgItem(HWindow, IDS_LABELREPLACE), show);
    ShowWindow(GetDlgItem(HWindow, IDS_REPLACESEP), show);

    ShowWindow(GetDlgItem(HWindow, IDS_LABELSEARCHFOR), show);
    ShowWindow(GetDlgItem(HWindow, IDC_SEARCH), show);
    ShowWindow(GetDlgItem(HWindow, IDB_SEARCH), show);

    ShowWindow(GetDlgItem(HWindow, IDS_LABELREPLACEWITH), show);
    ShowWindow(GetDlgItem(HWindow, IDC_REPLACE), show);
    ShowWindow(GetDlgItem(HWindow, IDB_REPLACE), show);

    ShowWindow(GetDlgItem(HWindow, IDC_CASESENSITIVE), show);
    ShowWindow(GetDlgItem(HWindow, IDC_WHOLEWORDS), show);
    ShowWindow(GetDlgItem(HWindow, IDC_GLOBAL), show);
    ShowWindow(GetDlgItem(HWindow, IDC_REGEXP), show);
    ShowWindow(GetDlgItem(HWindow, IDC_EXCLUDEEXT), show);

    ShowWindow(GetDlgItem(HWindow, IDS_LABELCASE), show);
    ShowWindow(GetDlgItem(HWindow, IDS_CHANGECASESEP), show);
    ShowWindow(GetDlgItem(HWindow, IDS_LABELNAME), show);
    ShowWindow(GetDlgItem(HWindow, IDC_NAMECASE), show);
    ShowWindow(GetDlgItem(HWindow, IDS_LABELEXT), show);
    ShowWindow(GetDlgItem(HWindow, IDC_EXTCASE), show);
    ShowWindow(GetDlgItem(HWindow, IDC_PATHPART), show);

    ShowWindow(GetDlgItem(HWindow, IDE_MANUAL), hide);
}

void CRenamerDialog::SetWait(BOOL wait)
{
    CALL_STACK_MESSAGE_NONE
    if (wait)
        SetCursor(LoadCursor(NULL, IDC_WAIT));
    else
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    WaitCursor = wait;
}

void CRenamerDialog::LoadSelection()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::LoadSelection()");
    int files = 0, dirs = 0;
    SG->GetPanelSelection(PANEL_SOURCE, &files, &dirs);

    SG->GetPanelPath(PANEL_SOURCE, Root, MAX_PATH, NULL, NULL);
    RootLen = (int)strlen(Root);

    // nacteme si selection z panelu
    const CFileData* fd;
    BOOL isDir = FALSE;
    if (files + dirs == 0)
    {
        fd = SG->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
        NotRenamedFiles.Add(new CSourceFile(fd, Root, RootLen, isDir));
    }
    else
    {
        int i = 0;
        while ((fd = SG->GetPanelSelectedItem(PANEL_SOURCE, &i, &isDir)) != NULL)
            NotRenamedFiles.Add(new CSourceFile(fd, Root, RootLen, isDir));
    }
}

void CRenamerDialog::ReloadSourceFiles()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::ReloadSourceFiles()");
    SourceFilesNeedUpdate = FALSE;
    SourceFilesValid = FALSE;
    SourceFiles.DestroyMembers();

    // reknu listview novy pocet polozek
    // Preview->SetItemCount(0, 0, 3);

    char mask[MAX_GROUPMASK];
    BOOL subdirs =
        SendDlgItemMessage(HWindow, IDC_SUBDIRS, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (!GetDlgItemText(HWindow, IDC_MASK, mask, MAX_GROUPMASK))
    {
        Preview->SetItemCount(0, 0, 2);
        SetDlgItemText(HWindow, IDS_COUNT, "");
        return;
    }

    SalMaskGroup->SetMasksString(mask, FALSE);
    int err;
    if (!SalMaskGroup->PrepareMasks(err))
    {
        Preview->SetItemCount(0, 0, 2);
        SetDlgItemText(HWindow, IDS_COUNT, "");
        return;
    }

    if (!FocusHWnd)
        FocusHWnd = GetFocus();

    SetWait(TRUE);

    EnableWindow(HWindow, FALSE);

    BOOL success = TRUE;
    char pathBuf[MAX_PATH];
    strcpy(pathBuf, Root);
    SkipAllLongNames = FALSE;
    SkipAllBadDirs = FALSE;
    TRACE_I("start reading files");
    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState
    int dirs = 0;
    LastUpdateTime = GetTickCount();
    int i, j;
    for (i = j = 0;
         ProcessRenamed && i < RenamedFiles.Count ||
         ProcessNotRenamed && j < NotRenamedFiles.Count;)
    {

        CSourceFile* item =
            ProcessRenamed && i < RenamedFiles.Count ? RenamedFiles[i++] : NotRenamedFiles[j++];
        if (subdirs && item->IsDir)
        {
            success = LoadSubdir(pathBuf, item->Name);
            if (!success)
                break;
        }
        else
        {
            if (SalMaskGroup->AgreeMasks(item->Name, item->IsDir ? NULL : item->Ext))
            {
                SourceFiles.Add(new CSourceFile(item));
                if (item->IsDir)
                    dirs++;
            }
        }
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!IsMenuBarMessage(&msg) && !IsDialogMessage(HWindow, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        if (SourceFilesNeedUpdate)
        {
            success = FALSE;
            break;
        }
    }
    LastUpdateTime = GetTickCount() - LastUpdateTime;
    TRACE_I("finished reading files");

    if (success)
    {
        SourceFilesValid = TRUE;

        // reknu listview novy pocet polozek
        Preview->SetItemCount(SourceFiles.Count, LVSICF_NOSCROLL, 1);

        if (SourceFiles.Count > 0)
        {
            char buf[200];
            SG->ExpandPluralFilesDirs(buf, 200, SourceFiles.Count - dirs, dirs,
                                      epfdmNormal, FALSE);
            SetDlgItemText(HWindow, IDS_COUNT, buf);
        }
        else
            SetDlgItemText(HWindow, IDS_COUNT, "");

        if (ManualMode)
        {
            ManualMode = ReloadManualModeEdit();
            if (!ManualMode)
            {
                SendDlgItemMessage(HWindow, IDC_MANUAL, BM_SETCHECK, BST_UNCHECKED, 0);
                ShowControls();
            }
        }
    }
    else
    {
        SourceFiles.DestroyMembers();
        if (subdirs)
        {
            if (!SourceFilesNeedUpdate)
            {
                // shodime subdirs flag a nacteme SourceFiles bez rekurze
                SendDlgItemMessage(HWindow, IDC_SUBDIRS, BM_SETCHECK, BST_UNCHECKED, 0);
                ReloadSourceFiles();
            }
        }
        else
        {
            Preview->SetItemCount(0, 0, 4);
            SetDlgItemText(HWindow, IDS_COUNT, "");
        }
    }

    SortBy = -1;

    EnableWindow(HWindow, TRUE);
    if (FocusHWnd)
    {
        SetFocus(FocusHWnd);
        FocusHWnd = NULL;
    }

    SetWait(FALSE);
}

BOOL CRenamerDialog::LoadSubdir(char* path, const char* subdir)
{
    CALL_STACK_MESSAGE2("CRenamerDialog::LoadSubdir(, %s)", subdir);
    BOOL b = SG->SalPathAppend(path, subdir, MAX_PATH);
    if (!b || !SG->SalPathAppend(path, "*.*", MAX_PATH))
    {
        if (b)
            SG->CutDirectory(path); // orizneme subdir
        BOOL skip;
        FileError(HWindow, subdir, IDS_TOOLONGPATH, FALSE,
                  &skip, &SkipAllLongNames, IDS_ERROR);
        return skip;
    }

    WIN32_FIND_DATA fd;
    HANDLE hFind;

    while ((hFind = FindFirstFile(path, &fd)) == INVALID_HANDLE_VALUE)
    {
        BOOL skip;
        if (!FileError(HWindow, path, IDS_ERRREADDIR, TRUE,
                       &skip, &SkipAllBadDirs, IDS_ERROR))
        {
            SG->CutDirectory(path); // orizneme *.*
            SG->CutDirectory(path); // orizneme subdir
            return skip;
        }
    }

    SG->CutDirectory(path); // orizneme *.*

    int pathLen = (int)strlen(path);
    BOOL more = TRUE, ret = TRUE;
    do
    {
        // test na ESC
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) &&
            GetForegroundWindow() == HWindow)
        {
            MSG msg; // vyhodime nabufferovany ESC
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;
            if (SG->SalMessageBox(HWindow, LoadStr(IDS_CANCELCNFRM),
                                  LoadStr(IDS_QUESTION), MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
            {
                ret = FALSE;
                break;
            }
            while (GetAsyncKeyState(VK_ESCAPE) & 0x8001)
                Sleep(1);
        }

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!IsMenuBarMessage(&msg) && !IsDialogMessage(HWindow, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (SourceFilesNeedUpdate)
        {
            ret = FALSE;
            break;
        }

        if (GetTickCount() - LastUpdateTime > 2000)
        {
            Preview->SetItemCount(0, 0, 3);
        }

        if (fd.cFileName[0] != 0 && strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!LoadSubdir(path, fd.cFileName))
                {
                    ret = FALSE;
                    break;
                }
            }
            else
            {
                char* ext = _tcsrchr(fd.cFileName, '.');
                if (!ext)
                    ext = fd.cFileName + strlen(fd.cFileName); // ".cvspass" ve Windows je pripona
                else
                    ext++;

                if (SalMaskGroup->AgreeMasks(fd.cFileName, ext))
                {
                    CSourceFile* item = new CSourceFile(fd, path, pathLen);
                    if (item->NameLen < MAX_PATH)
                        SourceFiles.Add(item);
                    else
                    {
                        BOOL skip;
                        FileError(HWindow, item->FullName, IDS_TOOLONGPATH, FALSE,
                                  &skip, &SkipAllLongNames, IDS_ERROR);
                        delete item;
                        if (!skip)
                        {
                            ret = FALSE;
                            break;
                        }
                    }
                }
            }
        }

        while (!FindNextFile(hFind, &fd))
        {
            if (GetLastError() == ERROR_NO_MORE_FILES ||
                !FileError(HWindow, path, IDS_ERRREADDIR, TRUE,
                           &ret, &SkipAllBadDirs, IDS_ERROR))
            {
                more = FALSE;
                break;
            }
        }
    } while (more);

    FindClose(hFind);
    SG->CutDirectory(path); // orizneme subdir

    return ret;
}

BOOL CRenamerDialog::ReloadManualModeEdit()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::ReloadManualModeEdit()");
    if (!SourceFilesValid)
        return FALSE;

    TBuffer<char> buf(TRUE);
    int size = 0;
    char* name;
    int l;
    int i;
    for (i = 0; i < SourceFiles.Count; i++)
    {
        switch (RenamerOptions.Spec)
        {
        case rsFileName:
            name = SourceFiles[i]->Name;
            break;
        case rsRelativePath:
            name = StripRoot(SourceFiles[i]->FullName, RootLen);
            break;
        case rsFullPath:
            name = SourceFiles[i]->FullName;
            break;
        }
        l = (int)strlen(name);
        if (!buf.Reserve(size + l + 2))
            return Error(IDS_LOWMEM);
        memcpy(buf.Get() + size, name, l);
        buf.Get()[size + l] = '\r';
        buf.Get()[size + l + 1] = '\n';
        size += l + 2;
    }
    if (!buf.Reserve(size + 1))
        return Error(IDS_LOWMEM);
    buf.Get()[size] = 0;

    SendMessage(ManualEdit->HWindow, WM_SETTEXT, 0, (LPARAM)buf.Get());

    return TRUE;
}

void CRenamerDialog::OnEnterIdle()
{
    CALL_STACK_MESSAGE_NONE
    HWND focus = GetFocus();
    HWND mask = GetDlgItem(HWindow, IDC_MASK);
    if ((!SourceFilesValid || SourceFilesNeedUpdate) &&
        IsWindowEnabled(HWindow)
        /*(LastUpdateTime < 5000 || focus != mask && GetParent(focus) != mask)*/)
        ReloadSourceFiles();
    Preview->Update();
}

BOOL CRenamerDialog::IsMenuBarMessage(CONST MSG* lpMsg)
{
    CALL_STACK_MESSAGE1("CRenamerDialog::IsMenuBarMessage()");
    if (MenuBar == NULL)
        return FALSE;
    return MenuBar->IsMenuBarMessage(lpMsg);
}

void CRenamerDialog::SetOptions(CRenamerOptions& options, char* mask,
                                BOOL subdirs, BOOL removeSourcePath)
{
    CALL_STACK_MESSAGE3("CRenamerDialog::SetOptions(, , %d, %d)", subdirs,
                        removeSourcePath);
    RenamerOptions = options;
    strcpy(Mask, mask);
    Subdirs = subdirs;
    RemoveSourcePath = removeSourcePath;

    if (RootLen <= 0 && RenamerOptions.Spec == rsRelativePath)
        RenamerOptions.Spec = rsFileName;

    TransferData(ttDataToWindow);
    Preview->Update(TRUE);
}

void CRenamerDialog::ResetOptions(BOOL soft)
{
    CALL_STACK_MESSAGE2("CRenamerDialog::ResetOptions(%d)", soft);
    RenamerOptions.Reset(soft);
    if (!soft)
    {
        Subdirs = FALSE;
        RemoveSourcePath = TRUE;
    }
    strcpy(Mask, DefMask);

    TransferData(ttDataToWindow);
    Preview->Update(TRUE);
}

BOOL CRenamerDialog::TransferForPreview()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::TransferForPreview()");
    CTransferInfo ti(HWindow, ttDataFromWindow);

    TransferDontSaveHistory = TRUE;
    Transfer(ti);
    TransferDontSaveHistory = FALSE;

    return ti.IsGood();
}

void CRenamerDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CRenamerDialog::Validate()");
    char mask[MAX_GROUPMASK];
    int err = 0;
    ti.EditLine(IDC_MASK, mask, MAX_GROUPMASK);
    SalMaskGroup->SetMasksString(mask, FALSE);
    if (strlen(mask) <= 0 || !SalMaskGroup->PrepareMasks(err))
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_INVALIDMASK), LoadStr(IDS_ERROR),
                          MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_START);
        MaskEdit->SetSel(err, err);
        return;
    }

    if (ManualMode)
    {
        // ignorujeme prazdne radky na konci
        int lines = (int)SendMessage(ManualEdit->HWindow, EM_GETLINECOUNT, 0, 0);
        while (lines > 0)
        {
            int index = (int)SendMessage(ManualEdit->HWindow, EM_LINEINDEX, lines - 1, 0);
            if (SendMessage(ManualEdit->HWindow, EM_LINELENGTH, index, 0) <= 0)
                lines--;
            else
                break;
        }
        if (lines != SourceFiles.Count)
        {
            SG->SalMessageBox(HWindow, LoadStr(IDS_BADLINECOUNT), LoadStr(IDS_ERROR),
                              MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_MANUAL);
            return;
        }
    }
}

void CRenamerDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CRenamerDialog::Transfer()");
    if (TransferDontSaveHistory)
    {
        ti.EditLine(IDC_MASK, Mask, MAX_GROUPMASK);
        ti.EditLine(IDC_NEWNAME, RenamerOptions.NewName, 2 * MAX_PATH);
        ti.EditLine(IDC_SEARCH, RenamerOptions.SearchFor, 2 * MAX_PATH);
        ti.EditLine(IDC_REPLACE, RenamerOptions.ReplaceWith, MAX_PATH);
    }
    else
    {
        HistoryComboBox(ti, IDC_MASK, Mask, MAX_GROUPMASK, MAX_HISTORY_ENTRIES, MaskHistory);
        HistoryComboBox(ti, IDC_NEWNAME, RenamerOptions.NewName,
                        2 * MAX_PATH, MAX_HISTORY_ENTRIES, NewNameHistory);
        HistoryComboBox(ti, IDC_SEARCH, RenamerOptions.SearchFor,
                        2 * MAX_PATH, MAX_HISTORY_ENTRIES, SearchHistory);
        HistoryComboBox(ti, IDC_REPLACE, RenamerOptions.ReplaceWith,
                        MAX_PATH, MAX_HISTORY_ENTRIES, ReplaceHistory);
    }
    ti.CheckBox(IDC_SUBDIRS, Subdirs);
    ti.CheckBox(IDC_CASESENSITIVE, RenamerOptions.CaseSensitive);
    ti.CheckBox(IDC_WHOLEWORDS, RenamerOptions.WholeWords);
    RenamerOptions.Global = !RenamerOptions.Global; // invertujem hodnotu pro checkbox ti.CheckBox(IDC_GLOBAL, RenamerOptions.Global);
    ti.CheckBox(IDC_GLOBAL, RenamerOptions.Global);
    RenamerOptions.Global = !RenamerOptions.Global; // invertujem hodnotu z check boxu
    ti.CheckBox(IDC_REGEXP, RenamerOptions.RegExp);
    ti.CheckBox(IDC_EXCLUDEEXT, RenamerOptions.ExcludeExt);
    int caseCombos[] =
        {
            ccDontChange, IDS_CASE_DONTCHANGE,
            ccLower, IDS_CASE_LOWER,
            ccUpper, IDS_CASE_UPPER,
            ccMixed, IDS_CASE_MIXED,
            -1, -1};
    int fileCase = (int)RenamerOptions.FileCase;
    TransferCombo(ti, IDC_NAMECASE, caseCombos, fileCase);
    RenamerOptions.FileCase = (CChangeCase)fileCase;
    int extCase = (int)RenamerOptions.ExtCase;
    TransferCombo(ti, IDC_EXTCASE, caseCombos, extCase);
    RenamerOptions.ExtCase = (CChangeCase)extCase;
    ti.CheckBox(IDC_PATHPART, RenamerOptions.IncludePath);
}

INT_PTR
CRenamerDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CRenamerDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                             wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (AlwaysOnTop)
            SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        DialogStackPush(HWindow);
        if (!Init())
            PostMessage(HWindow, WM_CLOSE, 0, 0);
        return TRUE;
    }

    case WM_SIZE:
    {
        LayoutControls();
        break;
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgW;
        lpmmi->ptMinTrackSize.y = MinDlgH;
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case CMD_VALIDATE:
        {
            TransferDontSaveHistory = TRUE;
            BOOL ret = ValidateData() && TransferData(ttDataFromWindow);
            TransferDontSaveHistory = FALSE;
            if (!ret)
                return TRUE;
            Rename(TRUE);
            return TRUE;
        }

        case IDOK:
        {
            if (!ValidateData() || !TransferData(ttDataFromWindow))
                return TRUE;
            // ulozime si options
            LastOptions = RenamerOptions;
            strcpy(LastMask, Mask);
            LastSubdirs = Subdirs;
            LastRemoveSourcePath = RemoveSourcePath;

            Rename(FALSE);
            return TRUE;
        }

        case IDCANCEL:
        {
            MSGBOXEX_PARAMS mbp;
            memset(&mbp, 0, sizeof(mbp));
            mbp.HParent = HWindow;
            mbp.Text = LoadStr(IDS_CNFRM_CLOSE);
            mbp.Caption = LoadStr(IDS_PLUGINNAME);
            mbp.Flags = MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED;
            mbp.CheckBoxText = LoadStr(IDS_CNFRM_ESC_CLOSE2);
            BOOL dontShow = !ConfirmESCClose;
            mbp.CheckBoxValue = &dontShow;

            if (!IsWindowEnabled(HWindow) || ConfirmESCClose && SG->SalMessageBoxEx(&mbp) != IDYES)
            {
                ConfirmESCClose = !dontShow;
                return 0;
            }
            ConfirmESCClose = !dontShow;
            break;
        }

        case CMD_UNDO:
        {
            if (UndoStack.Count &&
                SG->SalMessageBox(HWindow, LoadStr(IDS_UNDOQUEST),
                                  LoadStr(IDS_QUESTION), MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED) == IDYES)
            {
                Undo();
            }
            return TRUE;
        }

        case IDC_MASK:
        {
            switch (HIWORD(wParam))
            {
            case CBN_EDITCHANGE:
            case CBN_SELENDOK:
                SourceFilesNeedUpdate = TRUE;
                break;
            }
            break;
        }

        case IDC_SUBDIRS:
        {
            ReloadSourceFiles();
            break;
        }

        case IDC_MANUAL:
        {
            ManualMode =
                SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (ManualMode)
            {
                ManualMode = ReloadManualModeEdit();
                if (!ManualMode)
                    SendMessage((HWND)lParam, BM_SETCHECK, BST_UNCHECKED, 0);
            }
            ShowControls();
            Preview->SetDirty();
            break;
        }

        case IDC_NEWNAME:
        case IDC_SEARCH:
        case IDC_REPLACE:
        {
            if (HIWORD(wParam) == CBN_EDITCHANGE ||
                HIWORD(wParam) == CBN_SELENDOK)
                Preview->SetDirty();
            break;
        }

        case IDC_NAMECASE:
        case IDC_EXTCASE:
        {
            if (HIWORD(wParam) == CBN_SELENDOK)
                Preview->SetDirty();
            break;
        }

        case IDC_CASESENSITIVE:
        case IDC_WHOLEWORDS:
        case IDC_GLOBAL:
        case IDC_EXCLUDEEXT:
        case IDC_PATHPART:
        {
            Preview->SetDirty();
            break;
        }
        case IDC_REGEXP:
        {
            HWND wholeWords = GetDlgItem(HWindow, IDC_WHOLEWORDS);
            if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                SendMessage(wholeWords, BM_SETCHECK, BST_UNCHECKED, 0);
                EnableWindow(wholeWords, FALSE);
            }
            else
                EnableWindow(wholeWords, TRUE);

            Preview->SetDirty();
            break;
        }

        case IDE_MANUAL:
        {
            if (HIWORD(wParam) == EN_CHANGE)
                Preview->SetDirty();
            break;
        }

        case IDB_NEWNAME:
        {
            RECT r;
            char buffer[1024];
            GetWindowRect((HWND)lParam, &r);
            if (SelectVarStrVariable(HWindow, r.right, r.top, NewNameHelpMenu, buffer, TRUE))
                NewName->ReplaceText(buffer);
            break;
        }

        case IDB_SEARCH:
        {
            RECT r;
            char buffer[1024];
            GetWindowRect((HWND)lParam, &r);
            if (SelectVarStrVariable(HWindow, r.right, r.top, RegExpHelpMenu, buffer, FALSE))
            {
                SendDlgItemMessage(HWindow, IDC_REGEXP, BM_SETCHECK, BST_CHECKED, 0);
                HWND wholeWords = GetDlgItem(HWindow, IDC_WHOLEWORDS);
                SendMessage(wholeWords, BM_SETCHECK, BST_UNCHECKED, 0);
                EnableWindow(wholeWords, FALSE);
                SearchFor->ReplaceText(buffer);
            }
            break;
        }

        case IDB_REPLACE:
        {
            RECT r;
            char buffer[1024];
            GetWindowRect((HWND)lParam, &r);
            if (SelectVarStrVariable(HWindow, r.right, r.top, ReplaceHelpMenu, buffer, FALSE))
            {
                SendDlgItemMessage(HWindow, IDC_REGEXP, BM_SETCHECK, BST_CHECKED, 0);
                HWND wholeWords = GetDlgItem(HWindow, IDC_WHOLEWORDS);
                SendMessage(wholeWords, BM_SETCHECK, BST_UNCHECKED, 0);
                EnableWindow(wholeWords, FALSE);
                ReplaceWith->ReplaceText(buffer);
            }
            break;
        }

        case CMD_STORELIST:
        {
            if (ManualMode)
            {
                DWORD start = 0, end = -1;
                SendMessage(ManualEdit->HWindow, EM_GETSEL, WPARAM(&start), LPARAM(&end));
                SendMessage(ManualEdit->HWindow, EM_SETSEL, 0, -1);
                SendMessage(ManualEdit->HWindow, WM_COPY, 0, 0);
                SendMessage(ManualEdit->HWindow, EM_SETSEL, start, end);
                Preview->Update(TRUE);
                return TRUE;
            }
            break;
        }

        case CMD_LOADLIST:
        {
            if (ManualMode)
            {
                SendMessage(ManualEdit->HWindow, EM_SETSEL, 0, -1);
                SendMessage(ManualEdit->HWindow, WM_PASTE, 0, 0);
                Preview->Update(TRUE);
                SendMessage(HWindow, WM_USER_CARETMOVE, 0, 0);
                return TRUE;
            }
            break;
        }

        case CMD_EDIT:
        {
            if (ManualMode)
            {
                if (ExportToTempFile())
                {
                    if (ExecuteEditor(TempFile) &&
                        SG->SalMessageBox(HWindow, LoadStr(IDS_EXTEDIT),
                                          LoadStr(IDS_PLUGINNAME), MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
                    {
                        ImportFromTempFile();
                        Preview->Update(TRUE);
                        SendMessage(HWindow, WM_USER_CARETMOVE, 0, 0);
                    }
                    SG->CutDirectory(TempFile);
                    SG->RemoveTemporaryDir(TempFile);
                }
                return TRUE;
            }
            break;
        }

        case CMD_FILTER:
        {
            if (ManualMode)
            {
                char command[4096];
                command[0] = 0;
                if (CCommandDialog(HWindow, command).Execute() == IDOK)
                {
                    if (ExecuteCommand(command))
                    {
                        Preview->Update(TRUE);
                        SendMessage(HWindow, WM_USER_CARETMOVE, 0, 0);
                    }
                }
                return TRUE;
            }
            break;
        }

        case CMD_SORTOLD:
        {
            Preview->SortItems(CI_OLDNAME);
            SortBy = CI_OLDNAME;
            break;
        }

        case CMD_SORTSIZE:
        {
            Preview->SortItems(CI_SIZE);
            SortBy = CI_SIZE;
            break;
        }

        case CMD_SORTTIME:
        {
            Preview->SortItems(CI_TIME);
            SortBy = CI_TIME;
            break;
        }

        case CMD_SORTPATH:
        {
            Preview->SortItems(CI_PATH);
            SortBy = CI_PATH;
            break;
        }

        case CMD_ALLFILES:
        {
            ProcessRenamed = ProcessNotRenamed = TRUE;
            ReloadSourceFiles();
            break;
        }

        case CMD_RENAMED:
        {
            ProcessRenamed = TRUE;
            ProcessNotRenamed = FALSE;
            ReloadSourceFiles();
            break;
        }

        case CMD_NOTRENAMED:
        {
            ProcessRenamed = FALSE;
            ProcessNotRenamed = TRUE;
            ReloadSourceFiles();
            break;
        }

        case CMD_FILENAMES:
        {
            RenamerOptions.Spec = rsFileName;
            SortBy = -1;
            Preview->Update(TRUE);
            break;
        }

        case CMD_RELATIVEPATH:
        {
            if (RootLen > 0)
            {
                RenamerOptions.Spec = rsRelativePath;
                SortBy = -1;
                Preview->Update(TRUE);
            }
            break;
        }

        case CMD_FULLPATH:
        {
            RenamerOptions.Spec = rsFullPath;
            SortBy = -1;
            Preview->Update(TRUE);
            break;
        }

        case CMD_RMSOURCEPATH:
        {
            RemoveSourcePath = !RemoveSourcePath;
            break;
        }

            // case CMD_FONT:
            // {
            //   CHOOSEFONT cf;
            //   LOGFONT logFont = ManualModeLogFont;
            //   memset(&cf, 0, sizeof(CHOOSEFONT));
            //   cf.lStructSize = sizeof(CHOOSEFONT);
            //   cf.hwndOwner = HWindow;
            //   cf.hDC = NULL;
            //   cf.lpLogFont = &logFont;
            //   cf.iPointSize = 10;
            //   cf.Flags = CF_SCREENFONTS | CF_ENABLEHOOK | CF_FORCEFONTEXIST;
            //   if (UseCustomFont)
            //     cf.Flags |= CF_INITTOLOGFONTSTRUCT;
            //   cf.lpfnHook = ComDlgHookProc;
            //   if (ChooseFont(&cf) != 0)
            //   {
            //     ManualModeLogFont = logFont;
            //     UseCustomFont = TRUE;
            //     UpdateManualModeFont();
            //   }
            //   break;
            // }

            // case CMD_AUTOFONT:
            // {
            //   UseCustomFont = FALSE;
            //   UpdateManualModeFont();
            //   break;
            // }

        case CMD_LAST:
        {
            SetOptions(LastOptions, LastMask, LastSubdirs, LastRemoveSourcePath);
            break;
        }

        case CMD_RESET:
        {
            ResetOptions(FALSE);
            break;
        }

        case CMD_ADVANCED:
        {
            OnConfiguration(HWindow);
            break;
        }

        case CMD_HELPREGEXP:
        {
            SG->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_REGEXP, FALSE);
            break;
        }

        case CMD_FOCUS:
        {
            break;
        }
        }

        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDL_PREVIEW)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case NM_DBLCLK:
            {
                PostMessage(HWindow, WM_COMMAND, CMD_FOCUS, 0);
                break;
            }

            case LVN_COLUMNCLICK:
            {
                int cmd = Preview->GetSortCommand(((NM_LISTVIEW*)lParam)->iSubItem);
                if (cmd)
                    PostMessage(HWindow, WM_COMMAND, cmd, 0);
                break;
            }

            case LVN_GETDISPINFO:
            {
                Preview->GetDispInfo((LV_DISPINFO*)lParam);
                break;
            }

            case NM_CUSTOMDRAW:
            {
                LRESULT result;
                if (Preview->CustomDraw((LPNMLVCUSTOMDRAW)lParam, result))
                {
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, result);
                    return TRUE;
                }
                break;
            }

            case LVN_KEYDOWN:
            {
                NMLVKEYDOWN* kd = (NMLVKEYDOWN*)lParam;
                BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                DWORD cmd = 0;
                switch (kd->wVKey)
                {
                case VK_RETURN:
                {
                    // if (!controlPressed && !altPressed && !shiftPressed) cmd = CMD_FOCUS;
                    break;
                }

                case VK_SPACE:
                {
                    if (!controlPressed && !altPressed && !shiftPressed)
                        cmd = CMD_FOCUS;
                    break;
                }

                case VK_INSERT:
                {
                    // if (!controlPressed && altPressed && !shiftPressed) cmd = CM_COPYFULLNAME;
                    // if (!controlPressed && altPressed && shiftPressed) cmd = CM_COPYFILENAME;
                    // if (controlPressed && altPressed && !shiftPressed) cmd = CM_COPYFULLPATH;
                    break;
                }

                case VK_F10:
                    if (!shiftPressed)
                        break;
                case VK_APPS:
                {
                    // POINT pt;
                    // Preview->GetContextMenuPos(&pt);
                    // OnCtxMenu(pt.x, pt.y);
                    break;
                }
                }
                if (cmd != 0)
                    PostMessage(HWindow, WM_COMMAND, cmd, 0);
            }
            }
        }
        break;
    }

    case WM_SETCURSOR:
    {
        if (WaitCursor)
        {
            SetCursor(LoadCursor(NULL, IDC_WAIT));
            return TRUE;
        }
        break;
    }

    case WM_USER_ENTERMENULOOP:
    {
        TRACE_I("enter menu");
        UpdateMenuItems();
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        TRACE_I("WM_USER_BUTTONDROPDOWN");
        CGUIMenuPopupAbstract* popup = SalGUI->CreateMenuPopup();
        if (popup != NULL)
        {
            RECT r;
            GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);

            popup->LoadFromTemplate(HLanguage, RenameButtonMenuTemplate, NULL, NULL, NULL);
            popup->EnableItem(CMD_UNDO, FALSE, UndoStack.Count);

            DWORD flags = MENU_TRACK_RIGHTBUTTON;
            if (LOWORD(lParam))
            {
                popup->SetSelectedItemIndex(0);
                flags |= MENU_TRACK_SELECT;
            }

            popup->Track(flags, r.left, r.bottom, HWindow, &r);

            SalGUI->DestroyMenuPopup(popup);
        }
        return 0;
    }

    case WM_USER_CARETMOVE:
    {
        int i = (int)SendMessage(ManualEdit->HWindow, EM_LINEINDEX, -1, 0);
        i = (int)SendMessage(ManualEdit->HWindow, EM_LINEFROMCHAR, i, 0);
        TRACE_I("I = " << i);
        int count = ListView_GetItemCount(Preview->HWindow);
        if (i >= count)
        {
            i = count - 1;
            // jsme za poctem polozek v listview, zrusime selection
            ListView_SetItemState(Preview->HWindow, -1, 0,
                                  LVIS_FOCUSED | LVIS_SELECTED);
        }
        else
            ListView_SetItemState(Preview->HWindow, i,
                                  LVIS_FOCUSED | LVIS_SELECTED,
                                  LVIS_FOCUSED | LVIS_SELECTED);
        ListView_EnsureVisible(Preview->HWindow, i, FALSE);
        return 0;
    }

    case WM_USER_CFGCHNG:
    {
        UpdateManualModeFont();
        return 0;
    }

    case WM_USER_SETTINGCHANGE:
    {
        if (MenuBar != NULL)
            MenuBar->SetFont();
        return 0;
    }

    case WM_TIMER:
    {
        if (wParam == 666) // mame si poslat WM_CLOSE, blokujici dialog uz je snad sestreleny
        {
            KillTimer(HWindow, 666);
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
    {
        if (SG->IsCriticalShutdown() && !IsWindowEnabled(HWindow))
        { // zavreme postupne vsechny dialogy nad timto dialogem (posleme jim WM_CLOSE a pak ho posleme znovu sem)
            SG->CloseAllOwnedEnabledDialogs(HWindow);
            SetTimer(HWindow, 666, 100, NULL);
            return TRUE;
        }

        if (!IsWindowEnabled(HWindow))
            //SetForegroundWindow(HWindow);
            FlashWindow(HWindow, TRUE);
        else
            DestroyWindow(HWindow);
        return TRUE;
    }

    case WM_DESTROY:
    {
        Destroy();
        DialogStackPop();
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CRenamerDialog::NotifDlgJustCreated()
{
    SalGUI->ArrangeHorizontalLines(HWindow);
}

//*********************************************************************************
//
// CRenamerDialogThread
//

CRenamerDialogThread::CRenamerDialogThread()
    : CThread("CRenamerDialogThread")
{
    CALL_STACK_MESSAGE1("CRenamerDialogThread::CRenamerDialogThread()");
    Dialog = new CRenamerDialog(NULL);
    if (!Dialog)
        Error(IDS_LOWMEM);
}

HANDLE
CRenamerDialogThread::Create(CThreadQueue& queue)
{
    CALL_STACK_MESSAGE1("CRenamerDialogThread::Create()");
    if (!Dialog)
        return NULL;

    return CThread::Create(queue);
}

unsigned
CRenamerDialogThread::Body()
{
    CALL_STACK_MESSAGE1("CRenamerDialogThread::Body()");
    // SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    // uz nechcem, aby se Dialog deallokoval v destructoru
    CRenamerDialog* dlg = Dialog;
    Dialog = NULL;

    HWND wnd = dlg->Create();
    if (!wnd)
    {
        TRACE_E("Nepodarilo se vytvorit CRenamerDialog");
        delete dlg;
        return 0;
    }
    dlg->SetZeroOnDestroy(&dlg); // pri WM_DESTROY bude ukazatel vynulovan
                                 // obrana pred pristupem na neplatny ukazatel
                                 // z message loopy po destrukci okna

    SetForegroundWindow(wnd);

    if (!WindowQueue.Add(new CWindowQueueItem(wnd)))
    {
        TRACE_E("Low memory");
        DestroyWindow(wnd);
    }

    MSG msg;
    while (IsWindow(wnd))
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break; // ekvivalent situace, kdy GetMessage() vraci FALSE

            if (!dlg->IsMenuBarMessage(&msg) &&
                !TranslateAccelerator(wnd, HAccels, &msg) &&
                !IsDialogMessage(wnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (MsgWaitForMultipleObjects(0, NULL, FALSE, 50,
                                          QS_ALLINPUT | QS_ALLPOSTMESSAGE) == WAIT_TIMEOUT)
            {
                if (dlg != NULL)
                    dlg->OnEnterIdle();
            }
        }
    }

    WindowQueue.Remove(wnd);

    return 0;
}
