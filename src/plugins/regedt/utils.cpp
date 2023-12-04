// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

BOOL PathAppend(WCHAR* path, WCHAR* more, int pathSize)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("PathAppend(, , %d)", pathSize);
    if (more[0] == L'\0')
    {
        return TRUE;
    }

    int len1 = (int)wcslen(path);
    int len2 = (int)wcslen(more);

    if (len1 && path[len1 - 1] != L'\\' && *more != L'\\')
    {
        if (len1 + 1 >= pathSize)
            return FALSE;
        path[len1++] = L'\\';
        path[len1] = L'\0';
    }
    if (len1 + len2 >= pathSize)
        return FALSE;
    wcscpy(path + len1, more);
    return TRUE;
}

BOOL CutDirectory(WCHAR* path, WCHAR* cutDir, int size)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CutDirectory(, , %d)", size);
    WCHAR* slash = wcsrchr(path, L'\\');
    if (!slash)
    {
        // cesta nezacinajici lomitkem
        if (*path == L'\0')
            return FALSE; // tohle uz zkratit nepujde
        if (cutDir)
            lstrcpynW(cutDir, path, size);
        *path = L'\0';
    }
    else
    {
        if (cutDir)
            lstrcpynW(cutDir, slash + 1, size);
        if (slash != path)
            *slash = L'\0';
        else
            slash[1] = L'\0';
    }
    return TRUE;
}

WCHAR*
DupStr(const WCHAR* str)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("DupStr()");
    WCHAR* ret;
    int len = (int)(wcslen(str) + 1) * 2;
    ret = (WCHAR*)SG->Alloc(len);
    if (ret)
        memcpy(ret, str, len);
    return ret;
}

char* DupStrA(const WCHAR* str)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("DupStrA()");
    char* ret;
    int len = (int)wcslen(str);
    ret = (char*)SG->Alloc(len + 1);
    if (ret)
    {
        if (WStrToStr(ret, len + 1, str, len + 1) <= 0)
        {
            TRACE_E("nejde prevest unicode na char, GetLastError()=" << ret);
            free(ret);
            ret = SG->DupStr("?");
        }
    }

    return ret;
}

char* StrNCat(char* dest, const char* sour, int destSize)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE3("StrNCat(, %s, %d)", sour, destSize);
    if (destSize == 0)
        return dest;
    char* start = dest;
    while (*dest)
        dest++;
    destSize -= (int)(dest - start);
    while (*sour && --destSize)
        *dest++ = *sour++;
    *dest = NULL;
    return start;
}

void RemoveTrailingSlashes(char* path)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("RemoveTrailingSlashes()");
    char* end = path + strlen(path);
    while (end > path && end[-1] == '\\')
        end--;
    *end = 0;
}

void RemoveTrailingSlashes(LPWSTR path)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("RemoveTrailingSlashes()");
    LPWSTR end = path + wcslen(path);
    while (end > path && end[-1] == L'\\')
        end--;
    *end = L'\0';
}

BOOL RegOperationError(int lastError, int error, int title, int keyRoot, LPWSTR keyName,
                       LPBOOL skip, LPBOOL skipAllErrors)
{
    CALL_STACK_MESSAGE5("RegOperationError(%d, %d, %d, %d, , , )", lastError,
                        error, title, keyRoot);
    if (skipAllErrors && *skipAllErrors)
    {
        if (skip)
            *skip = TRUE;
        return FALSE;
    }

    char buf[1024];
    int l = (int)strlen(strcpy(buf, LoadStr(error)));
    SG->GetErrorText(lastError, buf + l, 1024 - l);

    char fullName[MAX_FULL_KEYNAME]; // buffer pro plne jmeno na REG pro chybovy dialog
    l = WStrToStr(fullName, MAX_FULL_KEYNAME, PredefinedHKeys[keyRoot].KeyName) - 1;
    fullName[l++] = '\\';
    l += WStrToStr(fullName + l, MAX_FULL_KEYNAME - l, keyName) - 1;
    fullName[l] = 0; // pro jistotu

    int res = skip ? SG->DialogError(GetParent(), BUTTONS_RETRYSKIPCANCEL, fullName, buf, LoadStr(title)) : SG->DialogError(GetParent(), BUTTONS_RETRYCANCEL, fullName, buf, LoadStr(title));
    switch (res)
    {
    case DIALOG_RETRY:
        return TRUE;

    case DIALOG_SKIPALL:
        *skipAllErrors = TRUE;
    case DIALOG_SKIP:
        *skip = TRUE;
        return FALSE;

    default:
        if (skip)
            *skip = FALSE;
        return FALSE; // DIALOG_CANCEL
    }
}

