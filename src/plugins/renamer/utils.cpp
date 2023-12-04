// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

void LoadHistory(HKEY regKey, const char* keyPattern, char** history,
                 char* buffer, int bufferSize, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE3("LoadHistory(, %s, , , %d, )", keyPattern, bufferSize);
    TCHAR buf[32];
    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        SalPrintf(buf, 32, keyPattern, i);
        if (!registry->GetValue(regKey, buf, REG_SZ, buffer, bufferSize))
            break;
        LPTSTR ptr = _tcsdup(buffer);
        if (!ptr)
            break;
        history[i] = ptr;
    }
}

void SaveHistory(HKEY regKey, const char* keyPattern, char** history, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE2("SaveHistory(, %s, , )", keyPattern);
    char buf[32];
    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        if (history[i] == NULL)
            break;
        SalPrintf(buf, 32, keyPattern, i);
        registry->SetValue(regKey, buf, REG_SZ, history[i], -1);
    }
}

BOOL FileError(HWND parent, const char* fileName, int error,
               BOOL retry, BOOL* skip, BOOL* skipAll, int title)
{
    CALL_STACK_MESSAGE_NONE
    int err = GetLastError();
    CALL_STACK_MESSAGE1("FileError()");

    char buffer[1024];
    strcpy(buffer, LoadStr(error));
    size_t len = strlen(buffer);
    if (err != NO_ERROR)
        SG->GetErrorText(err, buffer + len, (int)(1024 - len));

    if (skipAll && *skipAll)
    {
        if (skip)
            *skip = TRUE;
        return FALSE;
    }

    int ret;
    if (retry)
    {
        if (skip)
            ret = SG->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName, buffer, LoadStr(title));
        else
            ret = SG->DialogError(parent, BUTTONS_RETRYCANCEL, fileName, buffer, LoadStr(title));
    }
    else
    {
        if (skip)
            ret = SG->DialogError(parent, BUTTONS_SKIPCANCEL, fileName, buffer, LoadStr(title));
        else
            ret = SG->DialogError(parent, BUTTONS_OK, fileName, buffer, LoadStr(title));
    }

    switch (ret)
    {
    case DIALOG_RETRY:
        return TRUE;

    case DIALOG_SKIPALL:
        if (skipAll)
            *skipAll = TRUE;

    case DIALOG_SKIP:
        if (skip)
            *skip = TRUE;
        return FALSE;

    //case DIALOG_OK:
    //case DIALOG_CANCEL:
    default:
        if (skip)
            *skip = FALSE;
        return FALSE;
    }
}

char* GetFileData(const char* file, char* buffer)
{
    CALL_STACK_MESSAGE2("GetFileData(%s, )", file);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(file, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, GetLastError(),
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 100, NULL))
            buffer[0] = 0;
    }
    else
    {
        SYSTEMTIME st;
        FILETIME ft;
        FileTimeToLocalFileTime(&fd.ftLastWriteTime, &ft);
        FileTimeToSystemTime(&ft, &st);

        char number[50];
        char date[50], time[50];
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
            sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
            sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            strcpy(number, LoadStr(IDS_DIRTEXT));
        else
        {
            CQuadWord s(fd.nFileSizeLow, fd.nFileSizeHigh);
            SG->NumberToStr(number, s);
        }
        sprintf(buffer, "%s, %s, %s", number, date, time);
        FindClose(h);
    }
    return buffer;
}

