// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "plugins.h"
#include "usermenu.h"
#include "execute.h"
#include "cfgdlg.h"
#include "shellib.h"

//******************************************************************************
//
// CComboboxEdit
//

CComboboxEdit::CComboboxEdit()
    : CWindow(ooAllocated)
{
    SelStart = 0;
    SelEnd = -1;
}

LRESULT
CComboboxEdit::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CComboboxEdit::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_KILLFOCUS:
    {
        SendMessage(HWindow, EM_GETSEL, (WPARAM)&SelStart, (LPARAM)&SelEnd);
        break;
    }

    case EM_REPLACESEL:
    {
        LRESULT res = CWindow::WindowProc(uMsg, wParam, lParam);
        SendMessage(HWindow, EM_GETSEL, (WPARAM)&SelStart, (LPARAM)&SelEnd);
        return res;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

void CComboboxEdit::GetSel(DWORD* start, DWORD* end)
{
    if (GetFocus() == HWindow)
        SendMessage(HWindow, EM_GETSEL, (WPARAM)start, (LPARAM)end);
    else
    {
        *start = SelStart;
        *end = SelEnd;
    }
}

void CComboboxEdit::ReplaceText(const char* text)
{
    // musime ozivit selection, protoze dementni combobox ji zapomel
    SendMessage(HWindow, EM_SETSEL, SelStart, SelEnd);
    SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)text);
}

//******************************************************************************
//
// Keywords
//

const char* EXECUTE_DRIVE = "Drive";
const char* EXECUTE_PATH = "Path";
const char* EXECUTE_DOSPATH = "DOSPath";
const char* EXECUTE_NAME = "Name";
const char* EXECUTE_DOSNAME = "DOSName";
const char* EXECUTE_FULLNAME = "FullName";
const char* EXECUTE_DOSFULLNAME = "DOSFullName";
const char* EXECUTE_FULLPATH = "FullPath";
const char* EXECUTE_WINDIR = "WinDir";
const char* EXECUTE_SYSDIR = "SysDir";
const char* EXECUTE_SALDIR = "SalDir";
const char* EXECUTE_DOSFULLPATH = "DOSFullPath";
const char* EXECUTE_DOSWINDIR = "DOSWinDir";
const char* EXECUTE_DOSSYSDIR = "DOSSysDir";
const char* EXECUTE_NAMEPART = "NamePart";
const char* EXECUTE_EXTPART = "ExtPart";
const char* EXECUTE_DOSNAMEPART = "DOSNamePart";
const char* EXECUTE_DOSEXTPART = "DOSExtPart";
const char* EXECUTE_FULLPATHINACTIVE = "FullPathInactive";
const char* EXECUTE_FULLPATHLEFT = "FullPathLeft";
const char* EXECUTE_FULLPATHRIGHT = "FullPathRight";
const char* EXECUTE_COMPAREDFILELEFT = "FileToCompareLeft";
const char* EXECUTE_COMPAREDFILERIGHT = "FileToCompareRight";
const char* EXECUTE_COMPAREDDIRLEFT = "DirToCompareLeft";
const char* EXECUTE_COMPAREDDIRRIGHT = "DirToCompareRight";
const char* EXECUTE_COMPAREDFILEACT = "FileToCompareActive";
const char* EXECUTE_COMPAREDFILEINACT = "FileToCompareInactive";
const char* EXECUTE_COMPAREDDIRACT = "DirToCompareActive";
const char* EXECUTE_COMPAREDDIRINACT = "DirToCompareInactive";
const char* EXECUTE_COMPAREDLEFT = "FileOrDirToCompareLeft";
const char* EXECUTE_COMPAREDRIGHT = "FileOrDirToCompareRight";
const char* EXECUTE_COMPAREDACT = "FileOrDirToCompareActive";
const char* EXECUTE_COMPAREDINACT = "FileOrDirToCompareInactive";
const char* EXECUTE_LISTOFSELNAMES = "ListOfSelectedNames";
const char* EXECUTE_LISTOFSELFULLNAMES = "ListOfSelectedFullNames";

const char* EXECUTE_ENV = "$[]";

// dummy strings
// !!! stringy nesmi mit stejnou hodnotu, protoze release verze
// v ramci optimalizaci pak nasmeruje ukazatele na jedno misto
const char* EXECUTE_SEPARATOR = "Separator";
const char* EXECUTE_BROWSE = "Browse";       // prochazeni adresaru pomoci Open dialogu
const char* EXECUTE_BROWSEDIR = "BrowseDir"; // prochazeni adresaru pomoci GetTargetDirectory
const char* EXECUTE_HELP = "Help";
const char* EXECUTE_TERMINATOR = "Terminator";
const char* EXECUTE_SUBMENUSTART = "SubMenuStart";
const char* EXECUTE_SUBMENUEND = "SubMenuEnd";

// Information Line Content
const char* FILEDATA_FILENAME = "FileName";
const char* FILEDATA_FILESIZE = "FileSize";
const char* FILEDATA_FILEDATE = "FileDate";
const char* FILEDATA_FILETIME = "FileTime";
const char* FILEDATA_FILEATTR = "FileAttributes";
const char* FILEDATA_FILEDOSNAME = "FileDOSName";

// pro Make File List
const char* FILEDATA_FILENAMEPART = "FileNamePart";
const char* FILEDATA_FILEEXTENSION = "FileExtension";

// text file delimiter
const char* FILEDATA_LF = "LF";
const char* FILEDATA_CR = "CR";
const char* FILEDATA_CRLF = "CRLF";
const char* FILEDATA_TAB = "TAB";

// retezec zobrazovany pokud nejsou platna data pro pozadovanou promennou
const char* STR_FILE_DATA_NONE = "-";

// retezce pro regular expressions
const char* REGEXP_ANYCHAR = ".";
const char* REGEXP_SETOFCHAR = "[]";
const char* REGEXP_NOTSETOFCHAR = "[^]";
const char* REGEXP_RANGEOFCHAR = "[-]";
const char* REGEXP_BEGINOFLINE = "^";
const char* REGEXP_ENDOFLINE = "$";
const char* REGEXP_OR = "|";
const char* REGEXP_0ORMORE = "*";
const char* REGEXP_1ORMORE = "+";
const char* REGEXP_0OR1 = "?";

const char* REGEXP_PARENTHESIS_L_CHAR = "\\(";
const char* REGEXP_PARENTHESIS_R_CHAR = "\\)";
const char* REGEXP_DOT_CHAR = "\\.";
const char* REGEXP_PLUS_CHAR = "\\+";
const char* REGEXP_ASTERISK_CHAR = "\\*";

const char* REGEXP_ALPHANUMERIC_CHAR = "[a-zA-Z0-9]";
const char* REGEXP_ALPHABETIC_CHAR = "[a-zA-Z]";
const char* REGEXP_DECIMAL_DIGIT = "[0-9]";

const char* REGEXP_DECIMAL_NUMBER = "([0-9]+)";
const char* REGEXP_HEXADECIMAL_NUMBER = "([0-9a-fA-F]+)";
const char* REGEXP_REAL_NUMBER = "(([0-9]+\\.[0-9]*)|([0-9]*\\.[0-9]+)|([0-9]+))";

const char* REGEXP_QUOTED_STRING = "((\"[^\"]*\")|('[^']*'))";
const char* REGEXP_ALPHABETIC_STRING = "([a-zA-Z]+)";
const char* REGEXP_IDENTIFIER = "([a-zA-Z_$][a-zA-Z0-9_$]*)";

//******************************************************************************
//
// Preddefinovana pola
//
//