// ****************************************************************************

void LoadHistory(HKEY regKey, const char* keyPattern, LPWSTR* history,
                 LPWSTR buffer, int bufferSize, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE3("LoadHistory(, %s, , , %d, )", keyPattern, bufferSize);
    char buf[32];
    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        SalPrintf(buf, 32, keyPattern, i);
        if (!registry->GetValue(regKey, buf, REG_BINARY, buffer, bufferSize * 2))
            break;
        LPWSTR ptr = new WCHAR[wcslen(buffer) + 1];
        if (!ptr)
            break;
        wcscpy(ptr, buffer);
        history[i] = ptr;
    }
}

void SaveHistory(HKEY regKey, const char* keyPattern, LPWSTR* history, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("SaveHistory(, %s, , )", keyPattern);
    char buf[32];
    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        if (history[i] == NULL)
            break;
        SalPrintf(buf, 32, keyPattern, i);
        registry->SetValue(regKey, buf, REG_BINARY, history[i], (int)wcslen(history[i]) * 2 + 2);
    }
}

// ****************************************************************************

BOOL TestForCancel()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("TestForCancel()");  // Petr: prilis pomaly call-stack
    static DWORD nextTest;
    if ((int)(GetTickCount() - nextTest) < 0)
        return FALSE;

    BOOL cancel = FALSE;
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == SG->GetMainWindowHWND())
    {
        MSG msg; // vyhodime nabufferovany ESC
        while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
            ;
        cancel = TRUE;
    }
    else
    {
        cancel = SG->GetSafeWaitWindowClosePressed();
    }

    cancel = cancel && SG->SalMessageBox(SG->GetMsgBoxParent(),
                                         LoadStr(IDS_CANCEL), LoadStr(IDS_QUESTION),
                                         MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED) == IDYES;
    UpdateWindow(SG->GetMainWindowHWND());
    nextTest = GetTickCount() + 150;
    SG->WaitForESCRelease();
    return cancel;
}

BOOL DuplicateChar(WCHAR dup, LPWSTR buffer, int bufferSize)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("DuplicateChar(, , %d)", bufferSize);
    if (buffer == NULL)
    {
        TRACE_E("1. unexpected situation in DuplicateChar()");
        return FALSE;
    }
    LPWSTR s = buffer;
    int l = (int)wcslen(buffer);
    if (l >= bufferSize)
    {
        TRACE_E("2. unexpected situation in DuplicateChar()");
        return FALSE;
    }
    BOOL ret = TRUE;
    while (*s != L'\0')
    {
        if (*s == dup)
        {
            if (l + 1 < bufferSize)
            {
                memmove(s + 1, s, (l - (s - buffer) + 1) * 2); // zdvojime dup
                l++;
                s++;
            }
            else // nevejde se, orizneme buffer
            {
                ret = FALSE;
                memmove(s + 1, s, (l - (s - buffer)) * 2); // zdvojime dup, orizneme o znak
                buffer[l] = L'\0';
                s++;
            }
        }
        s++;
    }
    return ret;
}

LPWSTR
UnDuplicateChar(WCHAR dup, LPWSTR buffer)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("UnDuplicateChar(, )");
    if (buffer == NULL)
    {
        TRACE_E("1. unexpected situation in UnDuplicateChar()");
        return FALSE;
    }
    LPWSTR s = buffer;
    BOOL ret = TRUE;
    while (*s != L'\0')
    {
        if (s[0] == dup && s[1] == dup)
        {
            MoveMemory(s, s + 1, (wcslen(s + 1) + 1) * 2);
        }
        s++;
    }
    return buffer;
}