BOOL FileOverwrite(HWND parent, const char* fileName1, const char* fileData1,
                   const char* fileName2, const char* fileData2, DWORD attr,
                   int shquestion, int shtitle, BOOL* skip, DWORD* silent)
{
    CALL_STACK_MESSAGE8("FileOverwrite(, %s, %s, %s, %s, 0x%X, %d, %d, , )",
                        fileName1, fileData1, fileName2, fileData2, attr,
                        shquestion, shtitle);
    char buf1[100], buf2[100];
    if (!fileData1)
        fileData1 = GetFileData(fileName1, buf1);
    if (!fileData2)
        fileData2 = GetFileData(fileName2, buf2);

    if (!silent || !(*silent & SILENT_OVERWRITE_FILE_EXIST))
    {
        if (silent && (*silent & SILENT_SKIP_FILE_EXIST))
        {
            if (skip)
                *skip = TRUE;
            return FALSE;
        }

        int ret;
        if (skip)
            ret = SG->DialogOverwrite(parent, BUTTONS_YESALLSKIPCANCEL, fileName1, fileData1, fileName2, fileData2);
        else
            ret = SG->DialogOverwrite(parent, BUTTONS_YESNOCANCEL, fileName1, fileData1, fileName2, fileData2);

        switch (ret)
        {
        case DIALOG_ALL:
            if (silent)
                *silent |= SILENT_OVERWRITE_FILE_EXIST;

        case DIALOG_YES:
            break; // jeste musime overit hidden/system overwrite

        case DIALOG_SKIPALL:
            if (silent)
                *silent |= SILENT_SKIP_FILE_EXIST;

        case DIALOG_SKIP:
            if (skip)
                *skip = TRUE;
            return FALSE;

            //case DIALOG_OK:
            //case DIALOG_CANCEL:
        default:
            if (skip)
                *skip = FALSE;
            return FALSE;
        }
    }

    // jeste overime, zda neprepisujeme hidden/system adresar
    if (attr == -1)
        attr = SG->SalGetFileAttributes(fileName1);
    if ((attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) == 0)
        return TRUE;

    if (silent && (*silent & SILENT_OVERWRITE_FILE_SYSHID))
    {
        return TRUE;
    }

    if (silent && (*silent & SILENT_SKIP_FILE_SYSHID))
    {
        if (skip)
            *skip = TRUE;
        return FALSE;
    }

    int ret;
    if (skip)
        ret = SG->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, fileName1, LoadStr(shquestion), LoadStr(shtitle));
    else
        ret = SG->DialogQuestion(parent, BUTTONS_YESNOCANCEL, fileName1, LoadStr(shquestion), LoadStr(shtitle));

    switch (ret)
    {
    case DIALOG_ALL:
        if (silent)
            *silent |= SILENT_OVERWRITE_FILE_SYSHID;

    case DIALOG_YES:
        return TRUE;

    case DIALOG_SKIPALL:
        if (silent)
            *silent |= SILENT_SKIP_FILE_SYSHID;

    case DIALOG_SKIP:
        if (skip)
            *skip = TRUE;
        return FALSE;

        //case DIALOG_OK:
        //case DIALOG_CANCEL:
    default:
        if (skip)
            *skip = FALSE;
        return FALSE;
    }
}

// ****************************************************************************
//
// CBuffer
//

BOOL CBuffer::Reserve(size_t size)
{
    CALL_STACK_MESSAGE2("CBuffer::Reserve(%Iu)", size);
    if (size > Allocated)
    {
        size_t s = max(Allocated * 2, size);
        if (Persistent)
        {
            void* ptr = realloc(Buffer, s);
            if (!ptr)
                return FALSE;
            Buffer = ptr;
        }
        else
        {
            Release();
            Buffer = malloc(s);
            if (!Buffer)
                return FALSE;
        }
        Allocated = s;
    }
    return TRUE;
}

// ****************************************************************************

CGUIMenuPopupAbstract*
CreateVarStrHelpMenu(CVarStrHelpMenuItem* helpMenu, int& id)
{
    CALL_STACK_MESSAGE2("CreateVarStrHelpMenu(, %d)", id);
    CGUIMenuPopupAbstract* ret = SalGUI->CreateMenuPopup();
    if (!ret)
        return NULL;

    MENU_ITEM_INFO mii;
    int i;
    for (i = 0; helpMenu[i].MenuItemStringID; i++)
    {
        if (helpMenu[i].MenuItemStringID == -1)
        {
            mii.Mask = MENU_MASK_TYPE;
            mii.Type = MNTT_SP;
        }
        else
        {
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_SUBMENU |
                       MENU_MASK_STRING | MENU_MASK_CUSTOMDATA;
            mii.Type = MNTT_IT;
            mii.ID = id++;
            mii.SubMenu = helpMenu[i].SubMenu ? CreateVarStrHelpMenu(helpMenu[i].SubMenu, id) : NULL;
            mii.String = LoadStr(helpMenu[i].MenuItemStringID);
            mii.CustomData = (ULONG_PTR)&helpMenu[i];
        }
        ret->InsertItem(i, TRUE, &mii);
    }

    return ret;
}

CVarStrHelpMenuItem*
FindItem(CGUIMenuPopupAbstract* menu, DWORD id)
{
    CALL_STACK_MESSAGE2("FindItem(, 0x%X)", id);
    MENU_ITEM_INFO mii;
    int count = menu->GetItemCount();
    int i;
    for (i = 0; i < count; i++)
    {
        mii.Mask = MENU_MASK_CUSTOMDATA | MENU_MASK_ID | MENU_MASK_SUBMENU;
        if (menu->GetItemInfo(i, TRUE, &mii))
        {
            if (mii.ID == id)
                return (CVarStrHelpMenuItem*)mii.CustomData;
            if (mii.SubMenu)
            {
                CVarStrHelpMenuItem* ret = FindItem(mii.SubMenu, id);
                if (ret)
                    return ret;
            }
        }
    }
    return NULL;
}

