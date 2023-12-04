// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <stdio.h>

#ifdef ZIP_DLL
#include <ostream>
#include <commctrl.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr\\comdefs.h"
#include "config.h"
#include "typecons.h"
//#include "zip.rh"
//#include "lang\lang.rh"
#include "zip.rh2"
#include "chicon.h"
#include "common.h"
#include "add_del.h"
#include "iosfxset.h"
#else //ZIP_DLL

#include "selfextr\\comdefs.h"
#include "typecons.h"
#include "iosfxset.h"
#include "killdbg.h"
#endif //ZIP_DLL

#define SFX_SET_CURRENTVERSION 2

//const char * SFX_HEAD = ";Open Salamander's ZIP Self-extractor Settings\r\n";
const char* SFX_VERSION = "version";
const char* SFX_TARGETDIR = "target_dir";
const char* SFX_ALLOWCHANGE = "allow_dir_change";
const char* SFX_REMOVETEMP = "remove_temp";
const char* SFX_WAITFOR = "wait_for";
const char* SFX_AUTOSTART = "autostart";
const char* SFX_SHOWSUMARY = "show_summary";
const char* SFX_HIDEMAINDLG = "hide_dlg";
const char* SFX_OVEWRITEALL = "ovewrite_all";
const char* SFX_AUTODIR = "create_target_dir";
const char* SFX_COMMAND = "command";
const char* SFX_PACKAGE = "sfxpackage";
const char* SFX_MBOXBUTTONS = "mbox_buttons";
const char* SFX_MBOXICON = "mbox_icon";
const char* SFX_MBOXTYPE = "mbox_type";
const char* SFX_MBOK = "ok";
const char* SFX_MBOKCANCEL = "ok_cancel";
const char* SFX_MBYESNO = "yes_no";
const char* SFX_AGREEDISAGREE = "agree_disagree";
const char* SFX_MBNOICON = "none";
const char* SFX_MBSIMPLEMBOX = "simple";
const char* SFX_MBEXCLAMATION = "exclamation";
const char* SFX_MBINFORMATION = "information";
const char* SFX_MBQUESTION = "question";
const char* SFX_MBLONGMESSAGE = "long_message";
const char* SFX_MBOXTEXT = "mbox_text";
const char* SFX_MBOXTITLE = "mbox_title";
const char* SFX_TEXT = "dlg_text";
const char* SFX_TITLE = "dlg_title";
const char* SFX_EXTRBTNTEXT = "button_text";
const char* SFX_VENDOR = "vendor";
const char* SFX_WWW = "link";
const char* SFX_ICONFILE = "icon_file";
const char* SFX_ICONINDEX = "icon_index";
const char* SFX_TDTEMP = "$(Temp)";
const char* SFX_TDPROGFILES = "$(ProgFiles)";
const char* SFX_TDWINDIR = "$(WinDir)";
const char* SFX_TDSYSDIR = "$(SysDir)";
const char* SFX_TDENVVAR = "$[]";
const char* SFX_TDREGVAL = "$<>";
const char* SFX_REQUIRESADMIN = "requires_administrative_privileges";

struct CHKeyDectript
{
    const char* Name;
    HKEY HKey;
};