BOOL ParseFullPath(WCHAR* path, WCHAR*& keyName, int& keyRoot)
{
    CALL_STACK_MESSAGE2("ParseFullPath(, , %d)", keyRoot);
    if (path[0] == L'\0')
        return FALSE;
    if (path[0] == L'\\' && path[1] == L'\0')
    {
        keyName = path + 1;
        keyRoot = -1;
        return TRUE;
    }
    keyName = wcschr(path + 1, L'\\');
    if (keyName == NULL)
        keyName = path + wcslen(path);
    int len = (int)(keyName - (path + 1));
    if (*keyName == L'\\')
        keyName++;
    if (len)
    {
        int i = 0;
        while (PredefinedHKeys[i].HKey != NULL)
        {
            if ((int)wcslen(PredefinedHKeys[i].KeyName) == len &&
                CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                               PredefinedHKeys[i].KeyName, len,
                               path + 1, len) == CSTR_EQUAL &&
                ((path + 1)[len] == L'\0' || (path + 1)[len] == L'\\'))
            {
                keyRoot = i;
                return TRUE;
            }
            i++;
        }
    }
    return FALSE;
}

inline BOOL IsXdigit(WCHAR c)
{
    CALL_STACK_MESSAGE_NONE
    return c >= L'0' && c <= L'9' || towlower(c) >= L'a' && towlower(c) <= L'f';
}

void ConvertHexToString(LPWSTR text, char* hex, int& len)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("ConvertHexToString(, , %d)", len);
    len = 0;
    LPWSTR s = text; //, st = text;
    BYTE value = 0;
    BOOL openedQuotesASCII = FALSE;
    BOOL openedQuotesUnicode = FALSE;
    while (1)
    {
        if (*s == L'"' && !openedQuotesASCII)
        {
            s++;
            openedQuotesUnicode = !openedQuotesUnicode;
            continue;
        }
        if (*s == L'\'' && !openedQuotesUnicode)
        {
            s++;
            openedQuotesASCII = !openedQuotesASCII;
            continue;
        }
        if (openedQuotesUnicode)
        {
            if (*s == 0)
                break;
            else
            {
                *(LPWSTR)(hex + len) = *s++;
                len += 2;
            }
        }
        else
        {
            if (openedQuotesASCII)
            {
                if (*s == 0)
                    break;
                else
                {
                    WStrToStr(hex + len, 1, s++, 1);
                    len += 1;
                }
            }
            else
            {
                if (IsXdigit(*s))
                {
                    if (*s >= L'0' && *s <= L'9')
                        value = (BYTE)(*s - L'0'); // prvni cislice
                    else
                        value = (BYTE)(10 + (towlower(*s) - L'a'));
                    s++;
                    if (IsXdigit(*s)) // druha cislice
                    {
                        value <<= 4;
                        if (*s >= L'0' && *s <= L'9')
                            value |= (BYTE)(*s - '0');
                        else
                            value |= (BYTE)(10 + (towlower(*s) - L'a'));
                        s++;
                    }
                    hex[len++] = value;
                }
                else
                {
                    if (*s == L'\0')
                        break; // konec retezce
                    else
                        s++; // preskok mezery
                }
            }
        }
    }
}