BOOL SelectVarStrVariable(HWND parent, int x, int y,
                          CVarStrHelpMenuItem* helpMenu, char* buffer, BOOL varStr)
{
    CALL_STACK_MESSAGE4("SelectVarStrVariable(, %d, %d, , , %d)", x, y, varStr);
    int id = 1;
    CGUIMenuPopupAbstract* menu = CreateVarStrHelpMenu(helpMenu, id);

    if (!menu)
        return FALSE;

    BOOL ret = FALSE;
    DWORD cmd = menu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_NONOTIFY,
                            x, y, parent, NULL);
    if (cmd > 0)
    {
        CVarStrHelpMenuItem* item = FindItem(menu, cmd);
        const char *strStart = "", *strEnd = "";
        if (varStr)
        {
            strStart = "$(";
            strEnd = ")";
        }
        if (item)
        {
            char param[1024];
            if (item->FParameterGetValue)
            {
                if (item->FParameterGetValue(parent, param))
                {
                    if (item->VariableName)
                    {
                        if (strlen(param))
                            SalPrintf(buffer, 1024, "%s%s:%s%s", strStart, item->VariableName, param, strEnd);
                        else
                            SalPrintf(buffer, 1024, "%s%s%s", strStart, item->VariableName, strEnd);
                    }
                    else
                        SalPrintf(buffer, 1024, "%s%s%s", strStart, param, strEnd);
                    ret = TRUE;
                }
            }
            else
            {
                SalPrintf(buffer, 1024, "%s%s%s", strStart, item->VariableName, strEnd);
                ret = TRUE;
            }
        }
    }

    SalGUI->DestroyMenuPopup(menu);

    return ret;
}

// ****************************************************************************

const char*
StrQChr(const char* start, const char* end, char q, char c)
{
    CALL_STACK_MESSAGE_NONE
    BOOL quote = FALSE;
    while (start != end)
    {
        if (*start == q)
            quote = !quote;
        if (*start == c && !quote)
            break;
        start++;
    }
    return start;
}

BOOL IsValidInt(const char* begin, const char* end, BOOL isSigned)
{
    CALL_STACK_MESSAGE_NONE
    if (isSigned && *begin == '-' && begin < end)
        begin++;
    if (begin >= end)
        return FALSE;
    while (begin < end)
    {
        if (!IsDigit(*begin))
            return FALSE;
        begin++;
    }
    return TRUE;
}

BOOL IsValidFloat(const char* begin, const char* end)
{
    CALL_STACK_MESSAGE_NONE
    if (*begin == '-' && begin < end)
        begin++;
    if (begin >= end ||
        end - begin == 1 && *begin == '.')
        return FALSE;

    // cela cast
    while (begin < end)
    {
        if (!IsDigit(*begin))
            break;
        begin++;
    }
    if (begin < end)
    {
        // desetina tecka
        if (*begin != '.')
            return FALSE;

        // desetina cast
        while (begin < end)
        {
            if (!IsDigit(*begin))
                FALSE;
            begin++;
        }
    }
    return TRUE;
}

int GetRegExpErrorID(CRegExpErrors err)
{
    CALL_STACK_MESSAGE_NONE
    switch (err)
    {
    case reeNoError:
        return IDS_REENOERROR;
    case reeLowMemory:
        return IDS_REELOWMEMORY;
    case reeEmpty:
        return IDS_REEEMPTY;
    case reeTooBig:
        return IDS_REETOOBIG;
    case reeTooManyParenthesises:
        return IDS_REETOOMANYPARENTHESISES;
    case reeUnmatchedParenthesis:
        return IDS_REEUNMATCHEDPARENTHESIS;
    case reeOperandCouldBeEmpty:
        return IDS_REEOPERANDCOULDBEEMPTY;
    case reeNested:
        return IDS_REENESTED;
    case reeUnmatchedBracket:
        return IDS_REEUNMATCHEDBRACKET;
    case reeFollowsNothing:
        return IDS_REEFOLLOWSNOTHING;
    case reeTrailingBackslash:
        return IDS_REETRAILINGBACKSLASH;
    case reeInternalDisaster:
        return IDS_REEINTERNALDISASTER;
    case reeExpectingExtendedPattern1:
        return IDS_REEEXPECTINGEXTENDEDPATTERN1;
    case reeExpectingExtendedPattern2:
        return IDS_REEEXPECTINGEXTENDEDPATTERN2;
    case reeInvalidPosixClass:
        return IDS_REEINVALIDPOSIXCLASS;
    case reeExpectingXDigit:
        return IDS_REEEXPECTINGXDIGIT;
    case reeStackOverflow:
        return IDS_REESTACKOVERFLOW;
    case reeNoPattern:
        return IDS_REENOPATTERN;
    case reeErrorStartingThread:
        return IDS_REEERRORSTARTINGTHREAD;
    case reeBadFixedWidthLookBehind:
        return IDS_REEBADFIXEDWIDTHLOOKBEHIND;
    default:
        return IDS_ERROR;
    }
}