// Arguments - User Menu

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM UserMenuArgsExecutes[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_EXECUTE_FULLNAME
  {MNTT_IT, IDS_EXECUTE_DRIVE
  {MNTT_IT, IDS_EXECUTE_PATH
  {MNTT_IT, IDS_EXECUTE_NAME
  {MNTT_IT, IDS_EXECUTE_NAMEPART
  {MNTT_IT, IDS_EXECUTE_EXTPART
  {MNTT_IT, IDS_EXECUTE_FULLPATH
  {MNTT_IT, IDS_EXECUTE_WINDIR
  {MNTT_IT, IDS_EXECUTE_SYSDIR
  {MNTT_IT, IDS_EXECUTE_DOSFULLNAME
  {MNTT_IT, IDS_EXECUTE_DOSPATH
  {MNTT_IT, IDS_EXECUTE_DOSNAME
  {MNTT_IT, IDS_EXECUTE_DOSNAMEPART
  {MNTT_IT, IDS_EXECUTE_DOSEXTPART
  {MNTT_IT, IDS_EXECUTE_DOSFULLPATH
  {MNTT_IT, IDS_EXECUTE_DOSWINDIR
  {MNTT_IT, IDS_EXECUTE_DOSSYSDIR
  {MNTT_IT, IDS_EXECUTE_ENV
  {MNTT_PB, IDS_EXECUTE_ADVANCEDMENU
  {MNTT_IT, IDS_EXECUTE_FULLPATHINACTIVE
  {MNTT_IT, IDS_EXECUTE_FULLPATHLEFT
  {MNTT_IT, IDS_EXECUTE_FULLPATHRIGHT
  {MNTT_IT, IDS_EXECUTE_COMPAREDFILELEFT
  {MNTT_IT, IDS_EXECUTE_COMPAREDFILERIGHT
  {MNTT_IT, IDS_EXECUTE_COMPAREDDIRLEFT
  {MNTT_IT, IDS_EXECUTE_COMPAREDDIRRIGHT
  {MNTT_IT, IDS_EXECUTE_COMPAREDLEFT
  {MNTT_IT, IDS_EXECUTE_COMPAREDRIGHT
  {MNTT_IT, IDS_EXECUTE_COMPAREDFILEACT
  {MNTT_IT, IDS_EXECUTE_COMPAREDFILEINACT
  {MNTT_IT, IDS_EXECUTE_COMPAREDDIRACT
  {MNTT_IT, IDS_EXECUTE_COMPAREDDIRINACT
  {MNTT_IT, IDS_EXECUTE_COMPAREDACT
  {MNTT_IT, IDS_EXECUTE_COMPAREDINACT
  {MNTT_IT, IDS_EXECUTE_LISTOFSELNAMES
  {MNTT_IT, IDS_EXECUTE_LISTOFSELFULLNAMES
  {MNTT_PE, 0
  {MNTT_PE, 0
};
*/

CExecuteItem UserMenuArgsExecutes[] =
    {
        {EXECUTE_FULLNAME, IDS_EXECUTE_FULLNAME, EIF_VARIABLE},
        {EXECUTE_DRIVE, IDS_EXECUTE_DRIVE, EIF_VARIABLE},
        {EXECUTE_PATH, IDS_EXECUTE_PATH, EIF_VARIABLE},
        {EXECUTE_NAME, IDS_EXECUTE_NAME, EIF_VARIABLE},
        {EXECUTE_NAMEPART, IDS_EXECUTE_NAMEPART, EIF_VARIABLE},
        {EXECUTE_EXTPART, IDS_EXECUTE_EXTPART, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_FULLPATH, IDS_EXECUTE_FULLPATH, EIF_VARIABLE},
        {EXECUTE_WINDIR, IDS_EXECUTE_WINDIR, EIF_VARIABLE},
        {EXECUTE_SYSDIR, IDS_EXECUTE_SYSDIR, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_DOSFULLNAME, IDS_EXECUTE_DOSFULLNAME, EIF_VARIABLE},
        {EXECUTE_DOSPATH, IDS_EXECUTE_DOSPATH, EIF_VARIABLE},
        {EXECUTE_DOSNAME, IDS_EXECUTE_DOSNAME, EIF_VARIABLE},
        {EXECUTE_DOSNAMEPART, IDS_EXECUTE_DOSNAMEPART, EIF_VARIABLE},
        {EXECUTE_DOSEXTPART, IDS_EXECUTE_DOSEXTPART, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_DOSFULLPATH, IDS_EXECUTE_DOSFULLPATH, EIF_VARIABLE},
        {EXECUTE_DOSWINDIR, IDS_EXECUTE_DOSWINDIR, EIF_VARIABLE},
        {EXECUTE_DOSSYSDIR, IDS_EXECUTE_DOSSYSDIR, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_ENV, IDS_EXECUTE_ENV, EIF_CURSOR_1},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_SUBMENUSTART, IDS_EXECUTE_ADVANCEDMENU, 0},
        {EXECUTE_FULLPATHINACTIVE, IDS_EXECUTE_FULLPATHINACTIVE, EIF_VARIABLE},
        {EXECUTE_FULLPATHLEFT, IDS_EXECUTE_FULLPATHLEFT, EIF_VARIABLE},
        {EXECUTE_FULLPATHRIGHT, IDS_EXECUTE_FULLPATHRIGHT, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_COMPAREDFILELEFT, IDS_EXECUTE_COMPAREDFILELEFT, EIF_VARIABLE},
        {EXECUTE_COMPAREDFILERIGHT, IDS_EXECUTE_COMPAREDFILERIGHT, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_COMPAREDDIRLEFT, IDS_EXECUTE_COMPAREDDIRLEFT, EIF_VARIABLE},
        {EXECUTE_COMPAREDDIRRIGHT, IDS_EXECUTE_COMPAREDDIRRIGHT, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_COMPAREDLEFT, IDS_EXECUTE_COMPAREDLEFT, EIF_VARIABLE},
        {EXECUTE_COMPAREDRIGHT, IDS_EXECUTE_COMPAREDRIGHT, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_COMPAREDFILEACT, IDS_EXECUTE_COMPAREDFILEACT, EIF_VARIABLE},
        {EXECUTE_COMPAREDFILEINACT, IDS_EXECUTE_COMPAREDFILEINACT, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_COMPAREDDIRACT, IDS_EXECUTE_COMPAREDDIRACT, EIF_VARIABLE},
        {EXECUTE_COMPAREDDIRINACT, IDS_EXECUTE_COMPAREDDIRINACT, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_COMPAREDACT, IDS_EXECUTE_COMPAREDACT, EIF_VARIABLE},
        {EXECUTE_COMPAREDINACT, IDS_EXECUTE_COMPAREDINACT, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_LISTOFSELNAMES, IDS_EXECUTE_LISTOFSELNAMES, EIF_VARIABLE},
        {EXECUTE_LISTOFSELFULLNAMES, IDS_EXECUTE_LISTOFSELFULLNAMES, EIF_VARIABLE},
        {EXECUTE_SUBMENUEND, 0, 0},
        {EXECUTE_TERMINATOR, 0, 0},
};

// HotPath

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM HotPathItems[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_EXECUTE_BROWSE
  {MNTT_IT, IDS_EXECUTE_WINDIR
  {MNTT_IT, IDS_EXECUTE_SYSDIR
  {MNTT_IT, IDS_EXECUTE_SALDIR
  {MNTT_IT, IDS_EXECUTE_ENV
  {MNTT_PE, 0
};
*/

CExecuteItem HotPathItems[] =
    {
        {EXECUTE_BROWSEDIR, IDS_EXECUTE_BROWSE, EIF_REPLACE_ALL},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_WINDIR, IDS_EXECUTE_WINDIR, EIF_VARIABLE},
        {EXECUTE_SYSDIR, IDS_EXECUTE_SYSDIR, EIF_VARIABLE},
        {EXECUTE_SALDIR, IDS_EXECUTE_SALDIR, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_ENV, IDS_EXECUTE_ENV, EIF_CURSOR_1},
        {EXECUTE_TERMINATOR, 0, 0},
};

// Command - external View/Edit

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM CommandExecutes[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_EXECUTE_BROWSE
  {MNTT_IT, IDS_EXECUTE_WINDIR
  {MNTT_IT, IDS_EXECUTE_SYSDIR
  {MNTT_IT, IDS_EXECUTE_SALDIR
  {MNTT_IT, IDS_EXECUTE_ENV
  {MNTT_PE, 0
};
*/

CExecuteItem CommandExecutes[] =
    {
        {EXECUTE_BROWSE, IDS_EXECUTE_BROWSE, EIF_REPLACE_ALL},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_WINDIR, IDS_EXECUTE_WINDIR, EIF_VARIABLE},
        {EXECUTE_SYSDIR, IDS_EXECUTE_SYSDIR, EIF_VARIABLE},
        {EXECUTE_SALDIR, IDS_EXECUTE_SALDIR, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_ENV, IDS_EXECUTE_ENV, EIF_CURSOR_1},
        {EXECUTE_TERMINATOR, 0, 0},
};

// Arguments - External View/Edit

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM ArgumentsExecutes[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_EXECUTE_FULLNAME
  {MNTT_IT, IDS_EXECUTE_DRIVE
  {MNTT_IT, IDS_EXECUTE_PATH
  {MNTT_IT, IDS_EXECUTE_NAME
  {MNTT_IT, IDS_EXECUTE_NAMEPART
  {MNTT_IT, IDS_EXECUTE_EXTPART
  {MNTT_IT, IDS_EXECUTE_FULLPATH
  {MNTT_IT, IDS_EXECUTE_WINDIR
  {MNTT_IT, IDS_EXECUTE_SYSDIR
  {MNTT_IT, IDS_EXECUTE_DOSFULLNAME
  {MNTT_IT, IDS_EXECUTE_DOSPATH
  {MNTT_IT, IDS_EXECUTE_DOSNAME
  {MNTT_IT, IDS_EXECUTE_DOSNAMEPART
  {MNTT_IT, IDS_EXECUTE_DOSEXTPART
  {MNTT_IT, IDS_EXECUTE_DOSFULLPATH
  {MNTT_IT, IDS_EXECUTE_DOSWINDIR
  {MNTT_IT, IDS_EXECUTE_DOSSYSDIR
  {MNTT_IT, IDS_EXECUTE_ENV
  {MNTT_PE, 0
};
*/

CExecuteItem ArgumentsExecutes[] =
    {
        {EXECUTE_FULLNAME, IDS_EXECUTE_FULLNAME, EIF_VARIABLE},
        {EXECUTE_DRIVE, IDS_EXECUTE_DRIVE, EIF_VARIABLE},
        {EXECUTE_PATH, IDS_EXECUTE_PATH, EIF_VARIABLE},
        {EXECUTE_NAME, IDS_EXECUTE_NAME, EIF_VARIABLE},
        {EXECUTE_NAMEPART, IDS_EXECUTE_NAMEPART, EIF_VARIABLE},
        {EXECUTE_EXTPART, IDS_EXECUTE_EXTPART, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_FULLPATH, IDS_EXECUTE_FULLPATH, EIF_VARIABLE},
        {EXECUTE_WINDIR, IDS_EXECUTE_WINDIR, EIF_VARIABLE},
        {EXECUTE_SYSDIR, IDS_EXECUTE_SYSDIR, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_DOSFULLNAME, IDS_EXECUTE_DOSFULLNAME, EIF_VARIABLE},
        {EXECUTE_DOSPATH, IDS_EXECUTE_DOSPATH, EIF_VARIABLE},
        {EXECUTE_DOSNAME, IDS_EXECUTE_DOSNAME, EIF_VARIABLE},
        {EXECUTE_DOSNAMEPART, IDS_EXECUTE_DOSNAMEPART, EIF_VARIABLE},
        {EXECUTE_DOSEXTPART, IDS_EXECUTE_DOSEXTPART, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_DOSFULLPATH, IDS_EXECUTE_DOSFULLPATH, EIF_VARIABLE},
        {EXECUTE_DOSWINDIR, IDS_EXECUTE_DOSWINDIR, EIF_VARIABLE},
        {EXECUTE_DOSSYSDIR, IDS_EXECUTE_DOSSYSDIR, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_ENV, IDS_EXECUTE_ENV, EIF_CURSOR_1},
        {EXECUTE_TERMINATOR, 0, 0},
};

// Initial directory

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM InitDirExecutes[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_EXECUTE_FULLPATH
  {MNTT_IT, IDS_EXECUTE_WINDIR
  {MNTT_IT, IDS_EXECUTE_SYSDIR
  {MNTT_IT, IDS_EXECUTE_SALDIR
  {MNTT_IT, IDS_EXECUTE_DRIVE
  {MNTT_IT, IDS_EXECUTE_PATH
  {MNTT_IT, IDS_EXECUTE_ENV
  {MNTT_PE, 0
};
*/

CExecuteItem InitDirExecutes[] =
    {
        {EXECUTE_FULLPATH, IDS_EXECUTE_FULLPATH, EIF_VARIABLE},
        {EXECUTE_WINDIR, IDS_EXECUTE_WINDIR, EIF_VARIABLE},
        {EXECUTE_SYSDIR, IDS_EXECUTE_SYSDIR, EIF_VARIABLE},
        {EXECUTE_SALDIR, IDS_EXECUTE_SALDIR, EIF_VARIABLE},
        {EXECUTE_DRIVE, IDS_EXECUTE_DRIVE, EIF_VARIABLE},
        {EXECUTE_PATH, IDS_EXECUTE_PATH, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_ENV, IDS_EXECUTE_ENV, EIF_CURSOR_1},
        {EXECUTE_TERMINATOR, 0, 0},
};

// Information Line Content

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM InfoLineContentItems[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_FILEDATA_FILENAME
  {MNTT_IT, IDS_FILEDATA_FILESIZE
  {MNTT_IT, IDS_FILEDATA_FILEDATE
  {MNTT_IT, IDS_FILEDATA_FILETIME
  {MNTT_IT, IDS_FILEDATA_FILEATTR
  {MNTT_IT, IDS_FILEDATA_FILEDOSNAME
  {MNTT_PE, 0
};
*/

CExecuteItem InfoLineContentItems[] =
    {
        {FILEDATA_FILENAME, IDS_FILEDATA_FILENAME, EIF_VARIABLE},
        {FILEDATA_FILESIZE, IDS_FILEDATA_FILESIZE, EIF_VARIABLE},
        {FILEDATA_FILEDATE, IDS_FILEDATA_FILEDATE, EIF_VARIABLE},
        {FILEDATA_FILETIME, IDS_FILEDATA_FILETIME, EIF_VARIABLE},
        {FILEDATA_FILEATTR, IDS_FILEDATA_FILEATTR, EIF_VARIABLE},
        {FILEDATA_FILEDOSNAME, IDS_FILEDATA_FILEDOSNAME, EIF_VARIABLE},
        {EXECUTE_TERMINATOR, 0, 0},
};

// Make File List Items

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM MakeFileListItems[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_FILEDATA_FILENAME
  {MNTT_IT, IDS_FILEDATA_FILENAMEPART
  {MNTT_IT, IDS_FILEDATA_FILEEXTENSION
  {MNTT_IT, IDS_FILEDATA_FILESIZE
  {MNTT_IT, IDS_FILEDATA_FILEDATE
  {MNTT_IT, IDS_FILEDATA_FILETIME
  {MNTT_IT, IDS_FILEDATA_FILEATTR
  {MNTT_IT, IDS_FILEDATA_FILEDOSNAME
  {MNTT_IT, IDS_EXECUTE_DRIVE
  {MNTT_IT, IDS_EXECUTE_PATH
  {MNTT_IT, IDS_EXECUTE_DOSPATH
  {MNTT_IT, IDS_FILEDATA_LF
  {MNTT_IT, IDS_FILEDATA_CR
  {MNTT_IT, IDS_FILEDATA_CRLF
  {MNTT_IT, IDS_FILEDATA_TAB
  {MNTT_IT, IDS_EXECUTE_ENV
  {MNTT_PE, 0
};
*/

CExecuteItem MakeFileListItems[] =
    {
        {FILEDATA_FILENAME, IDS_FILEDATA_FILENAME, EIF_VARIABLE},
        {FILEDATA_FILENAMEPART, IDS_FILEDATA_FILENAMEPART, EIF_VARIABLE},
        {FILEDATA_FILEEXTENSION, IDS_FILEDATA_FILEEXTENSION, EIF_VARIABLE},
        {FILEDATA_FILESIZE, IDS_FILEDATA_FILESIZE, EIF_VARIABLE},
        {FILEDATA_FILEDATE, IDS_FILEDATA_FILEDATE, EIF_VARIABLE},
        {FILEDATA_FILETIME, IDS_FILEDATA_FILETIME, EIF_VARIABLE},
        {FILEDATA_FILEATTR, IDS_FILEDATA_FILEATTR, EIF_VARIABLE},
        {FILEDATA_FILEDOSNAME, IDS_FILEDATA_FILEDOSNAME, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_DRIVE, IDS_EXECUTE_DRIVE, EIF_VARIABLE},
        {EXECUTE_PATH, IDS_EXECUTE_PATH, EIF_VARIABLE},
        {EXECUTE_DOSPATH, IDS_EXECUTE_DOSPATH, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {FILEDATA_LF, IDS_FILEDATA_LF, EIF_VARIABLE},
        {FILEDATA_CR, IDS_FILEDATA_CR, EIF_VARIABLE},
        {FILEDATA_CRLF, IDS_FILEDATA_CRLF, EIF_VARIABLE},
        {FILEDATA_TAB, IDS_FILEDATA_TAB, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_ENV, IDS_EXECUTE_ENV, EIF_CURSOR_1},
        {EXECUTE_TERMINATOR, 0, 0},
};

// Regular Expression Items

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
MENU_TEMPLATE_ITEM RegularExpressionItems[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_REGEXP_ANYCHAR
  {MNTT_IT, IDS_REGEXP_SETOFCHAR
  {MNTT_IT, IDS_REGEXP_NOTSETOFCHAR
  {MNTT_IT, IDS_REGEXP_RANGEOFCHAR
  {MNTT_IT, IDS_REGEXP_BEGINOFLINE
  {MNTT_IT, IDS_REGEXP_ENDOFLINE
  {MNTT_IT, IDS_REGEXP_OR
  {MNTT_IT, IDS_REGEXP_0ORMORE
  {MNTT_IT, IDS_REGEXP_1ORMORE
  {MNTT_IT, IDS_REGEXP_0OR1
  {MNTT_IT, IDS_REGEXP_PARENTHESIS_L_CHAR
  {MNTT_IT, IDS_REGEXP_PARENTHESIS_R_CHAR
  {MNTT_IT, IDS_REGEXP_DOT_CHAR
  {MNTT_IT, IDS_REGEXP_PLUS_CHAR
  {MNTT_IT, IDS_REGEXP_ASTERISK_CHAR
  {MNTT_IT, IDS_REGEXP_ALPHANUMERIC_CHAR
  {MNTT_IT, IDS_REGEXP_ALPHABETIC_CHAR
  {MNTT_IT, IDS_REGEXP_DECIMAL_DIGIT
  {MNTT_IT, IDS_REGEXP_DECIMAL_NUMBER
  {MNTT_IT, IDS_REGEXP_HEXADECIMAL_NUMBER
  {MNTT_IT, IDS_REGEXP_REAL_NUMBER
  {MNTT_IT, IDS_REGEXP_ALPHABETIC_STRING
  {MNTT_IT, IDS_REGEXP_IDENTIFIER
  {MNTT_IT, IDS_REGEXP_QUOTED_STRING
  {MNTT_IT, IDS_REGEXP_HELP
  {MNTT_PE, 0
};
*/

CExecuteItem RegularExpressionItems[] =
    {
        {REGEXP_ANYCHAR, IDS_REGEXP_ANYCHAR, 0},
        {REGEXP_SETOFCHAR, IDS_REGEXP_SETOFCHAR, EIF_CURSOR_1},
        {REGEXP_NOTSETOFCHAR, IDS_REGEXP_NOTSETOFCHAR, EIF_CURSOR_1},
        {REGEXP_RANGEOFCHAR, IDS_REGEXP_RANGEOFCHAR, EIF_CURSOR_2},
        {REGEXP_BEGINOFLINE, IDS_REGEXP_BEGINOFLINE, 0},
        {REGEXP_ENDOFLINE, IDS_REGEXP_ENDOFLINE, 0},
        {REGEXP_OR, IDS_REGEXP_OR, 0},
        {REGEXP_0ORMORE, IDS_REGEXP_0ORMORE, 0},
        {REGEXP_1ORMORE, IDS_REGEXP_1ORMORE, 0},
        {REGEXP_0OR1, IDS_REGEXP_0OR1, 0},
        {EXECUTE_SEPARATOR, 0, 0},
        {REGEXP_PARENTHESIS_L_CHAR, IDS_REGEXP_PARENTHESIS_L_CHAR, 0},
        {REGEXP_PARENTHESIS_R_CHAR, IDS_REGEXP_PARENTHESIS_R_CHAR, 0},
        {REGEXP_DOT_CHAR, IDS_REGEXP_DOT_CHAR, 0},
        {REGEXP_PLUS_CHAR, IDS_REGEXP_PLUS_CHAR, 0},
        {REGEXP_ASTERISK_CHAR, IDS_REGEXP_ASTERISK_CHAR, 0},
        {EXECUTE_SEPARATOR, 0, 0},
        {REGEXP_ALPHANUMERIC_CHAR, IDS_REGEXP_ALPHANUMERIC_CHAR, 0},
        {REGEXP_ALPHABETIC_CHAR, IDS_REGEXP_ALPHABETIC_CHAR, 0},
        {REGEXP_DECIMAL_DIGIT, IDS_REGEXP_DECIMAL_DIGIT, 0},
        {EXECUTE_SEPARATOR, 0, 0},
        {REGEXP_DECIMAL_NUMBER, IDS_REGEXP_DECIMAL_NUMBER, 0},
        {REGEXP_HEXADECIMAL_NUMBER, IDS_REGEXP_HEXADECIMAL_NUMBER, 0},
        {REGEXP_REAL_NUMBER, IDS_REGEXP_REAL_NUMBER, 0},
        {EXECUTE_SEPARATOR, 0, 0},
        {REGEXP_ALPHABETIC_STRING, IDS_REGEXP_ALPHABETIC_STRING, 0},
        {REGEXP_IDENTIFIER, IDS_REGEXP_IDENTIFIER, 0},
        {REGEXP_QUOTED_STRING, IDS_REGEXP_QUOTED_STRING, 0},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_HELP, IDS_REGEXP_HELP, EIF_DONT_FOCUS},
        {EXECUTE_TERMINATOR, 0, 0},
};

//******************************************************************************
//
// Vlastni funkce
//

struct CExecuteExpData
{
    const char* Name;
    const char* DosName;
    char Buffer[MAX_PATH];
    BOOL* FileNameUsed;

    CUserMenuAdvancedData* UserMenuAdvancedData; // tyka se jen User Menu, jinak je zde NULL
};

const char* WINAPI ExecuteExpDrive(HWND msgParent, void* param) // drive ("D:", "\\server\share") bez backslashe
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    GetRootPath(data->Buffer, data->Name);
    int l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] == '\\')
        data->Buffer[l - 1] = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpPath(HWND msgParent, void* param) // plna cesta ("\\", "\\path\\")
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    GetRootPath(data->Buffer, data->Name);
    int l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] == '\\')
        l--;
    strcpy(data->Buffer, data->Name + l);
    char* s = strrchr(data->Buffer, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpPath().");
        return "\\";
    }
    *++s = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpDOSPath(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    GetRootPath(data->Buffer, data->DosName);
    int l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] == '\\')
        l--;
    strcpy(data->Buffer, data->DosName + l);
    char* s = strrchr(data->Buffer, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpDOSPath().");
        return "\\";
    }
    *++s = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpName(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->Name, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpName().");
        return "";
    }
    strcpy(data->Buffer, s + 1);
    return data->Buffer;
}

const char* WINAPI ExecuteExpDOSName(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->DosName, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpName().");
        return "";
    }
    strcpy(data->Buffer, s + 1);
    return data->Buffer;
}

const char* WINAPI ExecuteExpPath2(HWND msgParent, void* param) // plna cesta ("\\", "\\path")
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    GetRootPath(data->Buffer, data->Name);
    int l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] == '\\')
        l--;
    strcpy(data->Buffer, data->Name + l);
    char* s = strrchr(data->Buffer, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpPath2().");
        return "\\";
    }
    if (s != data->Buffer)
        *s = 0;
    else
        *++s = 0; // root musi koncit na '\\'
    return data->Buffer;
}

const char* WINAPI ExecuteExpFullName(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->Name, '\\');
    if (s != NULL && *(s + 1) == 0)
        return ""; // user-menu na "" nebo ".."
    return data->Name;
}

const char* WINAPI ExecuteExpDOSFullName(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->DosName, '\\');
    if (s != NULL && *(s + 1) == 0)
        return ""; // user-menu na "" nebo ".."
    return data->DosName;
}

const char* WINAPI ExecuteExpNamePart(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->Name, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpNamePart().");
        return "";
    }
    strcpy(data->Buffer, s + 1);
    char* ss = strrchr(data->Buffer, '.');
    //  if (ss != NULL && ss != data->Buffer)   // je zde pripona ('.' a to ne na zacatku jmena, viz ".cvspass")
    if (ss != NULL) // je zde pripona (".cvspass" ve Windows je pripona)
        *ss = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpExtPart(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->Name, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpNamePart().");
        return "";
    }
    strcpy(data->Buffer, s + 1);
    s = strrchr(data->Buffer, '.');
    //  if (s != NULL && s != data->Buffer)   // je zde pripona ('.' a to ne na zacatku jmena, viz ".cvspass")
    if (s != NULL) // je zde pripona (".cvspass" ve Windows je pripona)
        return s + 1;
    return "";
}

const char* WINAPI ExecuteExpDOSNamePart(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->DosName, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpDOSNamePart().");
        return "";
    }
    strcpy(data->Buffer, s + 1);
    char* ss = strrchr(data->Buffer, '.');
    //  if (ss != NULL && ss != data->Buffer)   // je zde pripona ('.' a to ne na zacatku jmena, viz ".cvspass")
    if (ss != NULL) // je zde pripona (".cvspass" ve Windows je pripona)
        *ss = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpDOSExtPart(HWND msgParent, void* param)
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    const char* s = strrchr(data->DosName, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpDOSNamePart().");
        return "";
    }
    strcpy(data->Buffer, s + 1);
    s = strrchr(data->Buffer, '.');
    //  if (s != NULL && s != data->Buffer)   // je zde pripona ('.' a to ne na zacatku jmena, viz ".cvspass")
    if (s != NULL) // je zde pripona (".cvspass" ve Windows je pripona)
        return s + 1;
    return "";
}

const char* WINAPI ExecuteExpFullPath(HWND msgParent, void* param) // plna cesta "c:\\long path\\"
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    strcpy(data->Buffer, data->Name);
    char* s = strrchr(data->Buffer, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpFullPath().");
        return "C:\\";
    }
    *++s = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpWinDir(HWND msgParent, void* param) // plna cesta do Windows adresare
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    GetWindowsDirectory(data->Buffer, MAX_PATH);
    char* s = data->Buffer + strlen(data->Buffer);
    if (s > data->Buffer && *(s - 1) != '\\')
        strcat(data->Buffer, "\\");
    return data->Buffer;
}

const char* WINAPI ExecuteExpSysDir(HWND msgParent, void* param) // plna cesta do Systemoveho adresare
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    GetSystemDirectory(data->Buffer, MAX_PATH);
    char* s = data->Buffer + strlen(data->Buffer);
    if (s > data->Buffer && *(s - 1) != '\\')
        strcat(data->Buffer, "\\");
    return data->Buffer;
}

const char* WINAPI ExecuteExpSalDir(HWND msgParent, void* param) // plna cesta do Salamanderiho adresare
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    GetModuleFileName(HInstance, data->Buffer, MAX_PATH);
    *(strrchr(data->Buffer, '\\') + 1) = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpDOSFullPath(HWND msgParent, void* param) // DOS plna cesta "c:\\sh_path\\"
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    strcpy(data->Buffer, data->DosName);
    char* s = strrchr(data->Buffer, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpDOSFullPath().");
        return "C:\\";
    }
    *++s = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpDOSWinDir(HWND msgParent, void* param) // DOS plna cesta do Windows adresare
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    char path[MAX_PATH];
    GetWindowsDirectory(path, MAX_PATH);
    GetShortPathName(path, data->Buffer, MAX_PATH);
    char* s = data->Buffer + strlen(data->Buffer);
    if (s > data->Buffer && *(s - 1) != '\\')
        strcat(data->Buffer, "\\");
    return data->Buffer;
}

const char* WINAPI ExecuteExpDOSSysDir(HWND msgParent, void* param) // DOS plna cesta do Systemoveho adresare
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    char path[MAX_PATH];
    GetSystemDirectory(path, MAX_PATH);
    GetShortPathName(path, data->Buffer, MAX_PATH);
    char* s = data->Buffer + strlen(data->Buffer);
    if (s > data->Buffer && *(s - 1) != '\\')
        strcat(data->Buffer, "\\");
    return data->Buffer;
}

const char* WINAPI ExecuteExpFullPath2(HWND msgParent, void* param) // plna cesta "c:\\long path"
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    if (data->FileNameUsed != NULL)
        *data->FileNameUsed = TRUE;
    strcpy(data->Buffer, data->Name);
    char* s = strrchr(data->Buffer, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpFullPath2().");
        return "C:\\";
    }
    if (s - data->Buffer == 2 && data->Buffer[1] == ':') // ne UNC cesta
    {
        *(s + 1) = 0;
    }
    else
    {
        *s = 0;
    }
    return data->Buffer;
}

const char* WINAPI ExecuteExpWinDir2(HWND msgParent, void* param) // plna cesta do Windows adresare, bez '\\' na konci
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    GetWindowsDirectory(data->Buffer, MAX_PATH);
    char* s = data->Buffer + strlen(data->Buffer);
    if (s > data->Buffer && *(s - 1) == '\\')
        *(s - 1) = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpSysDir2(HWND msgParent, void* param) // plna cesta do Systemoveho adresare, bez '\\' na konci
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    GetSystemDirectory(data->Buffer, MAX_PATH);
    char* s = data->Buffer + strlen(data->Buffer);
    if (s > data->Buffer && *(s - 1) == '\\')
        *(s - 1) = 0;
    return data->Buffer;
}

const char* WINAPI ExecuteExpSalDir2(HWND msgParent, void* param) // plna cesta do Salamanderiho adresare, bez '\\' na konci
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    GetModuleFileName(HInstance, data->Buffer, MAX_PATH);
    char* s = strrchr(data->Buffer, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in ExecuteExpSalDir2().");
        return "C:\\";
    }
    if (s - data->Buffer == 2 && data->Buffer[1] == ':') // ne UNC cesta
    {
        *(s + 1) = 0;
    }
    else
    {
        *s = 0;
    }
    return data->Buffer;
}

// Information Line Content

struct CFileDataExpData
{
    CPluginDataInterfaceEncapsulation* PluginData;
    const CFileData* FileData;
    BOOL IsDir;          // jde o soubor, ne adresar
    DWORD ValidFileData; // maska platnych dat v CFileData
    char Path[MAX_PATH]; // cesta do aktualniho panelu (jen pro Make File List)
    char Buffer[2000];
};

const char* WINAPI FileDataExpFileName(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    AlterFileName(data->Buffer, data->FileData->Name, -1, Configuration.FileNameFormat,
                  0, data->IsDir);
    return data->Buffer;
}

const char* WINAPI FileDataExpFileNamePart(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    AlterFileName(data->Buffer, data->FileData->Name, -1, Configuration.FileNameFormat,
                  0, data->IsDir);
    if (!data->IsDir && data->FileData->Ext != NULL && *data->FileData->Ext != 0)
    {
        if (data->FileData->Ext > data->FileData->Name) // (always true)
            data->Buffer[data->FileData->Ext - data->FileData->Name - 1] = 0;
    }
    return data->Buffer;
}

const char* WINAPI FileDataExpFileExtension(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    AlterFileName(data->Buffer, data->FileData->Name, -1, Configuration.FileNameFormat,
                  0, data->IsDir);
    if (!data->IsDir && data->FileData->Ext != NULL && *data->FileData->Ext != 0)
    {
        memmove(data->Buffer, data->Buffer + (data->FileData->Ext - data->FileData->Name),
                data->FileData->NameLen - (data->FileData->Ext - data->FileData->Name) + 1);
    }
    else
        *data->Buffer = 0;
    return data->Buffer;
}

const char* WINAPI FileDataExpFileSize(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    BOOL sizeValid = FALSE;
    CQuadWord plSize;
    BOOL plSizeValid = FALSE;
    if (data->ValidFileData & VALID_DATA_SIZE)
        sizeValid = TRUE;
    else
    {
        if ((data->ValidFileData & VALID_DATA_PL_SIZE) &&
            data->PluginData->NotEmpty() &&
            data->PluginData->GetByteSize(data->FileData, data->IsDir, &plSize))
        {
            plSizeValid = TRUE;
        }
    }
    if (!sizeValid && !plSizeValid && !data->IsDir)
    { // jde o soubor a size neni platne -> neni co ukazat
        return STR_FILE_DATA_NONE;
    }
    if (!data->IsDir || sizeValid && data->FileData->SizeValid || plSizeValid)
        NumberToStr(data->Buffer, plSizeValid ? plSize : data->FileData->Size);
    else
        CopyMemory(data->Buffer, DirColumnStr, DirColumnStrLen + 1);
    return data->Buffer;
}

const char* WINAPI FileDataExpFileSizeNoSpaces(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    BOOL sizeValid = FALSE;
    CQuadWord plSize;
    BOOL plSizeValid = FALSE;
    if (data->ValidFileData & VALID_DATA_SIZE)
        sizeValid = TRUE;
    else
    {
        if ((data->ValidFileData & VALID_DATA_PL_SIZE) &&
            data->PluginData->NotEmpty() &&
            data->PluginData->GetByteSize(data->FileData, data->IsDir, &plSize))
        {
            plSizeValid = TRUE;
        }
    }
    if (!sizeValid && !plSizeValid && !data->IsDir)
    { // jde o soubor a size neni platne -> neni co ukazat
        return STR_FILE_DATA_NONE;
    }
    if (!data->IsDir || sizeValid && data->FileData->SizeValid || plSizeValid)
    {
        _ui64toa(plSizeValid ? plSize.Value : data->FileData->Size.Value, data->Buffer, 10);
    }
    else
        CopyMemory(data->Buffer, DirColumnStr, DirColumnStrLen + 1);
    return data->Buffer;
}

const char* WINAPI FileDataExpFileDate(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    SYSTEMTIME st;
    FILETIME ft;
    if ((data->ValidFileData & VALID_DATA_DATE) == 0 &&
        ((data->ValidFileData & VALID_DATA_PL_DATE) == 0 ||
         !data->PluginData->NotEmpty() ||
         !data->PluginData->GetLastWriteDate(data->FileData, data->IsDir, &st)))
    { // last-write neni platne -> neni co ukazat
        return STR_FILE_DATA_NONE;
    }
    if ((data->ValidFileData & VALID_DATA_DATE) == 0) // datum z pluginu - je potreba zvalidnit "casovou cast" struktury
    {
        st.wHour = 0;
        st.wMinute = 0;
        st.wSecond = 0;
        st.wMilliseconds = 0;
    }
    if ((data->ValidFileData & VALID_DATA_DATE) == 0 ||
        FileTimeToLocalFileTime(&data->FileData->LastWrite, &ft) &&
            FileTimeToSystemTime(&ft, &st))
    {
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, data->Buffer, 50) == 0)
            sprintf(data->Buffer, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    }
    else
        strcpy(data->Buffer, LoadStr(IDS_INVALID_DATEORTIME));
    return data->Buffer;
}

const char* WINAPI FileDataExpFileDateOnlyForDisk(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    SYSTEMTIME st;
    FILETIME ft;
    BOOL stIsReady = FALSE;
    if (FileTimeToLocalFileTime(&data->FileData->LastWrite, &ft) &&
        FileTimeToSystemTime(&ft, &st) && (stIsReady = TRUE) != 0 &&
        st.wYear == 1602 && st.wMonth == 1 && st.wDay == 1 && st.wHour == 0 &&
        st.wMinute == 0 && st.wSecond == 0 && st.wMilliseconds == 0 &&
        strcmp(data->FileData->Name, "..") == 0) // pro UP-DIR je 1.1.1602 0:00:00.000 je "prazdna hodnota"
    {
        return STR_FILE_DATA_NONE;
    }
    if (stIsReady)
    {
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, data->Buffer, 50) == 0)
            sprintf(data->Buffer, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    }
    else
        strcpy(data->Buffer, LoadStr(IDS_INVALID_DATEORTIME));
    return data->Buffer;
}

const char* WINAPI FileDataExpFileTime(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    SYSTEMTIME st;
    FILETIME ft;
    if ((data->ValidFileData & VALID_DATA_TIME) == 0 &&
        ((data->ValidFileData & VALID_DATA_PL_TIME) == 0 ||
         !data->PluginData->NotEmpty() ||
         !data->PluginData->GetLastWriteTime(data->FileData, data->IsDir, &st)))
    { // last-write neni platne -> neni co ukazat
        return STR_FILE_DATA_NONE;
    }
    if ((data->ValidFileData & VALID_DATA_TIME) == 0) // cas z pluginu - je potreba zvalidnit "datumovou cast" struktury
    {
        st.wYear = 2000;
        st.wMonth = 12;
        st.wDay = 24;
        st.wDayOfWeek = 0; // nedele
    }
    if ((data->ValidFileData & VALID_DATA_TIME) == 0 ||
        FileTimeToLocalFileTime(&data->FileData->LastWrite, &ft) &&
            FileTimeToSystemTime(&ft, &st))
    {
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, data->Buffer, 50) == 0)
            sprintf(data->Buffer, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    }
    else
        strcpy(data->Buffer, LoadStr(IDS_INVALID_DATEORTIME));
    return data->Buffer;
}

const char* WINAPI FileDataExpFileTimeOnlyForDisk(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    SYSTEMTIME st;
    FILETIME ft;
    BOOL stIsReady = FALSE;
    if (FileTimeToLocalFileTime(&data->FileData->LastWrite, &ft) &&
        FileTimeToSystemTime(&ft, &st) && (stIsReady = TRUE) != 0 &&
        st.wYear == 1602 && st.wMonth == 1 && st.wDay == 1 && st.wHour == 0 &&
        st.wMinute == 0 && st.wSecond == 0 && st.wMilliseconds == 0 &&
        strcmp(data->FileData->Name, "..") == 0) // pro UP-DIR je 1.1.1602 0:00:00.000 je "prazdna hodnota"
    {
        return STR_FILE_DATA_NONE;
    }
    if (stIsReady)
    {
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, data->Buffer, 50) == 0)
            sprintf(data->Buffer, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    }
    else
        strcpy(data->Buffer, LoadStr(IDS_INVALID_DATEORTIME));
    return data->Buffer;
}

const char* WINAPI FileDataExpFileAttr(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    if ((data->ValidFileData & VALID_DATA_ATTRIBUTES) == 0)
    { // attr neni platne -> neni co ukazat
        return STR_FILE_DATA_NONE;
    }
    char attr[20];
    GetAttrsString(attr, data->FileData->Attr);
    if (attr[0] == 0)
    {
        attr[0] = '-'; // zadny atribut
        attr[1] = 0;
    }
    strcpy(data->Buffer, attr);
    return data->Buffer;
}

const char* WINAPI FileDataExpFileDOSName(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    if ((data->ValidFileData & VALID_DATA_DOSNAME) == 0)
    { // dos-name neni platne -> neni co ukazat
        return STR_FILE_DATA_NONE;
    }
    strcpy(data->Buffer, (data->FileData->DosName != NULL ? data->FileData->DosName : data->FileData->Name));
    return data->Buffer;
}

const char* WINAPI MFLFileDataExpDrive(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    GetRootPath(data->Buffer, data->Path);
    int l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] == '\\')
        data->Buffer[l - 1] = 0;
    return data->Buffer;
}

const char* WINAPI MFLFileDataExpPath(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    GetRootPath(data->Buffer, data->Path);
    int l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] == '\\')
        l--;
    strcpy(data->Buffer, data->Path + l);
    l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] != '\\')
    {
        data->Buffer[l] = '\\';
        data->Buffer[l + 1] = 0;
    }
    return data->Buffer;
}

const char* WINAPI MFLFileDataExpDOSPath(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    char dosPath[MAX_PATH];
    if (!GetShortPathName(data->Path, dosPath, MAX_PATH))
    {
        TRACE_E("Unexpected situation in FileDataExpDOSPath().");
        return MFLFileDataExpPath(msgParent, param);
    }

    GetRootPath(data->Buffer, dosPath);
    int l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] == '\\')
        l--;
    strcpy(data->Buffer, dosPath + l);
    l = (int)strlen(data->Buffer);
    if (l > 0 && data->Buffer[l - 1] != '\\')
    {
        data->Buffer[l] = '\\';
        data->Buffer[l + 1] = 0;
    }
    return data->Buffer;
}

const char* WINAPI FileDataExpLF(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    strcpy(data->Buffer, "\n");
    return data->Buffer;
}

const char* WINAPI FileDataExpCR(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    strcpy(data->Buffer, "\r");
    return data->Buffer;
}

const char* WINAPI FileDataExpCRLF(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    strcpy(data->Buffer, "\r\n");
    return data->Buffer;
}

const char* WINAPI FileDataExpTAB(HWND msgParent, void* param)
{
    CFileDataExpData* data = (CFileDataExpData*)param;
    strcpy(data->Buffer, "\t");
    return data->Buffer;
}

const char* WINAPI ExecuteValDummy(HWND msgParent, void* param)
{
    return "";
}

const char* WINAPI ExecuteValOneByOne(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsOneByOne = TRUE;
    return "";
}

const char* WINAPI ExecuteValFullPathInact(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->UsesFullPathInactive = TRUE;
    return "";
}

const char* WINAPI ExecuteValFullPathLeft(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->UsesFullPathLeft = TRUE;
    return "";
}

const char* WINAPI ExecuteValFullPathRight(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->UsesFullPathRight = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompFileLeft(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 1 /* file-left-right */;
    else
    {
        if (data->UsedCompareType != 1)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareLeftOrActive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompFileRight(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 1 /* file-left-right */;
    else
    {
        if (data->UsedCompareType != 1)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareRightOrInactive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompDirLeft(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 3 /* dir-left-right */;
    else
    {
        if (data->UsedCompareType != 3)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareLeftOrActive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompDirRight(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 3 /* dir-left-right */;
    else
    {
        if (data->UsedCompareType != 3)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareRightOrInactive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompLeft(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 6 /* file-or-dir-left-right */;
    else
    {
        if (data->UsedCompareType != 6)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareLeftOrActive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompRight(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 6 /* file-or-dir-left-right */;
    else
    {
        if (data->UsedCompareType != 6)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareRightOrInactive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompFileActive(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 2 /* file-active-inactive */;
    else
    {
        if (data->UsedCompareType != 2)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareLeftOrActive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompFileInact(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 2 /* file-active-inactive */;
    else
    {
        if (data->UsedCompareType != 2)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareRightOrInactive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompDirActive(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 4 /* dir-active-inactive */;
    else
    {
        if (data->UsedCompareType != 4)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareLeftOrActive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompDirInact(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 4 /* dir-active-inactive */;
    else
    {
        if (data->UsedCompareType != 4)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareRightOrInactive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompActive(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 7 /* file-or-dir-active-inactive */;
    else
    {
        if (data->UsedCompareType != 7)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareLeftOrActive = TRUE;
    return "";
}

const char* WINAPI ExecuteValCompInact(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    if (data->UsedCompareType == 0)
        data->UsedCompareType = 7 /* file-or-dir-active-inactive */;
    else
    {
        if (data->UsedCompareType != 7)
            data->UsedCompareType = 5 /* kolize vice typu */;
    }
    data->UsedCompareRightOrInactive = TRUE;
    return "";
}

const char* WINAPI ExecuteValListOfSelNames(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    data->UsesListOfSelNames = TRUE;
    return "";
}

const char* WINAPI ExecuteValListOfSelFullNames(HWND msgParent, void* param)
{
    CUserMenuValidationData* data = (CUserMenuValidationData*)param;
    data->MustHandleItemsAsGroup = TRUE;
    data->UsesListOfSelFullNames = TRUE;
    return "";
}

const char* WINAPI ExecuteExpFullPathInact(HWND msgParent, void* param) // plna cesta "c:\\long path\\"
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    return data->UserMenuAdvancedData->FullPathInactive;
}

const char* WINAPI ExecuteExpFullPathLeft(HWND msgParent, void* param) // plna cesta "c:\\long path\\"
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    return data->UserMenuAdvancedData->FullPathLeft;
}

const char* WINAPI ExecuteExpFullPathRight(HWND msgParent, void* param) // plna cesta "c:\\long path\\"
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    return data->UserMenuAdvancedData->FullPathRight;
}

const char* WINAPI ExecuteExpCompareName1(HWND msgParent, void* param) // plne jmeno k porovnavanemu souboru/adresari
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    return data->UserMenuAdvancedData->CompareName1;
}

const char* WINAPI ExecuteExpCompareName2(HWND msgParent, void* param) // plne jmeno k porovnavanemu souboru/adresari
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    return data->UserMenuAdvancedData->CompareName2;
}

const char* WINAPI ExecuteExpListOfSelNames(HWND msgParent, void* param) // seznam jmen (bez cest) oddelenych mezerami
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    return data->UserMenuAdvancedData->ListOfSelNames;
}

const char* WINAPI ExecuteExpListOfSelFullNames(HWND msgParent, void* param) // seznam jmen (bez cest) oddelenych mezerami
{
    CExecuteExpData* data = (CExecuteExpData*)param;
    return data->UserMenuAdvancedData->ListOfSelFullNames;
}

// Arrays

CSalamanderVarStrEntry UserMenuArgsExpArray[] =
    {
        {EXECUTE_DRIVE, ExecuteExpDrive},
        {EXECUTE_PATH, ExecuteExpPath},
        {EXECUTE_NAME, ExecuteExpName},
        {EXECUTE_DOSPATH, ExecuteExpDOSPath},
        {EXECUTE_DOSNAME, ExecuteExpDOSName},
        {EXECUTE_FULLNAME, ExecuteExpFullName},
        {EXECUTE_DOSFULLNAME, ExecuteExpDOSFullName},
        {EXECUTE_FULLPATH, ExecuteExpFullPath},
        {EXECUTE_WINDIR, ExecuteExpWinDir},
        {EXECUTE_SYSDIR, ExecuteExpSysDir},
        {EXECUTE_DOSFULLPATH, ExecuteExpDOSFullPath},
        {EXECUTE_DOSWINDIR, ExecuteExpDOSWinDir},
        {EXECUTE_DOSSYSDIR, ExecuteExpDOSSysDir},
        {EXECUTE_NAMEPART, ExecuteExpNamePart},
        {EXECUTE_EXTPART, ExecuteExpExtPart},
        {EXECUTE_DOSNAMEPART, ExecuteExpDOSNamePart},
        {EXECUTE_DOSEXTPART, ExecuteExpDOSExtPart},
        {EXECUTE_FULLPATHINACTIVE, ExecuteExpFullPathInact},
        {EXECUTE_FULLPATHLEFT, ExecuteExpFullPathLeft},
        {EXECUTE_FULLPATHRIGHT, ExecuteExpFullPathRight},
        {EXECUTE_COMPAREDFILELEFT, ExecuteExpCompareName1},
        {EXECUTE_COMPAREDFILERIGHT, ExecuteExpCompareName2},
        {EXECUTE_COMPAREDDIRLEFT, ExecuteExpCompareName1},
        {EXECUTE_COMPAREDDIRRIGHT, ExecuteExpCompareName2},
        {EXECUTE_COMPAREDLEFT, ExecuteExpCompareName1},
        {EXECUTE_COMPAREDRIGHT, ExecuteExpCompareName2},
        {EXECUTE_COMPAREDFILEACT, ExecuteExpCompareName1},
        {EXECUTE_COMPAREDFILEINACT, ExecuteExpCompareName2},
        {EXECUTE_COMPAREDDIRACT, ExecuteExpCompareName1},
        {EXECUTE_COMPAREDDIRINACT, ExecuteExpCompareName2},
        {EXECUTE_COMPAREDACT, ExecuteExpCompareName1},
        {EXECUTE_COMPAREDINACT, ExecuteExpCompareName2},
        {EXECUTE_LISTOFSELNAMES, ExecuteExpListOfSelNames},
        {EXECUTE_LISTOFSELFULLNAMES, ExecuteExpListOfSelFullNames},
        {NULL, NULL}};

// jen pro zjisteni, jake promenne se pouzivaji, aby se dalo zkontrolovat, jestli jsou to pripustne kombinace
CSalamanderVarStrEntry UserMenuArgsValidationArray[] =
    {
        {EXECUTE_DRIVE, ExecuteValDummy},
        {EXECUTE_PATH, ExecuteValDummy},
        {EXECUTE_DOSPATH, ExecuteValDummy},
        {EXECUTE_FULLPATH, ExecuteValDummy},
        {EXECUTE_WINDIR, ExecuteValDummy},
        {EXECUTE_SYSDIR, ExecuteValDummy},
        {EXECUTE_DOSFULLPATH, ExecuteValDummy},
        {EXECUTE_DOSWINDIR, ExecuteValDummy},
        {EXECUTE_DOSSYSDIR, ExecuteValDummy},
        {EXECUTE_NAME, ExecuteValOneByOne},
        {EXECUTE_DOSNAME, ExecuteValOneByOne},
        {EXECUTE_FULLNAME, ExecuteValOneByOne},
        {EXECUTE_DOSFULLNAME, ExecuteValOneByOne},
        {EXECUTE_NAMEPART, ExecuteValOneByOne},
        {EXECUTE_EXTPART, ExecuteValOneByOne},
        {EXECUTE_DOSNAMEPART, ExecuteValOneByOne},
        {EXECUTE_DOSEXTPART, ExecuteValOneByOne},
        {EXECUTE_FULLPATHINACTIVE, ExecuteValFullPathInact},
        {EXECUTE_FULLPATHLEFT, ExecuteValFullPathLeft},
        {EXECUTE_FULLPATHRIGHT, ExecuteValFullPathRight},
        {EXECUTE_COMPAREDFILELEFT, ExecuteValCompFileLeft},
        {EXECUTE_COMPAREDFILERIGHT, ExecuteValCompFileRight},
        {EXECUTE_COMPAREDDIRLEFT, ExecuteValCompDirLeft},
        {EXECUTE_COMPAREDDIRRIGHT, ExecuteValCompDirRight},
        {EXECUTE_COMPAREDLEFT, ExecuteValCompLeft},
        {EXECUTE_COMPAREDRIGHT, ExecuteValCompRight},
        {EXECUTE_COMPAREDFILEACT, ExecuteValCompFileActive},
        {EXECUTE_COMPAREDFILEINACT, ExecuteValCompFileInact},
        {EXECUTE_COMPAREDDIRACT, ExecuteValCompDirActive},
        {EXECUTE_COMPAREDDIRINACT, ExecuteValCompDirInact},
        {EXECUTE_COMPAREDACT, ExecuteValCompActive},
        {EXECUTE_COMPAREDINACT, ExecuteValCompInact},
        {EXECUTE_LISTOFSELNAMES, ExecuteValListOfSelNames},
        {EXECUTE_LISTOFSELFULLNAMES, ExecuteValListOfSelFullNames},
        {NULL, NULL}};

CSalamanderVarStrEntry CommandExpArray[] =
    {
        {EXECUTE_WINDIR, ExecuteExpWinDir},
        {EXECUTE_SYSDIR, ExecuteExpSysDir},
        {EXECUTE_SALDIR, ExecuteExpSalDir},
        {NULL, NULL}};

CSalamanderVarStrEntry HotPathExpArray[] =
    {
        {EXECUTE_WINDIR, ExecuteExpWinDir},
        {EXECUTE_SYSDIR, ExecuteExpSysDir},
        {EXECUTE_SALDIR, ExecuteExpSalDir},
        {NULL, NULL}};

CSalamanderVarStrEntry ArgumentsExpArray[] =
    {
        {EXECUTE_DRIVE, ExecuteExpDrive},
        {EXECUTE_PATH, ExecuteExpPath},
        {EXECUTE_NAME, ExecuteExpName},
        {EXECUTE_DOSPATH, ExecuteExpDOSPath},
        {EXECUTE_DOSNAME, ExecuteExpDOSName},
        {EXECUTE_FULLNAME, ExecuteExpFullName},
        {EXECUTE_DOSFULLNAME, ExecuteExpDOSFullName},
        {EXECUTE_FULLPATH, ExecuteExpFullPath},
        {EXECUTE_WINDIR, ExecuteExpWinDir},
        {EXECUTE_SYSDIR, ExecuteExpSysDir},
        {EXECUTE_DOSFULLPATH, ExecuteExpDOSFullPath},
        {EXECUTE_DOSWINDIR, ExecuteExpDOSWinDir},
        {EXECUTE_DOSSYSDIR, ExecuteExpDOSSysDir},
        {EXECUTE_NAMEPART, ExecuteExpNamePart},
        {EXECUTE_EXTPART, ExecuteExpExtPart},
        {EXECUTE_DOSNAMEPART, ExecuteExpDOSNamePart},
        {EXECUTE_DOSEXTPART, ExecuteExpDOSExtPart},
        {NULL, NULL}};

CSalamanderVarStrEntry InitDirExpArray[] =
    {
        {EXECUTE_DRIVE, ExecuteExpDrive},
        {EXECUTE_PATH, ExecuteExpPath2},         // j.r. nemam tuseni, proc se zde volaji specialni verze promennych bez
        {EXECUTE_FULLPATH, ExecuteExpFullPath2}, // zpetnych lomitek na konci; kazdopadne jsem nyni pridal volani funkce
        {EXECUTE_WINDIR, ExecuteExpWinDir2},     // RemoveDoubleBackslahesFromPath, takze bychom asi mohli prejit na verze
        {EXECUTE_SYSDIR, ExecuteExpSysDir2},     // s lomitkama na konci -- pouze bychom do ExpandInitDir() dopsali oriznuti
        {EXECUTE_SALDIR, ExecuteExpSalDir2},     // posledniho zpetneho lomitka (pokud nejde o root)
        {NULL, NULL}};

// !!! nepouzivat primo InfoLineExpArray, pouzivat GetInfoLineExpArray()
CSalamanderVarStrEntry InfoLineExpArray[] =
    {
        {FILEDATA_FILENAME, FileDataExpFileName},
        {FILEDATA_FILESIZE, FileDataExpFileSize},
        {FILEDATA_FILEDATE, NULL /* viz nize */},
        {FILEDATA_FILETIME, NULL /* viz nize */},
        {FILEDATA_FILEATTR, FileDataExpFileAttr},
        {FILEDATA_FILEDOSNAME, FileDataExpFileDOSName},
        {NULL, NULL}};

CSalamanderVarStrEntry* GetInfoLineExpArray(BOOL isDisk)
{
    if (isDisk)
    {
        InfoLineExpArray[2].Execute = FileDataExpFileDateOnlyForDisk;
        InfoLineExpArray[3].Execute = FileDataExpFileTimeOnlyForDisk;
    }
    else
    {
        InfoLineExpArray[2].Execute = FileDataExpFileDate;
        InfoLineExpArray[3].Execute = FileDataExpFileTime;
    }
    return InfoLineExpArray;
}

CSalamanderVarStrEntry MakeFileListExpArray[] =
    {
        {FILEDATA_FILENAME, FileDataExpFileName},
        {FILEDATA_FILENAMEPART, FileDataExpFileNamePart},
        {FILEDATA_FILEEXTENSION, FileDataExpFileExtension},
        {FILEDATA_FILESIZE, FileDataExpFileSizeNoSpaces},
        {FILEDATA_FILEDATE, FileDataExpFileDate},
        {FILEDATA_FILETIME, FileDataExpFileTime},
        {FILEDATA_FILEATTR, FileDataExpFileAttr},
        {FILEDATA_FILEDOSNAME, FileDataExpFileDOSName},
        {FILEDATA_LF, FileDataExpLF},
        {FILEDATA_CR, FileDataExpCR},
        {FILEDATA_CRLF, FileDataExpCRLF},
        {FILEDATA_TAB, FileDataExpTAB},
        {EXECUTE_DRIVE, MFLFileDataExpDrive},
        {EXECUTE_PATH, MFLFileDataExpPath},
        {EXECUTE_DOSPATH, MFLFileDataExpDOSPath},
        {NULL, NULL}};

BOOL BrowseDirCommand(HWND hParent, int editlineResID, int filterResID)
{
    CALL_STACK_MESSAGE2("BrowseDirCommand(, %d)", editlineResID);
    char path[MAX_PATH];
    SendMessage(GetDlgItem(hParent, editlineResID), WM_GETTEXT,
                MAX_PATH, (LPARAM)path);

    CALL_STACK_MESSAGE1("BrowseDirCommand::GetOpenFileName");
    if (GetTargetDirectory(hParent, hParent, LoadStr(IDS_BROWSEDIRECTORY_TITLE), LoadStr(IDS_BROWSEDIRECTORY_TEXT), path, FALSE, path))
    {
        CALL_STACK_MESSAGE2("BrowseDirCommand::SendMessage( , , ,%s)", path);
        SendMessage(GetDlgItem(hParent, editlineResID), WM_SETTEXT, 0, (LPARAM)path);
        return TRUE;
    }
    return FALSE;
}

BOOL ValidateUserMenuArguments(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                               CUserMenuValidationData* userMenuValidationData)
{
    CALL_STACK_MESSAGE2("ValidateUserMenuArguments(, %s, ,)", strlen(varText) > 300 ? "(too long)" : varText);
    CUserMenuValidationData dummyUserMenuValidationData;
    if (userMenuValidationData == NULL)
        userMenuValidationData = &dummyUserMenuValidationData; // volajiciho nezajimaji validacni data
    memset(userMenuValidationData, 0, sizeof(CUserMenuValidationData));
    if (!ValidateVarString(msgParent, varText, errorPos1, errorPos2, UserMenuArgsExpArray))
        return FALSE; // pokud jde o syntaktickou chybu, nahlasime ji zde (vcetne pozice pro editaci)
    errorPos1 = errorPos2 = 0;
    char dummyBuffer[USRMNUARGS_MAXLEN];
    ExpandVarString(msgParent, varText, dummyBuffer, USRMNUARGS_MAXLEN, UserMenuArgsValidationArray,
                    userMenuValidationData, TRUE);

    if (userMenuValidationData->MustHandleItemsAsGroup &&
        userMenuValidationData->MustHandleItemsOneByOne)
    { // nekompatibilni prace s oznacenim
        SalMessageBox(msgParent, LoadStr(IDS_INCOMPATIBLEARGS),
                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    if (userMenuValidationData->UsedCompareType == 5)
    { // kolize vice typu Compare parametru
        SalMessageBox(msgParent, LoadStr(IDS_COMPAREARGSCOLISION),
                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    if (userMenuValidationData->UsedCompareType != 0 &&
        (!userMenuValidationData->UsedCompareLeftOrActive ||
         !userMenuValidationData->UsedCompareRightOrInactive))
    { // nepouzivaji se oba Compare parametry -> nesmysl
        SalMessageBox(msgParent, LoadStr(IDS_COMPARENEEDSBOTHARGS),
                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

BOOL ExpandUserMenuArguments(HWND msgParent, const char* name, const char* dosName, const char* varText,
                             char* buffer, int bufferLen, BOOL* fileNameUsed,
                             CUserMenuAdvancedData* userMenuAdvancedData,
                             BOOL ignoreEnvVarNotFoundOrTooLong)
{
    CALL_STACK_MESSAGE4("ExpandUserMenuArguments(, %s, %s, %s, , ,)", name, dosName,
                        strlen(varText) > 300 ? "(too long)" : varText);

    CExecuteExpData data;
    data.Name = name;
    data.DosName = dosName;
    data.FileNameUsed = fileNameUsed;
    data.UserMenuAdvancedData = userMenuAdvancedData;
    return ExpandVarString(msgParent, varText, buffer, bufferLen, UserMenuArgsExpArray,
                           &data, ignoreEnvVarNotFoundOrTooLong);
}

BOOL ValidateCommandFile(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2)
{
    CALL_STACK_MESSAGE2("ValidateCommandFile(, %s, ,)", varText);

    if (!ValidatePathIsNotEmpty(msgParent, varText))
    {
        // edit line je prazdna nebo plna mezer -- vyberem vse
        errorPos1 = 0;
        errorPos2 = -1;
        return FALSE;
    }

    return ValidateVarString(msgParent, varText, errorPos1, errorPos2, CommandExpArray);
}

BOOL ValidateHotPath(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2)
{
    CALL_STACK_MESSAGE2("ValidateHotPath(, %s, ,)", varText);

    if (!ValidatePathIsNotEmpty(msgParent, varText))
    {
        // edit line je prazdna nebo plna mezer -- vyberem vse
        errorPos1 = 0;
        errorPos2 = -1;
        return FALSE;
    }

    return ValidateVarString(msgParent, varText, errorPos1, errorPos2, HotPathExpArray);
}

BOOL ValidateArguments(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2)
{
    CALL_STACK_MESSAGE2("ValidateArguments(, %s, ,)", varText);
    return ValidateVarString(msgParent, varText, errorPos1, errorPos2, ArgumentsExpArray);
}

BOOL ExpandArguments(HWND msgParent, const char* name, const char* dosName, const char* varText,
                     char* buffer, int bufferLen, BOOL* fileNameUsed)
{
    CALL_STACK_MESSAGE4("ExpandArguments(, %s, %s, %s, , ,)", name, dosName, varText);
    CExecuteExpData data;
    data.Name = name;
    data.DosName = dosName;
    data.FileNameUsed = fileNameUsed;
    return ExpandVarString(msgParent, varText, buffer, bufferLen, ArgumentsExpArray, &data);
}

BOOL ValidateInitDir(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2)
{
    CALL_STACK_MESSAGE2("ValidateInitDir(, %s, ,)", varText);
    if (*varText == 0)
    {
        SalMessageBox(msgParent, LoadStr(IDS_EXP_EMPTYSTR), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        errorPos1 = errorPos2 = 0;
        return FALSE;
    }
    return ValidateVarString(msgParent, varText, errorPos1, errorPos2, InitDirExpArray);
}

BOOL ValidateInfoLineItems(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2)
{
    CALL_STACK_MESSAGE2("ValidateInfoLineItems(, %s, ,)", varText);
    return ValidateVarString(msgParent, varText, errorPos1, errorPos2,
                             GetInfoLineExpArray(TRUE /* pro validaci neni rozdil mezi TRUE a FALSE */));
}

BOOL ExpandInfoLineItems(HWND msgParent, const char* varText, CPluginDataInterfaceEncapsulation* pluginData,
                         CFileData* fData, BOOL isDir, char* buffer, int bufferLen, DWORD* varPlacements,
                         int* varPlacementsCount, DWORD validFileData, BOOL isDisk)
{
    CALL_STACK_MESSAGE1("ExpandInfoLineItems()");
    CFileDataExpData data;
    data.PluginData = pluginData;
    data.FileData = fData;
    data.IsDir = isDir;
    data.ValidFileData = validFileData;
    data.Path[0] = 0;
    return ExpandVarString(msgParent, varText, buffer, bufferLen, GetInfoLineExpArray(isDisk),
                           &data, TRUE, varPlacements, varPlacementsCount);
}

BOOL ValidateMakeFileList(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2)
{
    CALL_STACK_MESSAGE2("ValidateMakeFileList(, %s, ,)", varText);
    return ValidateVarString(msgParent, varText, errorPos1, errorPos2, MakeFileListExpArray);
}

BOOL ExpandMakeFileList(HWND msgParent, const char* varText, CPluginDataInterfaceEncapsulation* pluginData,
                        CFileData* fData, BOOL isDir, char* buffer, int bufferLen, BOOL detectMaxVarSizes,
                        int* maxVarSizes, int maxVarSizesCount, DWORD validFileData, const char* path,
                        BOOL ignoreEnvVarNotFoundOrTooLong)
{
    CALL_STACK_MESSAGE1("ExpandMakeFileList()");
    CFileDataExpData data;
    data.PluginData = pluginData;
    data.FileData = fData;
    data.IsDir = isDir;
    data.ValidFileData = validFileData;
    strcpy(data.Path, path);
    return ExpandVarString(msgParent, varText, buffer, bufferLen, MakeFileListExpArray, &data,
                           ignoreEnvVarNotFoundOrTooLong, NULL, NULL, detectMaxVarSizes,
                           maxVarSizes, maxVarSizesCount);
}

BOOL RemoveDoubleBackslahesFromPath(char* text)
{
    if (text == NULL)
    {
        TRACE_E("Unexpected situation in RemoveDoubleBackslahesFromPath().");
        return FALSE;
    }
    int len = (int)strlen(text);
    if (len < 3)
        return TRUE;
    char* s = text + 2; // UNC cesty maji na zacatku "\\"
    char* d = s;
    while (*s != 0)
    {
        if (*s == '\\' && *(s + 1) == '\\')
            s++;
        *d = *s;
        s++;
        d++;
    }
    *d = 0;
    return TRUE;
}

BOOL ExpandInitDir(HWND msgParent, const char* name, const char* dosName, const char* varText,
                   char* buffer, int bufferLen, BOOL ignoreEnvVarNotFoundOrTooLong)
{
    CALL_STACK_MESSAGE4("ExpandInitDir(, %s, %s, %s, ,)", name, dosName, varText);
    CExecuteExpData data;
    data.Name = name;
    data.DosName = dosName;
    data.FileNameUsed = NULL;
    if (ExpandVarString(msgParent, varText, buffer, bufferLen, InitDirExpArray,
                        &data, ignoreEnvVarNotFoundOrTooLong))
    {
        // promenne $() z Initial Directory sice normalne nejsou zakonceny zpetnym lomitkem,
        // ale pokud vedou do rootu, pak jsou; potom se cesta "$(SalDir)\Editor" prevede na
        // "C:\\Editor", takze dostaneme dve zpetna lomitka
        RemoveDoubleBackslahesFromPath(buffer); // setreseme dvojita zpetna lomitka na jedno
        return TRUE;
    }
    else
        return FALSE;
}

BOOL ExpandCommand(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                   BOOL ignoreEnvVarNotFoundOrTooLong)
{
    CALL_STACK_MESSAGE2("ExpandCommand(, %s, , ,)", varText);
    CExecuteExpData data;
    data.Name = NULL;
    data.DosName = NULL;
    data.FileNameUsed = NULL;
    data.UserMenuAdvancedData = NULL;
    if (ExpandVarString(msgParent, varText, buffer, bufferLen, CommandExpArray, &data,
                        ignoreEnvVarNotFoundOrTooLong))
    {
        // promenne EXECUTE_WINDIR, EXECUTE_SYSDIR a EXECUTE_SALDIR jsou zakonceny zpetnym lomitkem
        // uzivatel doplni vlastni zpetne lomitko, takze jsou ve vysledku v ceste dve
        RemoveDoubleBackslahesFromPath(buffer); // setreseme dvojita zpetna lomitka na jedno
        return TRUE;
    }
    else
        return FALSE;
}

BOOL ExpandHotPath(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                   BOOL ignoreEnvVarNotFoundOrTooLong)
{
    CALL_STACK_MESSAGE2("ExpandHotPath(, %s, , ,)", varText);
    CExecuteExpData data;
    data.Name = NULL;
    data.DosName = NULL;
    data.FileNameUsed = NULL;
    data.UserMenuAdvancedData = NULL;
    if (ExpandVarString(msgParent, varText, buffer, bufferLen, HotPathExpArray, &data,
                        ignoreEnvVarNotFoundOrTooLong))
    {
        // promenne EXECUTE_WINDIR, EXECUTE_SYSDIR a EXECUTE_SALDIR jsou zakonceny zpetnym lomitkem
        // uzivatel doplni vlastni zpetne lomitko, takze jsou ve vysledku v ceste dve
        RemoveDoubleBackslahesFromPath(buffer); // setreseme dvojita zpetna lomitka na jedno
        return TRUE;
    }
    else
        return FALSE;
}

const CExecuteItem*
TrackExecuteMenu(HWND hParent, int buttonResID, int editlineResID,
                 BOOL combobox, CExecuteItem* executeItems, int filterResID)
{
    CALL_STACK_MESSAGE4("TrackExecuteMenu(, %d, %d, %d)", buttonResID, editlineResID, filterResID);
    HWND hButton = GetDlgItem(hParent, buttonResID);
    if (hButton == NULL)
        TRACE_E("Child window was not found: buttonResID=" << buttonResID);
    HWND hEdit = NULL;
    CComboboxEdit* comboEdit = NULL;
    if (combobox)
    {
        hEdit = GetWindow(GetDlgItem(hParent, editlineResID), GW_CHILD);
        if (hEdit != NULL)
        {
            comboEdit = (CComboboxEdit*)WindowsManager.GetWindowPtr(hEdit);
            if (comboEdit == NULL)
            {
                TRACE_E("CComboboxEdit was not found: editlineResID=" << editlineResID);
                return NULL;
            }
        }
    }
    else
        hEdit = GetDlgItem(hParent, editlineResID);
    if (hEdit == NULL)
    {
        TRACE_E("Child window was not found: editlineResID=" << editlineResID);
        return NULL;
    }

    RECT r;
    GetWindowRect(hButton, &r);
    POINT p;
    p.x = r.right;
    p.y = r.top;

    HMENU hMenu = CreatePopupMenu();
    CExecuteItem* item = executeItems;
    int i = 0;
    while (item[i].Keyword != EXECUTE_TERMINATOR)
    {
        if (item[i].Keyword == EXECUTE_SUBMENUSTART)
        {
            int subMenuIndex = i++;
            HMENU hSubMenu = CreatePopupMenu();
            while (item[i].Keyword != EXECUTE_SUBMENUEND && item[i].Keyword != EXECUTE_TERMINATOR)
            {
                if (item[i].Keyword == EXECUTE_SEPARATOR)
                    InsertMenu(hSubMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_SEPARATOR, 1, NULL);
                else
                    InsertMenu(hSubMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, (UINT_PTR)i + 1,
                               LoadStr(item[i].NameResID));
                i++;
            }
            InsertMenu(hMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hSubMenu,
                       LoadStr(item[subMenuIndex].NameResID));
        }
        else
        {
            if (item[i].Keyword == EXECUTE_SEPARATOR)
                InsertMenu(hMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_SEPARATOR, 1, NULL);
            else
                InsertMenu(hMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, (UINT_PTR)i + 1,
                           LoadStr(item[i].NameResID));
        }
        i++;
    }

    TPMPARAMS tpmPar;
    tpmPar.cbSize = sizeof(tpmPar);
    tpmPar.rcExclude = r;
    DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                 p.x, p.y, hParent, &tpmPar);
    DestroyMenu(hMenu);
    item = NULL;
    if (cmd != 0)
    {
        item = &executeItems[cmd - 1];
        char buff[255];
        if (item->Keyword == EXECUTE_BROWSE)
        {
            BrowseCommand(hParent, editlineResID, filterResID);
            return item;
        }
        if (item->Keyword == EXECUTE_BROWSEDIR)
        {
            BrowseDirCommand(hParent, editlineResID, filterResID); // JRFIXME
            return item;
        }
        if (item->Keyword == EXECUTE_HELP)
            return item;

        if (item->Flags & EIF_VARIABLE)
            sprintf(buff, "$(%s)", item->Keyword);
        else
            sprintf(buff, "%s", item->Keyword);

        if (item->Flags & EIF_REPLACE_ALL)
        {
            SendMessage(hEdit, WM_SETTEXT, 0, (LPARAM)buff);
            SendMessage(hEdit, EM_SETSEL, lstrlen(buff), lstrlen(buff));
        }
        else
        {
            if (comboEdit != NULL)
                comboEdit->ReplaceText(buff);
            else
                SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM)buff);
        }
        if ((item->Flags & EIF_DONT_FOCUS) == 0)
        {
            // v pripade comba je treba napred nastavit focus
            DWORD start;
            DWORD end;
            if (comboEdit != NULL)
            {
                SendMessage(hEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
                SetFocus(hEdit);
            }
            if (item->Flags & EIF_CURSOR_1 || item->Flags & EIF_CURSOR_2)
            {
                int delta = 1;
                if (item->Flags & EIF_CURSOR_2)
                    delta = 2;
                if (delta > lstrlen(buff))
                {
                    TRACE_E("delta > strlen(buff)");
                    delta = (int)strlen(buff);
                }
                if (comboEdit == NULL)
                    SendMessage(hEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
                SendMessage(hEdit, EM_SETSEL, end - delta, end - delta);
            }
            else if (comboEdit != NULL)
                SendMessage(hEdit, EM_SETSEL, end, end);
            if (comboEdit == NULL)
                SetFocus(hEdit);
            // default by zustal u nas -- vratime ho dialogu
            SendMessage(hButton, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
            HWND hDialog = hParent;
            DWORD dlgStyle;
            do
            {
                dlgStyle = (DWORD)GetWindowLongPtr(hDialog, GWL_STYLE);
                if (dlgStyle & DS_CONTROL)
                {
                    HWND hPar = GetParent(hDialog);
                    if (hPar == NULL)
                        break;
                    hDialog = hPar;
                }
            } while (dlgStyle & DS_CONTROL);
            DWORD defID = (DWORD)SendMessage(hDialog, DM_GETDEFID, 0, 0);
            if (HIWORD(defID) == DC_HASDEFID)
                SendMessage(GetDlgItem(hDialog, LOWORD(defID)), BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
        }
    }
    return item;
}

BOOL BrowseCommand(HWND hParent, int editlineResID, int filterResID)
{
    CALL_STACK_MESSAGE2("BrowseCommand(, %d)", editlineResID);
    char file[MAX_PATH];
    SendMessage(GetDlgItem(hParent, editlineResID), WM_GETTEXT,
                MAX_PATH, (LPARAM)file);
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hParent;
    char* s = LoadStr(filterResID);
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    //  ofn.lpstrFileTitle = file;
    //  ofn.nMaxFileTitle = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    CALL_STACK_MESSAGE1("BrowseCommand::GetOpenFileName");
    if (GetOpenFileName(&ofn))
    {
        if (SalGetFullName(file))
        {
            CALL_STACK_MESSAGE2("BrowseCommand::SendMessage( , , ,%s)", file);
            SendMessage(GetDlgItem(hParent, editlineResID), WM_SETTEXT, 0, (LPARAM)file);
            return TRUE;
        }
    }
    else
    {
        DWORD error = CommDlgExtendedError();
        if (error == FNERR_INVALIDFILENAME)
        {
            char buff[MAX_PATH + 100];
            sprintf(buff, LoadStr(IDS_COMDLG_INVALIDFILENAME), file);
            SalMessageBox(hParent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        }
    }
    return FALSE;
}