BOOL ValidateHexString(LPWSTR text)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("ValidateHexString()");
    int len = 0;
    LPWSTR s = text; //, st = text;
    BYTE value = 0;
    BOOL openedQuotesASCII = FALSE;
    BOOL openedQuotesUnicode = FALSE;
    while (1)
    {
        if (*s == L'"' && !openedQuotesASCII)
        {
            s++;
            openedQuotesUnicode = !openedQuotesUnicode;
            continue;
        }
        if (*s == L'\'' && !openedQuotesUnicode)
        {
            s++;
            openedQuotesASCII = !openedQuotesASCII;
            continue;
        }
        if (openedQuotesUnicode)
        {
            if (*s == 0)
                break;
            s++;
            len += 2;
        }
        else
        {
            if (openedQuotesASCII)
            {
                if (*s == 0)
                    break;
                else
                {
                    s++;
                    len += 1;
                }
            }
            else
            {
                if (IsXdigit(*s))
                {
                    s++;
                    if (IsXdigit(*s)) // druha cislice (povina)
                    {
                        s++;
                    }
                    else
                        return FALSE;
                }
                else
                {
                    if (*s == L'\0')
                        break; // konec retezce
                    else
                    {
                        if (*s != L' ')
                            return FALSE;
                        s++; // preskok mezery
                    }
                }
            }
        }
    }
    return TRUE;
}

// ****************************************************************************
//
// CBuffer
//

BOOL CBuffer::Reserve(int size)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("CBuffer::Reserve(%d)", size);
    if (size > Allocated)
    {
        int s = max(Allocated * 2, size);
        Release();
        Buffer = malloc(s);
        if (Buffer)
            Allocated = s;
        else
            return FALSE;
    }
    return TRUE;
}

// ****************************************************************************

char* Replace(char* string, char s, char d)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE3("Replace(, %d, %d)", s, d);
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
    char fileName[MAX_PATH];
    lstrcpyn(buf, filter, 200);
    Replace(buf, '\t', '\0');

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = buf;
    DWORD attr = SG->SalGetFileAttributes(buffer);
    if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        fileName[0] = 0;
        ofn.lpstrInitialDir = buffer;
    }
    else
        strcpy(fileName, buffer);
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    //ofn.lpfnHook = OFNHookProc;
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR /*| OFN_ENABLEHOOK*/;

    BOOL ret;
    if (save)
        ret = SG->SafeGetSaveFileName(&ofn);
    else
    {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        ret = SG->SafeGetOpenFileName(&ofn);
    }

    if (ret)
        strcpy(buffer, fileName);

    return ret;
}

BOOL RemoveFSNameFromPath(LPWSTR path)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("RemoveFSNameFromPath()");
    LPWSTR iterator = path;
    while (*iterator != L'\0' && *iterator != L'\\')
    {
        if (*iterator == L':')
        {
            char fsName[MAX_PATH];
            if (iterator - path)
            {
                int len = (int)WStrToStr(fsName, MAX_PATH, path, (int)(iterator - path));
                if (len == (int)strlen(AssignedFSName) && SG->StrNICmp(fsName, AssignedFSName, len) == 0)
                {
                    memmove(path, iterator + 1, (wcslen(iterator + 1) + 1) * 2);
                    return TRUE;
                }
                else
                    return FALSE; // neznamy FS
            }
            else
                return FALSE; // neznamy FS
        }
        iterator++;
    }
    return TRUE;
}

BOOL DecStringToNumber(char* string, QWORD& qw)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("DecStringToNumber(, )");
    QWORD ret = 0;

    // orizneme whitespace ze zacatku
    while (isspace(*string))
        string++;

    if (*string == 0)
        return FALSE;

    while (isdigit(*string))
    {
        if (ret > (((QWORD)-1) - (*string - '0')) / 10)
            return FALSE; // preteceni
        ret = ret * 10 + *string - '0';
        string++;
    }

    // orizneme whitespace na konci
    while (*string)
    {
        if (!isspace(*string))
            return FALSE;
        string++;
    }

    qw = ret;
    return TRUE;
}

BOOL HexStringToNumber(char* string, QWORD& qw)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("HexStringToNumber(, )");
    QWORD ret = 0;

    // orizneme whitespace ze zacatku
    while (isspace(*string))
        string++;

    if (string[0] == 0)
        return FALSE;

    while (isxdigit(*string))
    {
        if (ret >
            (((QWORD)-1) - (isdigit(*string) ? *string - '0' : tolower(*string) - 'a' + 10)) / 10)
            return FALSE; // preteceni
        ret = (ret << 4) + (isdigit(*string) ? *string - '0' : tolower(*string) - 'a' + 10);
        string++;
    }

    // orizneme whitespace na konci
    while (*string)
    {
        if (!isspace(*string))
            return FALSE;
        string++;
    }

    qw = ret;
    return TRUE;
}