char* StripRoot(char* path, int rootLen)
{
    CALL_STACK_MESSAGE_NONE
    char* ret = path + rootLen;
    if (*ret == '\\' && rootLen)
        ret++;
    return ret;
}

BOOL IsValidFileNameComponent(const char* start, const char* end)
{
    CALL_STACK_MESSAGE_NONE
    // ignorujeme tecky na konci jmena
    while (end > start && end[-1] == '.')
        end--;
    char save = *end;
    *(char*)end = 0;
    BOOL ret = SG->SalIsValidFileNameComponent(start);
    *(char*)end = save;
    return ret;
}

BOOL IsValidRelativePath(const char* name, int len)
{
    CALL_STACK_MESSAGE_NONE
    const char* nameEnd = name + len;
    const char* start = name;
    const char* end;
    do
    {
        end = GetNextPathComponent(start);
        if (!IsValidFileNameComponent(start, end))
            return FALSE;
        start = end + 1;
    } while (start <= nameEnd);
    return TRUE;
}

BOOL IsValidFullPath(const char* name, int len)
{
    CALL_STACK_MESSAGE_NONE
    int i = 0;

    // validujem drive/unc root
    if (name[0] == '\\' && name[1] == '\\') // UNC
    {
        i += 2;
        // server name
        if (name[i] == '\\')
            return FALSE;
        while (name[i] != 0 && name[i] != '\\')
            i++;
        if (name[i] != '\\')
            return FALSE;
        i++;
        // share name
        if (name[i] == '\\')
            return FALSE;
        while (name[i] != 0 && name[i] != '\\')
            i++;
        if (name[i] != '\\')
            return FALSE;
        i++;
    }
    else
    {
        if (!IsAlpha(name[0]) || name[1] != ':' || name[2] != '\\')
            return FALSE;
        i = 3;
    }

    return IsValidRelativePath(name + i, len - i);
}

BOOL ValidateFileName(const char* name, int len, CRenameSpec spec,
                      BOOL* skip, BOOL* skipAll)
{
    CALL_STACK_MESSAGE_NONE
    int err = 0;
    switch (spec)
    {
    case rsFileName:
        if (!IsValidFileNameComponent(name, name + len))
            err = IDS_NOTVALID_FILENAME;
        break;
    case rsRelativePath:
        if (!IsValidRelativePath(name, len))
            err = IDS_NOTVALID_RELATIVEPATH;
        break;
    case rsFullPath:
        if (!IsValidFullPath(name, len))
            err = IDS_NOTVALID_FULLPATH;
        break;
    }
    return err == 0 ||
           skip != NULL && FileError(GetParent(), name, err, FALSE, skip, skipAll, IDS_ERROR);
}

int CutTrailingDots(char* name, int len, CRenameSpec spec)
{
    CALL_STACK_MESSAGE_NONE
    char* nameEnd = name + len;
    // preskocime root
    if (spec == rsFullPath)
    {
        if (name[0] == '\\' && name[1] == '\\') // UNC
        {
            name += 2;
            while (*name != 0 && *name != '\\')
                name++;
            if (*name != 0)
                name++; // '\\'
            while (*name != 0 && *name != '\\')
                name++;
            name++;
        }
        else
            name += 3;
    }
    char *start = nameEnd, *end;
    while (start > name)
    {
        end = start;
        while (start > name && start[-1] == '.')
            start--;
        memmove(start, end, nameEnd - end + 1);
        len -= (int)(end - start);
        nameEnd -= end - start;
        while (start > name && *--start != '\\')
            ;
    }
    return len;
}

char* Replace(char* string, char s, char d)
{
    CALL_STACK_MESSAGE3("Replace(, %u, %u)", s, d);
    char* iterator = string;
    while (*iterator)
    {
        if (*iterator == s)
            *iterator = d;
        iterator++;
    }
    return string;
}
BOOL GetOpenFileName(HWND parent, const char* title, const char* filter, char* buffer, BOOL save)
{
    CALL_STACK_MESSAGE4("GetOpenFileName(, %s, %s, , %d)", title, filter, save);
    OPENFILENAME ofn;
    char buf[200];
    lstrcpyn(buf, filter, 200);
    Replace(buf, '\t', '\0');

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = buf;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    //ofn.lpfnHook = OFNHookProc;
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR /*| OFN_ENABLEHOOK*/;

    if (save)
        return SG->SafeGetSaveFileName(&ofn);
    else
    {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        return SG->SafeGetOpenFileName(&ofn);
    }
}