CHKeyDectript HKeys[] =
    {
        {"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
        {"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
        {"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
        {"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
        {"HKEY_USERS", HKEY_USERS},
        {"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA},
        {"HKEY_DYN_DATA", HKEY_DYN_DATA},
        {NULL, 0}};

const char* SFX_PROGFILES_REGVAL =
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ProgramFilesDir";

// return values
// 0 strings are equal
// 1 strings are different

int StrNICmp(const char* str1, const char* str2, int max)
{
    CALL_STACK_MESSAGE_NONE
    int l1 = lstrlen(str1);
    if (l1 > max)
        l1 = max;
    int l2 = lstrlen(str2);
    if (l2 > max)
        l2 = max;
    return CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, str1, max, str2, max) - 2;
}

const char*
GetValue(char* buffer, DWORD size, const char* textData)
{
    CALL_STACK_MESSAGE_NONE
    const char* sour = textData;
    char* dest = buffer;
    while (*sour)
    {
        while (*sour == ' ' || *sour == '\r' || *sour == '\n')
            *sour++;
        if (*sour == ';')
            while (*sour != '\n' && *sour)
                *sour++;
        else
            break;
    }
    while (*sour != ' ' && *sour != '\r' && *sour != '=' && *sour && dest < buffer + size - 1)
    {
        *dest++ = *sour++;
    }
    while (*sour != '=' && *sour)
        *sour++;
    if (*sour == '=')
        sour++;
    *dest = 0;
    return sour;
}

const char*
GetNumber(int* number, const char* textData)
{
    CALL_STACK_MESSAGE_NONE
    const char* sour = textData;
    while (*sour)
    {
        while (*sour == ' ' || *sour == '\r' || *sour == '\n')
            *sour++;
        if (*sour == ';')
            while (*sour != '\n' && *sour)
                *sour++;
        else
            break;
    }
    char buffer[50];
    char* dest = buffer;
    if (*sour == '-')
        *dest++ = *sour++;
    while (isdigit(*sour) && *sour && dest < buffer + 49)
    {
        *dest++ = *sour++;
    }
    *dest = 0;
    char* endptr;
    *number = strtoul(buffer, &endptr, 10);
    return sour;
}

const char*
GetString(char* buffer, DWORD size, const char* textData)
{
    CALL_STACK_MESSAGE_NONE
    const char* sour = textData;
    char* dest = buffer;
    while (*sour)
    {
        while (*sour == ' ' || *sour == '\r' || *sour == '\n')
            *sour++;
        if (*sour == ';')
            while (*sour != '\n' && *sour)
                *sour++;
        else
            break;
    }
    if (*sour != '"')
    {
        buffer[0] = 0;
        return sour;
    }
    sour++;
    while (*sour && dest < buffer + size - 1)
    {
        if (*sour == '"' && *++sour != '"')
            break;
        /*{
      if (*++sour != '"') break;
    }*/
        *dest++ = *sour++;
    }
    *dest = 0;
    return sour;
}

const char*
GetStringAlloc(char*& buffer, const char* textData)
{
    CALL_STACK_MESSAGE_NONE
    const char* sour = textData;
    while (*sour)
    {
        while (*sour == ' ' || *sour == '\r' || *sour == '\n')
            *sour++;
        if (*sour == ';')
            while (*sour != '\n' && *sour)
                *sour++;
        else
            break;
    }
    if (*sour != '"')
    {
        if (buffer)
            free(buffer);
        buffer = NULL;
        return sour;
    }
    sour++;
    // spocitame si delku
    int len = 0;
    const char* save = sour;
    while (*sour)
    {
        if (*sour == '"' && *++sour != '"')
            break;
        /*{
      if (*++sour != '"') break;
    }*/
        sour++;
        len++;
    }
    // zkopirujeme string
    buffer = (char*)realloc(buffer, len + 1);
    char* dest = buffer;
    sour = save;
    while (*sour)
    {
        if (*sour == '"' && *++sour != '"')
            break;
        /*{
      if (*++sour != '"') break;
    }*/
        *dest++ = *sour++;
    }
    *dest = 0;
    return sour;
}

int GetRootPath(char* root, const char* path)
{
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        int len = (int)(s - path);
        memcpy(root, path, len);
        root[len] = '\\';
        root[len + 1] = 0;
        return len + 1;
    }
    else
    {
        root[0] = path[0];
        root[1] = ':';
        root[2] = '\\';
        root[3] = 0;
        return 3;
    }
}

void HandlePathRelativeToZip2Sfx(char* path, const char* zip2sfxDir)
{
    if (zip2sfxDir[0] != 0 &&
        !(path[0] != 0 && path[1] == ':' || path[0] == '\\' && path[1] == '\\'))
    { // relativni cesta + zname relativni cestu k zip2sfx.exe
        char joinedPath[MAX_PATH];
        if (path[0] == '\\')
            GetRootPath(joinedPath, zip2sfxDir);
        else
            lstrcpyn(joinedPath, zip2sfxDir, MAX_PATH);
        int len = (int)strlen(joinedPath);
        const char* pathAux = path[0] == '\\' ? path + 1 : path;
        if (strlen(pathAux) + len < MAX_PATH)
        {
            strcpy(joinedPath + len, pathAux);
            strcpy(path, joinedPath);
        }
        else
            path[0] = 0; // chyba, radsi nulujeme...
    }
}

// return values
//
// 0 OK
// spatne zadany target_dir:
//   1 bad temp
//   2 missing right parenthesis
//   3 unknown keyword in $()
//   4 bad key name
// 5 missing settings version
// 6 incorrect settings version
// 7 incorrect data format
// 8 agree disagree buttons used for classical message box

int ImportSFXSettings(const char* textData, CSfxSettings* settings, const char* zip2sfxDir)
{
    CALL_STACK_MESSAGE2("ImportSFXSettings(%s, )", textData);
    char buf1[128];
    const char* sour = textData;
    int i;
    int version = -1;
    unsigned mbButtons = 0;
    unsigned mbType = 0;
    while (*(sour = GetValue(buf1, 128, sour)))
    {
        if (lstrcmpi(buf1, SFX_VERSION) == 0)
        {
            sour = GetNumber(&version, sour);
            if (version != SFX_SET_CURRENTVERSION)
                return 6;
            continue;
        }
        if (lstrcmpi(buf1, SFX_TARGETDIR) == 0)
        {
            sour = GetString(settings->TargetDir, MAX_PATH, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_ALLOWCHANGE) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags &= ~SE_NOTALLOWCHANGE;
            else
                settings->Flags |= SE_NOTALLOWCHANGE;
            continue;
        }
        if (lstrcmpi(buf1, SFX_REMOVETEMP) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags |= SE_REMOVEAFTER;
            else
                settings->Flags &= ~SE_REMOVEAFTER;
            continue;
        }
        if (lstrcmpi(buf1, SFX_WAITFOR) == 0)
        {
            sour = GetString(settings->WaitFor, MAX_PATH, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_AUTOSTART) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags |= SE_AUTO;
            else
                settings->Flags &= ~SE_AUTO;
            continue;
        }
        if (lstrcmpi(buf1, SFX_SHOWSUMARY) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags |= SE_SHOWSUMARY;
            else
                settings->Flags &= ~SE_SHOWSUMARY;
            continue;
        }
        if (lstrcmpi(buf1, SFX_HIDEMAINDLG) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags |= SE_HIDEMAINDLG;
            else
                settings->Flags &= ~SE_HIDEMAINDLG;
            continue;
        }
        if (lstrcmpi(buf1, SFX_OVEWRITEALL) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags |= SE_OVEWRITEALL;
            else
                settings->Flags &= ~SE_OVEWRITEALL;
            continue;
        }
        if (lstrcmpi(buf1, SFX_AUTODIR) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags |= SE_AUTODIR;
            else
                settings->Flags &= ~SE_AUTODIR;
            continue;
        }
        if (lstrcmpi(buf1, SFX_COMMAND) == 0)
        {
            sour = GetString(settings->Command, SE_MAX_COMMANDLINE, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_PACKAGE) == 0)
        {
            sour = GetString(settings->SfxFile, MAX_PATH, sour);
            HandlePathRelativeToZip2Sfx(settings->SfxFile, zip2sfxDir);
            continue;
        }
        if (lstrcmpi(buf1, SFX_MBOXBUTTONS) == 0)
        {
            char buf[50];
            sour = GetString(buf, 50, sour);
            if (lstrcmpi(buf, SFX_MBOK) == 0)
            {
                mbButtons = 0;
                continue;
            }
            if (lstrcmpi(buf, SFX_MBOKCANCEL) == 0)
            {
                mbButtons = 1;
                continue;
            }
            if (lstrcmpi(buf, SFX_MBYESNO) == 0)
            {
                mbButtons = 2;
                continue;
            }
            if (lstrcmpi(buf, SFX_AGREEDISAGREE) == 0)
            {
                mbButtons = 3;
                continue;
            }
        }
        if (lstrcmpi(buf1, SFX_MBOXICON) == 0 || lstrcmpi(buf1, SFX_MBOXTYPE) == 0)
        {
            char buf[50];
            sour = GetString(buf, 50, sour);
            if (lstrcmpi(buf, SFX_MBNOICON) == 0 || lstrcmpi(buf, SFX_MBSIMPLEMBOX) == 0)
            {
                mbType = 0;
                continue;
            }
            if (lstrcmpi(buf, SFX_MBEXCLAMATION) == 0)
            {
                mbType = 1;
                continue;
            }
            if (lstrcmpi(buf, SFX_MBINFORMATION) == 0)
            {
                mbType = 2;
                continue;
            }
            if (lstrcmpi(buf, SFX_MBQUESTION) == 0)
            {
                mbType = 3;
                continue;
            }
            if (lstrcmpi(buf, SFX_MBLONGMESSAGE) == 0)
            {
                mbType = 4;
                continue;
            }
        }
        if (lstrcmpi(buf1, SFX_MBOXTEXT) == 0)
        {
            sour = GetStringAlloc(settings->MBoxText, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_MBOXTITLE) == 0)
        {
            sour = GetString(settings->MBoxTitle, SE_MAX_TITLE, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_TEXT) == 0)
        {
            sour = GetString(settings->Text, SE_MAX_TEXT, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_TITLE) == 0)
        {
            sour = GetString(settings->Title, SE_MAX_TITLE, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_EXTRBTNTEXT) == 0)
        {
            sour = GetString(settings->ExtractBtnText, SE_MAX_EXTRBTN, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_VENDOR) == 0)
        {
            sour = GetString(settings->Vendor, SE_MAX_VENDOR, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_WWW) == 0)
        {
            sour = GetString(settings->WWW, SE_MAX_WWW, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_ICONFILE) == 0)
        {
            sour = GetString(settings->IconFile, MAX_PATH, sour);
            HandlePathRelativeToZip2Sfx(settings->IconFile, zip2sfxDir);
            continue;
        }
        if (lstrcmpi(buf1, SFX_ICONINDEX) == 0)
        {
            sour = GetNumber((int*)&settings->IconIndex, sour);
            continue;
        }
        if (lstrcmpi(buf1, SFX_REQUIRESADMIN) == 0)
        {
            sour = GetNumber(&i, sour);
            if (i)
                settings->Flags |= SE_REQUIRESADMIN;
            else
                settings->Flags &= ~SE_REQUIRESADMIN;
            continue;
        }
        return 7;
    }
    if (version == -1)
        return 5;
    //check settings
    switch (mbType)
    {
    case 0:
        settings->MBoxStyle = 0;
        break;
    case 1:
        settings->MBoxStyle = MB_ICONEXCLAMATION;
        break;
    case 2:
        settings->MBoxStyle = MB_ICONINFORMATION;
        break;
    case 3:
        settings->MBoxStyle = MB_ICONQUESTION;
        break;
    case 4:
        settings->MBoxStyle = SE_LONGMESSAGE;
        break;
    }
    if (mbType == 4)
    {
        switch (mbButtons)
        {
        case 0:
            settings->MBoxStyle = SE_MBOK;
            break;
        case 1:
            settings->MBoxStyle = SE_MBOKCANCEL;
            break;
        case 2:
            settings->MBoxStyle = SE_MBYESNO;
            break;
        case 3:
            settings->MBoxStyle = SE_MBAGREEDISAGREE;
            break;
        }
    }
    else
    {
        switch (mbButtons)
        {
        case 0:
            settings->MBoxStyle |= MB_OK;
            break;
        case 1:
            settings->MBoxStyle |= MB_OKCANCEL;
            break;
        case 2:
            settings->MBoxStyle |= MB_YESNO;
            break;
        case 3:
            return 8;
        }
    }
    //orezeme mezery na konci
    char* iterator = settings->TargetDir + lstrlen(settings->TargetDir);
    while (--iterator >= settings->TargetDir && *iterator == ' ')
        ;
    iterator[1] = 0;
    unsigned targetDir;
    int ret = ParseTargetDir(settings->TargetDir, &targetDir, NULL, NULL, NULL, NULL);
    if (ret)
        return LOWORD(ret);
    if (targetDir != SE_TEMPDIREX)
        settings->Flags &= ~SE_REMOVEAFTER;
    //if (settings->Flags & SE_AUTO) settings->Flags |= SE_NOTALLOWCHANGE;
    if (!(settings->Flags & SE_AUTO))
        settings->Flags &= ~SE_HIDEMAINDLG;
    return 0;
}

// return valueas
//
// lower word is one of the following values:
//
// 0 OK
// 1 bad temp
// 2 missing right parenthesis
// 3 unknown keyword in $()
// 4 bad key name
//
// higher word contains index of where the syntax error occured in the target dir string

DWORD
ParseTargetDir(const char* path, unsigned* targetDir, const char** subDir,
               const char** dirSpecLeft, const char** dirSpecRight, HKEY* Key)
{
    CALL_STACK_MESSAGE2("ParseTargetDir(%s, , , , , )", path);
    //nastavime defaultni hodnoty
    if (targetDir)
        *targetDir = SE_CURDIR;
    if (subDir)
        *subDir = path;
    if (dirSpecLeft)
        *dirSpecLeft = path; // to jen tak pro jistotu abych tam strcil platny pointer
    if (dirSpecRight)
        *dirSpecRight = path;
    if (Key)
        *Key = NULL;

    //nechybi nam prava zavorka?
    if (path[0] == '$')
    {
        char right = 0;
        switch (path[1])
        {
        case '(':
            right = ')';
            break;
        case '[':
            right = ']';
            break;
        case '<':
            right = '>';
            break;
        }
        if (right)
        {
            int count = 1;
            const char* iterator = path + 2;
            while (*iterator)
            {
                if (*iterator == right)
                {
                    if (!--count)
                        break;
                }
                else
                {
                    if (*iterator == path[1])
                        count++;
                }
                iterator++;
            }
            if (count)
            {
                return 2 + (DWORD)((iterator - path) << 16); //chybi nam zavorka
            }

            // jeste otestujem jestli mame spravny string v zavorce
            if (iterator - path + 2 <= 0)
            {
                return 3 + (2 << 16); // prazdna zavorka
            }
            if (path[1] == '(')
            {
                if (StrNICmp(path, SFX_TDTEMP, (int)(iterator - path)) == 0)
                {
                    if (iterator[1] != 0)
                    {
                        return 1 + (DWORD)(((iterator - path) + 1) << 16); //za $(Temp) jiz nesmi nic byt
                    }
                    if (targetDir)
                        *targetDir = SE_TEMPDIREX;
                    goto L_KEYS_OK;
                }
                if (StrNICmp(path, SFX_TDPROGFILES, (int)(iterator - path)) == 0)
                {
                    if (targetDir)
                        *targetDir = SE_REGVALUE;
                    if (Key)
                        *Key = HKEY_LOCAL_MACHINE;
                    if (dirSpecLeft)
                        *dirSpecLeft = SFX_PROGFILES_REGVAL;
                    if (dirSpecRight)
                        *dirSpecRight = SFX_PROGFILES_REGVAL + lstrlen(SFX_PROGFILES_REGVAL);
                    goto L_KEYS_OK;
                }
                if (StrNICmp(path, SFX_TDWINDIR, (int)(iterator - path)) == 0)
                {
                    if (targetDir)
                        *targetDir = SE_WINDIR;
                    goto L_KEYS_OK;
                }
                if (StrNICmp(path, SFX_TDSYSDIR, (int)(iterator - path)) == 0)
                {
                    if (targetDir)
                        *targetDir = SE_SYSDIR;
                    goto L_KEYS_OK;
                }

                return 3 + (2 << 16); // spatny keyword
            }
            else // popripade naplnime dirSpec
            {
                if (path[1] == '[')
                {
                    if (targetDir)
                        *targetDir = SE_ENVVAR;
                    if (dirSpecLeft)
                        *dirSpecLeft = path + 2;
                    if (dirSpecRight)
                        *dirSpecRight = iterator;
                }
                if (path[1] == '<')
                {
                    path += 2;
                    int i, l;
                    for (i = 0; HKeys[i].Name; i++)
                    {
                        l = lstrlen(HKeys[i].Name);
                        if (StrNICmp(path, HKeys[i].Name, l) == 0 &&
                            (path[l] == '>' || path[l] == '\\'))
                            break;
                    }
                    if (!HKeys[i].Name)
                        return 4 + (2 << 16); // spatne jmeno klice

                    if (targetDir)
                        *targetDir = SE_REGVALUE;
                    if (Key)
                        *Key = HKeys[i].HKey;
                    if (dirSpecLeft)
                    {
                        *dirSpecLeft = path + l;
                        if (**dirSpecLeft == '\\')
                            (*dirSpecLeft)++;
                    }
                    if (dirSpecRight)
                        *dirSpecRight = iterator;
                }
            }

        L_KEYS_OK:

            if (subDir)
            {
                *subDir = iterator + 1;
                if (**subDir == '\\')
                    (*subDir)++;
            }
        }
    }

    return 0;
}

#ifdef ZIP_DLL

char* ExportString(char* buffer, const char* string)
{
    CALL_STACK_MESSAGE_NONE
    const char* sour = string;
    char* dest = buffer;
    while (*sour)
    {
        if (*sour == '"')
            *dest++ = '"';
        *dest++ = *sour++;
    }
    *dest = 0;
    return buffer;
}

BOOL CZipPack::ExportLongString(CFile* outFile, const char* string)
{
    CALL_STACK_MESSAGE_NONE
    const char* sour = string;
    while (*sour)
    {
        if (*sour == '"')
            if (Write(outFile, (void*)"\"", 1, NULL))
                return FALSE;
        if (Write(outFile, (void*)sour, 1, NULL))
            return FALSE;
        sour++;
    }
    return TRUE;
}

BOOL CZipPack::WriteSFXComment(CFile* outFile, CSfxSettingsComments comment)
{
    char buf[3000];
    buf[0] = 0;
    switch (comment)
    {
    case SFX_COMMENT_HEAD:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_HEAD1));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_HEAD2));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_HEAD3));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_HEAD4));
        break;

    case SFX_COMMENT_VERSION:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_VERSION));
        break;

    case SFX_COMMENT_TARGDIR:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_TARGDIR1));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_TARGDIR2));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_TARGDIR3));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_TARGDIR4));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_TARGDIR5));
        break;

    case SFX_COMMENT_ALLOWCHANGE:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_ALLOWCHANGE));
        break;

    case SFX_COMMENT_REMOVE:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_REMOVE1));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_REMOVE2));
        break;

    case SFX_COMMENT_AUTO:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_AUTO));
        break;

    case SFX_COMMENT_SUMMARY:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_SUMMARY));
        break;

    case SFX_COMMENT_HIDE:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_HIDE));
        break;

    case SFX_COMMENT_OVERWRITE:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_OVERWRITE));
        break;

    case SFX_COMMENT_AUTODIR:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_AUTODIR));
        break;

    case SFX_COMMENT_COMMAND:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_COMMAND));
        break;

    case SFX_COMMENT_PACKAGE:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_PACKAGE));
        break;

    case SFX_COMMENT_MBUT:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_MBUT));
        break;

    case SFX_COMMENT_MICO:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_MICO));
        break;

    case SFX_COMMENT_MBOX:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_MBOX));
        break;

    case SFX_COMMENT_TEXT:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_TEXT));
        break;

    case SFX_COMMENT_TITLE:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_TITLE));
        break;

    case SFX_COMMENT_BUTTON:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_BUTTON));
        break;

    case SFX_COMMENT_VENDOR:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_VENDOR));
        break;

    case SFX_COMMENT_WWW:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_WWW));
        break;

    case SFX_COMMENT_ICOFILE:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_ICOFILE));
        break;

    case SFX_COMMENT_ICOINDEX:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_ICOINDEX));
        break;

    case SFX_COMMENT_WAITFOR:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_WAITFOR1));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_WAITFOR2));
        strcat(buf, LoadStr(IDS_SFX_COMMENT_WAITFOR3));
        break;

    case SFX_COMMENT_REQUIRESADMIN:
        strcat(buf, LoadStr(IDS_SFX_COMMENT_REQUIRESADMIN));
        break;
    }

    return Write(outFile, (void*)buf, (unsigned int)strlen(buf), NULL);
}