char* ReplaceUnsafeCharacters(char* string)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("ReplaceUnsafeCharacters()");
    char* iterator = string;
    while (*iterator)
    {
        if (*iterator > 0 && *iterator <= 31 ||
            strchr("*?<>:\"/\\|", *iterator))
            *iterator = '_';
        iterator++;
    }
    return string;
}

/*
LPDLGTEMPLATE
LoadDlgTemplate(int id, DWORD &size)
{
  CALL_STACK_MESSAGE3("LoadDlgTemplate(%d, 0x%X)", id, size);
  LPDLGTEMPLATE ret = NULL;
  HRSRC hRsrc = FindResource(HLanguage, MAKEINTRESOURCE(id),  MAKEINTRESOURCE(RT_DIALOG));
  if (hRsrc)
  {
    void * data = LoadResource(HLanguage, hRsrc);
    if (data)
    {
      size = SizeofResource(HLanguage, hRsrc);
      if (size)
      {
        ret = (LPDLGTEMPLATE) malloc(size);
        if (ret)
        {
          memcpy(ret, data, size);
        }
        else TRACE_E("Low memory");
      }
      else TRACE_E("Unable to get size of resource");
    }
    else TRACE_E("Unable to load resource");
  }
  else TRACE_E("Unable to find resource");
  return ret;
}
*/

/*
LPDLGTEMPLATE
ReplaceDlgTemplateFont(LPDLGTEMPLATE dlgTemplate, DWORD &size, LPCWSTR newFont)
{
  CALL_STACK_MESSAGE2("ReplaceDlgTemplateFont(, 0x%X, )", size);
  if (!(dlgTemplate->style & DS_SETFONT ))
  {
    TRACE_E("ReplaceDlgTemplateFont: Dialog template nema DS_SETFONT.");
    return dlgTemplate;
  }

  LPWSTR ptr = LPWSTR((char *)dlgTemplate + sizeof(DLGTEMPLATE));

  // preskocime menu array
  if (*ptr == 0xFFFF) ptr += 2; // nasleduje ordinal value of a menu resource 
  else
  {
    // jde o NULL terminated string
    ptr += wcslen(ptr) + 1;
  }

  // preskocime class array
  if (*ptr == 0xFFFF) ptr += 2; // nasleduje ordinal value of a predefined system window class
  else
  {
    // jde o NULL terminated string
    ptr += wcslen(ptr) + 1;
  }

  // preskocime window title
  ptr += wcslen(ptr) + 1;

  // preskocime point size 
  ptr++;

  // upravime velikost template a presune data nasledujici za jmenem fontu
  int len1 = wcslen(ptr) + 1;
  int len2 = wcslen(newFont) + 1;
  DWORD dest = (DWORD(ptr + len2) - DWORD(dlgTemplate) + 3)/4*4;
  DWORD sour = (DWORD(ptr + len1) - DWORD(dlgTemplate) + 3)/4*4;
  if (sour > dest)
  {
    memmove((char*)dlgTemplate + dest, (char*)dlgTemplate + sour, size - sour);
    size -= sour - dest;
  }
  else
  {
    if (sour < dest)
    {
      DWORD offset = DWORD(ptr) - DWORD(dlgTemplate);
      void * p = realloc(dlgTemplate, size + (dest - sour));
      if (!p)
      {
        TRACE_E("Low memory");
        return dlgTemplate;
      }
      dlgTemplate = (LPDLGTEMPLATE) p;
      ptr = LPWSTR(DWORD(dlgTemplate) + offset);

      memmove((char*)dlgTemplate + dest, (char*)dlgTemplate + sour, size - sour);
      size += dest - sour;
    }
  }

  // zapiseme novy font name
  wcscpy(ptr, newFont);
  
  return dlgTemplate;
}
*/