BOOL CZipPack::ExportSFXSettings(CFile* outFile, CSfxSettings* settings)
{
    CALL_STACK_MESSAGE1("CZipPack::ExportSFXSettings(, )");
    char buf1[2048];
    char buf2[2048];
    //header
    if (WriteSFXComment(outFile, SFX_COMMENT_HEAD))
        return FALSE;
    //version
    if (WriteSFXComment(outFile, SFX_COMMENT_VERSION))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_VERSION, SFX_SET_CURRENTVERSION);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_TARGDIR))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_TARGETDIR, ExportString(buf2, settings->TargetDir));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_ALLOWCHANGE))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_ALLOWCHANGE, settings->Flags & SE_NOTALLOWCHANGE ? 0 : 1);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_REMOVE))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_REMOVETEMP, settings->Flags & SE_REMOVEAFTER ? 1 : 0);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_WAITFOR))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_WAITFOR, ExportString(buf2, settings->WaitFor));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_AUTO))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_AUTOSTART, settings->Flags & SE_AUTO ? 1 : 0);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_SUMMARY))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_SHOWSUMARY, settings->Flags & SE_SHOWSUMARY ? 1 : 0);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_HIDE))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_HIDEMAINDLG, settings->Flags & SE_HIDEMAINDLG ? 1 : 0);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_OVERWRITE))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_OVEWRITEALL, settings->Flags & SE_OVEWRITEALL ? 1 : 0);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_AUTODIR))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_AUTODIR, settings->Flags & SE_AUTODIR ? 1 : 0);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_COMMAND))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_COMMAND, ExportString(buf2, settings->Command));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_PACKAGE))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_PACKAGE, ExportString(buf2, settings->SfxFile));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_MBUT))
        return FALSE;
    if ((int)settings->MBoxStyle < 0)
    {
        switch (settings->MBoxStyle)
        {
        case SE_MBOK:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_MBOK);
            break;
        case SE_MBOKCANCEL:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_MBOKCANCEL);
            break;
        case SE_MBYESNO:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_MBYESNO);
            break;
        case SE_MBAGREEDISAGREE:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_AGREEDISAGREE);
            break;
        }
    }
    else
    {
        switch (settings->MBoxStyle & 0x0F)
        {
        case MB_OK:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_MBOK);
            break;
        case MB_OKCANCEL:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_MBOKCANCEL);
            break;
        case MB_YESNO:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_MBYESNO);
            break;
        default:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXBUTTONS, SFX_MBOK);
            break;
        }
    }
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_MICO))
        return FALSE;
    if ((int)settings->MBoxStyle < 0)
    {
        sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXTYPE, SFX_MBLONGMESSAGE);
    }
    else
    {
        switch (settings->MBoxStyle & 0xF0)
        {
        case 0:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXTYPE, SFX_MBSIMPLEMBOX);
            break;
        case MB_ICONEXCLAMATION:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXTYPE, SFX_MBEXCLAMATION);
            break;
        case MB_ICONINFORMATION:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXTYPE, SFX_MBINFORMATION);
            break;
        case MB_ICONQUESTION:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXTYPE, SFX_MBQUESTION);
            break;
        default:
            sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXTYPE, SFX_MBSIMPLEMBOX);
            break;
        }
    }
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_MBOX))
        return FALSE;
    const char* str = settings->MBoxText ? settings->MBoxText : "";
    sprintf(buf1, "%s=\"", SFX_MBOXTEXT);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;
    if (!ExportLongString(outFile, str))
        return FALSE;
    if (Write(outFile, "\"\r\n", lstrlen("\"\r\n"), NULL))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_MBOXTITLE, ExportString(buf2, settings->MBoxTitle));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_TEXT))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_TEXT, ExportString(buf2, settings->Text));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_TITLE))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_TITLE, ExportString(buf2, settings->Title));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_BUTTON))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_EXTRBTNTEXT, ExportString(buf2, settings->ExtractBtnText));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_VENDOR))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_VENDOR, ExportString(buf2, settings->Vendor));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_WWW))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_WWW, ExportString(buf2, settings->WWW));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_ICOFILE))
        return FALSE;
    sprintf(buf1, "%s=\"%s\"\r\n", SFX_ICONFILE, ExportString(buf2, settings->IconFile));
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_ICOINDEX))
        return FALSE;
    sprintf(buf1, "%s=%u\r\n", SFX_ICONINDEX, settings->IconIndex);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (WriteSFXComment(outFile, SFX_COMMENT_REQUIRESADMIN))
        return FALSE;
    sprintf(buf1, "%s=%d\r\n", SFX_REQUIRESADMIN, settings->Flags & SE_REQUIRESADMIN ? 1 : 0);
    if (Write(outFile, buf1, lstrlen(buf1), NULL))
        return FALSE;

    if (Flush(outFile, outFile->OutputBuffer, outFile->BufferPosition, NULL))
        return FALSE;
    return TRUE;
}

#endif //ZIP_DLL
