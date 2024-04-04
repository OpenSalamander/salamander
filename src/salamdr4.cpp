// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "salinflt.h"

//****************************************************************************
//
// CTruncatedString
//
// description in C
//

CTruncatedString::CTruncatedString()
{
    Text = NULL;
    SubStrIndex = -1;
    SubStrLen = 0;
    TruncatedText = NULL;
}

CTruncatedString::~CTruncatedString()
{
    if (Text != NULL)
        free(Text);
    if (TruncatedText != NULL)
        free(TruncatedText);
};

BOOL CTruncatedString::CopyFrom(const CTruncatedString* src)
{
    if (Text != NULL)
    {
        free(Text);
        Text = NULL;
    }
    if (src->Text != NULL)
    {
        Text = DupStr(src->Text);
        if (Text == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    SubStrIndex = src->SubStrIndex;
    SubStrLen = src->SubStrLen;
    if (TruncatedText != NULL)
    {
        free(TruncatedText);
        TruncatedText = NULL;
    }
    if (src->TruncatedText != NULL)
    {
        TruncatedText = DupStr(src->TruncatedText);
        if (TruncatedText == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CTruncatedString::Set(const char* str, const char* subStr)
{
    int len = (int)strlen(str);
    int subStrIndex = -1;
    int subStrLen = 0;
    if (subStr != NULL)
    {
        const char* p = str;
        int doubles = 0;
        while (*p != 0)
        {
            if (*p == '%')
            {
                if (*(p + 1) == '%')
                {
                    p++;
                    doubles++; // "%%" will be shortened to "%" thanks to sprintf
                }
                else
                {
                    if (*(p + 1) == 's')
                    {
                        subStrIndex = (int)(p - str - doubles);
                        break;
                    }
                    else
                    {
                        TRACE_E("CTruncatedString::Set: unknown format specifier in str:" << str);
                        break;
                    }
                }
            }
            p++;
        }
        if (subStrIndex == -1)
        {
            TRACE_E("CTruncatedString::Set: %s was not found in str:" << str);
        }
        else
        {
            len -= 2; // subtract %s that will be discarded
            subStrLen = (int)strlen(subStr);
            len += subStrLen;
        }
    }
    char* text = (char*)malloc(len + 1);
    if (text == NULL)
        return FALSE;
    if (Text != NULL)
        free(Text);
    Text = text;
    if (subStrIndex != -1)
    {
        sprintf(Text, str, subStr);
        SubStrIndex = subStrIndex;
        SubStrLen = subStrLen;
    }
    else
    {
        strcpy(Text, str);
        SubStrIndex = -1;
        SubStrLen = 0;
    }

    return TRUE;
}

const char*
CTruncatedString::Get()
{
    if (SubStrIndex == -1 || TruncatedText == NULL)
    {
        if (Text == NULL)
        {
            TRACE_E("Text == NULL");
            return "";
        }
        else
            return Text;
    }
    else
    {
        return TruncatedText;
    }
}

BOOL CTruncatedString::TruncateText(HWND hWindow, BOOL forMessageBox)
{
    // if there is nothing to shorten, we will leave
    if (SubStrIndex == -1)
        return TRUE;

    BOOL ret = TRUE;

    HDC hDC = HANDLES(GetDC(hWindow));
    HFONT hFont = (HFONT)SendMessage(hWindow, WM_GETFONT, 0, 0);
    HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);

    int fitChars;
    int alpDx[8000]; // for calculating widths
    int textLen = (int)strlen(Text);
    char* truncated = (char*)malloc(textLen + 1 + 3); // 3: reserve for emergency discharge
    if (truncated == NULL)
    {
        TRACE_E(LOW_MEMORY);
        ret = FALSE;
    }
    else
    {
        if (TruncatedText != NULL)
            free(TruncatedText);
        TruncatedText = truncated;

        if (forMessageBox)
        {
            // for message boxes -- we just ensure that the substring is not larger than 400 points. (fits even under 640x480)
            int chars = SubStrLen;
            int maxWidth = 400;
            SIZE sz;
            GetTextExtentExPoint(hDC, Text + SubStrIndex, SubStrLen, maxWidth, &fitChars, alpDx, &sz);
            if (fitChars < SubStrLen)
            {
                // first part with shortened substring
                memcpy(TruncatedText, Text, SubStrIndex + fitChars);
                // pass
                memcpy(TruncatedText + SubStrIndex + fitChars, "...", 3);
                // remainder
                strcpy(TruncatedText + SubStrIndex + fitChars + 3, Text + SubStrIndex + SubStrLen);
            }
            else
                memcpy(TruncatedText, Text, textLen + 1); // we just copy -- we fit
        }
        else
        {
            // single line for dialogues
            // we will determine the maximum width that we can afford
            RECT r;
            GetClientRect(hWindow, &r);
            int maxWidth = r.right;

            SIZE sz;
            if (textLen > 8000)
            {
                TRACE_E("Text was truncated (to 7999 characters)");
                Text[7999] = 0;
                textLen = 7999;
            }
            GetTextExtentExPoint(hDC, Text, textLen, 0, NULL, alpDx, &sz);
            if (sz.cx > maxWidth)
            {
                int width = sz.cx;

                GetTextExtentPoint32(hDC, "...", 3, &sz);
                int ellipsisWidth = sz.cx;

                // we will be subtracting from the reducible part
                int index = SubStrIndex + SubStrLen - 1;
                maxWidth -= ellipsisWidth;
                while (width > maxWidth && index >= SubStrIndex)
                {
                    width -= (alpDx[index] - alpDx[index - 1]);
                    index--;
                }
                // first part with shortened substring
                memcpy(TruncatedText, Text, index);
                // pass
                memcpy(TruncatedText + index, "...", 3);
                // remainder
                strcpy(TruncatedText + index + 3, Text + SubStrIndex + SubStrLen);
            }
            else
                memcpy(TruncatedText, Text, textLen + 1); // we just copy -- we fit
        }
    }

    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(hWindow, hDC));
    return ret;
}

//****************************************************************************
//
// StrToUInt64
//
// Converts a number (which can be preceded by a + sign) to unsigned __int64.
// Variable len specifies the maximum number of processed characters
// if 'isNum' is not NULL, it returns TRUE in it if the entire string
// 'str' represents a number
//

unsigned __int64
StrToUInt64(const char* str, int len, BOOL* isNum)
{
    const char* end = str + len;
    const char* s = str;
    while (s < end && *s <= ' ')
        s++;
    if (s < end && *s == '+')
        s++;

    unsigned __int64 total = 0;
    const char* begNum = s;
    while (s < end && *s >= '0' && *s <= '9')
    {
        unsigned __int64 new_total = total * 10 + (*s - '0');
        if (new_total >= total)
        {
            total = total * 10 + (*s - '0');
            s++;
        }
        else
        {
            total = 0xffffffffffffffff;
            while (s < end && *s >= '0' && *s <= '9')
                s++;
            break;
        }
    }
    BOOL hasDigits = begNum != s;
    while (s < end && *s <= ' ')
        s++;
    if (isNum != NULL)
        *isNum = (hasDigits && s == end);
    return total;
}

//****************************************************************************
//
// ExpandPluralString
//

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// WARNING: when making any modifications to ExpandPluralString, it is also necessary to modify
//        ValidatePluralStrings in the TRANSLATOR project
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

int ExpandPluralString(char* lpOut, int nOutMax, const char* lpFmt, int nParCount,
                       const CQuadWord* lpParArray)
{
    const char* input = lpFmt;
    char* output = lpOut;
    char* outputNullTerm = lpOut + nOutMax - 1;
    int actParIndex = 0;

    struct CAuxParUsed
    {
        BOOL StackArr[20];
        BOOL* Arr;
        CAuxParUsed(int nParCount)
        {
            Arr = nParCount <= sizeof(StackArr) / sizeof(StackArr[0]) ? StackArr : new BOOL[nParCount];
            memset(Arr, 0, nParCount * sizeof(BOOL));
        }
        ~CAuxParUsed()
        {
            if (Arr != StackArr)
                delete[] (Arr);
        }
    } parUsedArr(max(0, nParCount));

    if (nOutMax > 0 && lpOut != NULL)
        *lpOut = 0;

    // we will check and skip the signature {!}
    if (input != NULL && *input++ == '{' && *input++ == '!' && *input++ == '}' && nOutMax > 0)
    {
        while (*input != 0)
        {
            if (*input == '\\' &&
                (*(input + 1) == '|' || *(input + 1) == '\\' || *(input + 1) == ':' ||
                 *(input + 1) == '{' || *(input + 1) == '}')) // escape sequences
            {
                input++;
                if (output >= outputNullTerm) // The buffer must also fit the null terminator
                {
                    lpOut[nOutMax - 1] = 0;
                    TRACE_E("ExpandPluralString: truncated output string.");
                    return nOutMax - 1;
                }
                *output++ = *input++;
            }
            else
            {
                if (*input == '{') // we will perform expansion of a compound bracket
                {
                    input++;

                    // retrieve the corresponding value of the parameter from the array
                    unsigned __int64 arg;
                    const char* parInd = input;
                    int parIndVal = 0;
                    while (*parInd >= '0' && *parInd <= '9')
                        parIndVal = 10 * parIndVal + *parInd++ - '0';
                    if (*parInd == ':' && parInd > input) // we have assigned an index, we will use it
                    {
                        if (parIndVal >= 1 && parIndVal <= nParCount)
                        {
                            input = parInd + 1;
                            parUsedArr.Arr[parIndVal - 1] = TRUE;
                            arg = lpParArray[parIndVal - 1].Value;
                        }
                        else
                        {
                            TRACE_E("ExpandPluralString: specified index of parameter is out of range: " << parIndVal);
                            *output = 0;
                            return (int)(output - lpOut);
                        }
                    }
                    else // we will use another parameter in the sequence
                    {
                        if (actParIndex < nParCount)
                        {
                            parUsedArr.Arr[actParIndex] = TRUE;
                            arg = lpParArray[actParIndex++].Value;
                        }
                        else
                        {
                            TRACE_E("ExpandPluralString: few parameters in array.");
                            *output = 0;
                            return (int)(output - lpOut);
                        }
                    }

                    while (*input != '}' && *input != 0)
                    {
                        const char* subStr = input;
                        int subStrLen = 0;

                        while (*input != '}' && *input != 0 && *input != '|')
                        {
                            if (*input == '\\' &&
                                (*(input + 1) == '|' || *(input + 1) == '\\' || *(input + 1) == ':' ||
                                 *(input + 1) == '{' || *(input + 1) == '}')) // escape sequences
                                input++;
                            subStrLen++;
                            input++;
                        }

                        if (*input == '|')
                            input++;

                        const char* numStr = input;
                        int numStrLen = 0;

                        while (*input != '}' && *input != 0 && *input != '|')
                        {
                            if (*input == '\\' &&
                                (*(input + 1) == '|' || *(input + 1) == '\\' || *(input + 1) == ':' ||
                                 *(input + 1) == '{' || *(input + 1) == '}')) // escape sequences
                                input++;
                            numStrLen++;
                            input++;
                        }

                        if (*input == '|')
                            input++;

                        if (numStrLen == 0 && *input != '}')
                        {
                            TRACE_E("ExpandPluralString: syntax error: " << lpFmt);
                        }

                        unsigned __int64 num = 0;
                        if (numStrLen > 0)
                        {
                            BOOL isNum;
                            num = StrToUInt64(numStr, numStrLen, &isNum);
                            if (!isNum)
                                TRACE_E("ExpandPluralString: contains limit that is not a number: " << lpFmt);
                        }

                        // when it comes to the last string without interval restrictions,
                        // or the value of arg is less than or equal to the boundary of the interval
                        if (numStrLen == 0 || arg <= num)
                        {
                            // we insert the corresponding substring into the output string
                            int i;
                            for (i = 0; i < subStrLen; i++)
                            {
                                if (*subStr == '\\' &&
                                    (*(subStr + 1) == '|' || *(subStr + 1) == '\\' || *(subStr + 1) == ':' ||
                                     *(subStr + 1) == '{' || *(subStr + 1) == '}')) // escape sequences
                                    subStr++;
                                if (output >= outputNullTerm) // The buffer must also fit the null terminator
                                {
                                    lpOut[nOutMax - 1] = 0;
                                    TRACE_E("ExpandPluralString: truncated output string.");
                                    return nOutMax - 1;
                                }
                                *output++ = *subStr++;
                            }

                            // and let's end the search
                            while (*input != '}' && *input != 0)
                            {
                                if (*input == '\\' &&
                                    (*(input + 1) == '|' || *(input + 1) == '\\' || *(input + 1) == ':' ||
                                     *(input + 1) == '{' || *(input + 1) == '}')) // escape sequences
                                    input++;
                                input++;
                            }
                        }
                    }
                    if (*input == '}')
                        input++;
                }
                else
                {
                    if (output >= outputNullTerm) // The buffer must also fit the null terminator
                    {
                        lpOut[nOutMax - 1] = 0;
                        TRACE_E("ExpandPluralString: truncated output string.");
                        return nOutMax - 1;
                    }
                    *output++ = *input++;
                }
            }
        }
        *output = 0; // insert terminator
    }
    else
        TRACE_E("ExpandPluralString: format string does not contain {!} signature or output buffer is too short.");

    int i;
    for (i = 0; i < nParCount; i++)
        if (!parUsedArr.Arr[i])
        {
            TRACE_E("ExpandPluralString: warning: some parameters from array were not used, zero-based index "
                    "of first unused parameter is "
                    << i);
            break;
        }

    return (int)(output - lpOut);
}

//****************************************************************************
//
// ExpandPluralFilesDirs
//
// Writes the string to lpOut depending on the variables files and dirs:
// files > 0 && dirs == 0  ->  XXX (selected) files
// files == 0 && dirs > 0  ->  YYY (selected) directories
// files > 0 && dirs > 0   ->  XXX (selected) files and YYY directories
//
// where XXX and YYY correspond to the values of the variables files and dirs.
// Variable selectedForm controls the insertion of the selected word.
//
// ForDlgCaption is TRUE/FALSE if the text is/is not intended for the dialog caption
// (In English, capital letters at the beginning are required).
//
// Return the number of copied characters without the terminator.
//

int ExpandPluralFilesDirs(char* lpOut, int nOutMax, int files, int dirs, int mode, BOOL forDlgCaption)
{
    static int form[2][3][3] =
        {
            {{IDS_PLURAL_X_FILES, IDS_PLURAL_X_DIRS, IDS_PLURAL_X_FILES_Y_DIRS},
             {IDS_PLURAL_X_SEL_FILES, IDS_PLURAL_X_SEL_DIRS, IDS_PLURAL_X_SEL_FILES_Y_SEL_DIRS},
             {IDS_PLURAL_X_HID_FILES, IDS_PLURAL_X_HID_DIRS, IDS_PLURAL_X_HID_FILES_Y_HID_DIRS}},

            {{IDS_DLG_PLURAL_X_FILES, IDS_DLG_PLURAL_X_DIRS, IDS_DLG_PLURAL_X_FILES_Y_DIRS},
             {IDS_DLG_PLURAL_X_SEL_FILES, IDS_DLG_PLURAL_X_SEL_DIRS, IDS_DLG_PLURAL_X_SEL_FILES_Y_SEL_DIRS},
             {IDS_DLG_PLURAL_X_HID_FILES, IDS_DLG_PLURAL_X_HID_DIRS, IDS_DLG_PLURAL_X_HID_FILES_Y_HID_DIRS}},
        };
    int indDlgCaption = forDlgCaption ? 1 : 0;
    char expanded[200];
    if (nOutMax > 200)
        nOutMax = 200;
    nOutMax -= 20; // create space for numbers of files and dirs

    int ret;

    if (files > 0 && dirs == 0)
    {
        CQuadWord qwFiles(files, 0);
        ExpandPluralString(expanded, nOutMax, LoadStr(form[indDlgCaption][mode][0]), 1, &qwFiles);
        ret = sprintf(lpOut, expanded, files);
    }
    else
    {
        if (files == 0 && dirs > 0)
        {
            CQuadWord qwDirs(dirs, 0);
            ExpandPluralString(expanded, nOutMax, LoadStr(form[indDlgCaption][mode][1]), 1, &qwDirs);
            ret = sprintf(lpOut, expanded, dirs);
        }
        else
        {
            CQuadWord qwPars[2] = {CQuadWord(files, 0), CQuadWord(dirs, 0)};
            ExpandPluralString(expanded, nOutMax, LoadStr(form[indDlgCaption][mode][2]), 2, qwPars);
            ret = sprintf(lpOut, expanded, files, dirs);
        }
    }
    return ret;
}

int ExpandPluralBytesFilesDirs(char* lpOut, int nOutMax, const CQuadWord& selectedBytes, int files, int dirs, BOOL useSubTexts)
{
    char expanded[200];
    char number[50];
    if (nOutMax > 200)
        nOutMax = 200;
    nOutMax -= 30; // create space for numbers of files and dirs

    int ret;

    if (files > 0 && dirs == 0)
    {
        CQuadWord qwPars[2] = {selectedBytes, CQuadWord(files, 0)};
        ExpandPluralString(expanded, nOutMax,
                           LoadStr(useSubTexts ? IDS_PLURAL_X_BYTES_Y_SEL_FILES2 : IDS_PLURAL_X_BYTES_Y_SEL_FILES),
                           2, qwPars);
        ret = sprintf(lpOut, expanded, NumberToStr(number, selectedBytes), files);
    }
    else
    {
        if (files == 0 && dirs > 0)
        {
            CQuadWord qwPars[2] = {selectedBytes, CQuadWord(dirs, 0)};
            ExpandPluralString(expanded, nOutMax,
                               LoadStr(useSubTexts ? IDS_PLURAL_X_BYTES_Y_SEL_DIRS2 : IDS_PLURAL_X_BYTES_Y_SEL_DIRS),
                               2, qwPars);
            ret = sprintf(lpOut, expanded, NumberToStr(number, selectedBytes), dirs);
        }
        else
        {
            CQuadWord qwPars[3] = {selectedBytes, CQuadWord(files, 0), CQuadWord(dirs, 0)};
            ExpandPluralString(expanded, nOutMax,
                               LoadStr(useSubTexts ? IDS_PLURAL_X_BYTES_Y_SEL_FILES_Z_SEL_DIRS2 : IDS_PLURAL_X_BYTES_Y_SEL_FILES_Z_SEL_DIRS),
                               3, qwPars);
            ret = sprintf(lpOut, expanded, NumberToStr(number, selectedBytes), files, dirs);
        }
    }
    return ret;
}

BOOL LookForSubTexts(char* text, DWORD* varPlacements, int* varPlacementsCount)
{
    int maxVars = *varPlacementsCount;
    *varPlacementsCount = 0;

    const char* src = text; // we take a character from this pointer
    char* dst = text;       // we write the result to this pointer
    char* var = NULL;       // pointer to the first character of a variable

    while (*src != 0)
    {
        switch (*src)
        {
        case '\\':
        {
            if (*(src + 1) == '<' || *(src + 1) == '>' || *(src + 1) == '\\')
                src++; // escape sequence
            break;
        }

        case '<':
        {
            if (var != NULL)
            {
                TRACE_E("LookForSubTexts: syntax error in (already changed): " << text);
                return FALSE;
            }
            src++;
            var = dst;
            continue;
        }

        case '>':
        {
            if (var == NULL)
            {
                TRACE_E("LookForSubTexts: syntax error in (already changed): " << text);
                return FALSE;
            }
            if (*varPlacementsCount >= maxVars)
            {
                TRACE_E("LookForSubTexts: too many variables in: " << text);
                return FALSE;
            }
            *varPlacements = MAKELPARAM(var - text, dst - var);
            varPlacements++;
            (*varPlacementsCount)++;
            src++;
            var = NULL;
            continue;
        }
        }

        *dst = *src;
        src++;
        dst++;
    }
    // write terminator
    *dst = 0;
    if (var != NULL)
    {
        TRACE_E("LookForSubTexts: syntax error in (already changed): " << text);
        return FALSE;
    }

    return TRUE;
}

//****************************************************************************
//
// CViewTemplates
//

const char* SALAMANDER_VIEWTEMPLATE_NAME = "Name";
const char* SALAMANDER_VIEWTEMPLATE_FLAGS = "Flags";
const char* SALAMANDER_VIEWTEMPLATE_COLUMNS = "Columns";
const char* SALAMANDER_VIEWTEMPLATE_LEFTSMARTMODE = "Left Smart Mode";
const char* SALAMANDER_VIEWTEMPLATE_RIGHTSMARTMODE = "Right Smart Mode";

CViewTemplates::CViewTemplates()
{
    // default values
    Set(0, VIEW_MODE_TREE, LoadStr(IDS_TREE_VIEW), 0, TRUE, TRUE);
    Set(1, VIEW_MODE_BRIEF, LoadStr(IDS_BRIEF_VIEW), 0, TRUE, TRUE);
    Set(2, VIEW_MODE_DETAILED, LoadStr(IDS_DETAILED_VIEW), VIEW_SHOW_SIZE | VIEW_SHOW_DATE | VIEW_SHOW_TIME | VIEW_SHOW_ATTRIBUTES, TRUE, TRUE);
    Set(3, VIEW_MODE_ICONS, LoadStr(IDS_ICONS_VIEW), 0, TRUE, TRUE);
    Set(4, VIEW_MODE_THUMBNAILS, LoadStr(IDS_THUMBNAILS_VIEW), 0, TRUE, TRUE);
    Set(5, VIEW_MODE_TILES, LoadStr(IDS_TILES_VIEW), 0, TRUE, TRUE);
    Set(6, VIEW_MODE_DETAILED, LoadStr(IDS_TYPES_VIEW), VIEW_SHOW_SIZE | VIEW_SHOW_TYPE | VIEW_SHOW_DATE | VIEW_SHOW_TIME | VIEW_SHOW_ATTRIBUTES, TRUE, TRUE);
    //  Set(4, VIEW_MODE_DETAILED, LoadStr(IDS_DESCRIPTIONS_VIEW), VIEW_SHOW_SIZE | VIEW_SHOW_DESCRIPTION, TRUE, TRUE);
    int i;
    for (i = 7; i < VIEW_TEMPLATES_COUNT; i++)
        Set(i, VIEW_MODE_DETAILED, "", 0, TRUE, TRUE);
    for (i = 0; i < VIEW_TEMPLATES_COUNT; i++)
        ZeroMemory(Items[i].Columns, sizeof(Items[i].Columns));
}

void CViewTemplates::Set(DWORD index, const char* name, DWORD flags, BOOL leftSmartMode, BOOL rightSmartMode)
{
    if (lstrlen(name) >= VIEW_NAME_MAX)
        TRACE_E("String is too long");
    lstrcpyn(Items[index].Name, name, VIEW_NAME_MAX);
    Items[index].Flags = flags;
    Items[index].LeftSmartMode = leftSmartMode;
    Items[index].RightSmartMode = rightSmartMode;
}

void CViewTemplates::Set(DWORD index, DWORD viewMode, const char* name, DWORD flags, BOOL leftSmartMode, BOOL rightSmartMode)
{
    Items[index].Mode = viewMode;
    Set(index, name, flags, leftSmartMode, rightSmartMode);
}

BOOL CViewTemplates::SwapItems(int index1, int index2)
{
    if (index1 < 2 || index2 < 2)
    {
        TRACE_E("It is not possible to move first nor second item.");
        return FALSE;
    }

    if (index1 >= VIEW_TEMPLATES_COUNT || index2 >= VIEW_TEMPLATES_COUNT)
    {
        TRACE_E("Index is out of range");
        return FALSE;
    }

    CViewTemplate tmp = Items[index1];
    Items[index1] = Items[index2];
    Items[index2] = tmp;
    return TRUE;
}

BOOL CViewTemplates::CleanName(char* name)
{
    char* start = name;
    char* end = name + strlen(name) - 1;
    while (*start != 0 && *start == ' ')
        start++;
    while (end >= name && *end == ' ')
        end--;
    end++;
    *end = 0;
    if (start > name && start < end)
        memmove(name, start, end - start + 1);
    return strlen(name) > 0;
}

int CViewTemplates::SaveColumns(CColumnConfig* columns, char* buffer)
{
    char* s = buffer;
    int i;
    for (i = 0; i < STANDARD_COLUMNS_COUNT; i++)
    {
        CColumnConfig* column = &columns[i];
        if (i > 0)
        {
            *s = ',';
            s++;
        }
        DWORD data = column->LeftWidth | column->LeftFixedWidth << 16;
        s += sprintf(s, "%lx", data);
    }
    *s++ = ',';
    for (i = 0; i < STANDARD_COLUMNS_COUNT; i++)
    {
        CColumnConfig* column = &columns[i];
        if (i > 0)
        {
            *s = ',';
            s++;
        }
        DWORD data = column->RightWidth | column->RightFixedWidth << 16;
        s += sprintf(s, "%lx", data);
    }
    *s = 0;
    return (int)(s - buffer);
}

void CViewTemplates::LoadColumns(CColumnConfig* columns, char* buffer)
{
    CColumnConfig* firstColumn = columns;
    char* p = strtok(buffer, ",");
    while (p != NULL)
    {
        DWORD data;
        int i = sscanf(p, "%xl", &data);
        columns->LeftWidth = data & 0x0000ffff;
        columns->LeftFixedWidth = (data & 0x00010000) >> 16;
        if (columns->LeftWidth > 2000)
            columns->LeftWidth = 2000;
        columns->RightWidth = columns->LeftWidth;
        columns->RightFixedWidth = columns->LeftFixedWidth;
        p = strtok(NULL, ",");
        columns++;
        if (columns - firstColumn >= STANDARD_COLUMNS_COUNT)
            break;
    }
    columns = firstColumn;
    while (p != NULL)
    {
        DWORD data;
        int i = sscanf(p, "%xl", &data);
        columns->RightWidth = data & 0x0000ffff;
        columns->RightFixedWidth = (data & 0x00010000) >> 16;
        if (columns->RightWidth > 2000)
            columns->RightWidth = 2000;
        p = strtok(NULL, ",");
        columns++;
        if (columns - firstColumn >= STANDARD_COLUMNS_COUNT)
            break;
    }
}

BOOL CViewTemplates::Save(HKEY hKey)
{
    char buff[512];
    char keyName[5];
    int i;
    for (i = 0; i < VIEW_TEMPLATES_COUNT; i++)
    {
        itoa(i < VIEW_TEMPLATES_COUNT - 1 ? i + 1 : 0, keyName, 10);
        HKEY actKey;
        if (CreateKey(hKey, keyName, actKey))
        {
            SetValue(actKey, SALAMANDER_VIEWTEMPLATE_NAME, REG_SZ, Items[i].Name, -1);
            SetValue(actKey, SALAMANDER_VIEWTEMPLATE_FLAGS, REG_DWORD, &Items[i].Flags, sizeof(DWORD));
            SetValue(actKey, SALAMANDER_VIEWTEMPLATE_COLUMNS, REG_SZ, buff, SaveColumns(Items[i].Columns, buff));
            SetValue(actKey, SALAMANDER_VIEWTEMPLATE_LEFTSMARTMODE, REG_DWORD, &Items[i].LeftSmartMode, sizeof(DWORD));
            SetValue(actKey, SALAMANDER_VIEWTEMPLATE_RIGHTSMARTMODE, REG_DWORD, &Items[i].RightSmartMode, sizeof(DWORD));
            CloseKey(actKey);
        }
    }
    return TRUE;
}

BOOL CViewTemplates::Load(HKEY hKey)
{
    char buff[512];
    char keyName[5];
    int i;
    for (i = 0; i < VIEW_TEMPLATES_COUNT; i++)
    {
        itoa(i < VIEW_TEMPLATES_COUNT - 1 ? i + 1 : 0, keyName, 10);
        if (i == 6 && Configuration.ConfigVersion < 23)
            continue; // for the IDS_TYPES_VIEW view, we want default columns
        HKEY actKey;
        if (OpenKey(hKey, keyName, actKey))
        {
            char name[MAX_PATH];
            DWORD flags;
            name[0] = 0;
            flags = 0;
            buff[0] = 0;
            DWORD leftSM = TRUE;
            DWORD rightSM = TRUE;
            GetValue(actKey, SALAMANDER_VIEWTEMPLATE_LEFTSMARTMODE, REG_DWORD, &leftSM, sizeof(DWORD));
            GetValue(actKey, SALAMANDER_VIEWTEMPLATE_RIGHTSMARTMODE, REG_DWORD, &rightSM, sizeof(DWORD));
            if (GetValue(actKey, SALAMANDER_VIEWTEMPLATE_NAME, REG_SZ, name, VIEW_NAME_MAX) &&
                GetValue(actKey, SALAMANDER_VIEWTEMPLATE_FLAGS, REG_DWORD, &flags, sizeof(DWORD)) &&
                GetValue(actKey, SALAMANDER_VIEWTEMPLATE_COLUMNS, REG_SZ, buff, 512))
            {
                LoadColumns(Items[i].Columns, buff);
                CleanName(name);

                // overriding file names that the same user could not change
                int resID = -1;
                switch (i)
                {
                case 0:
                    resID = IDS_TREE_VIEW;
                    break;
                case 1:
                    resID = IDS_BRIEF_VIEW;
                    break;
                case 2:
                    resID = IDS_DETAILED_VIEW;
                    break;
                case 3:
                    resID = IDS_ICONS_VIEW;
                    break;
                case 4:
                    resID = IDS_THUMBNAILS_VIEW;
                    break;
                case 5:
                    resID = IDS_TILES_VIEW;
                    break;
                case 6:
                    resID = IDS_TYPES_VIEW;
                    break;
                }
                if (resID != -1)
                    strcpy(name, LoadStr(resID));

                Set(i, name, flags, leftSM, rightSM);
            }
            CloseKey(actKey);
        }
    }
    return TRUE;
}

// ****************************************************************************

DWORD AddUnicodeToClipboard(const char* str, int textLen)
{
    DWORD err = ERROR_SUCCESS;
    int unicodeLen = 0;
    if (textLen > 0)
    {
        unicodeLen = MultiByteToWideChar(CP_ACP, 0, str, textLen, NULL, 0);
        if (unicodeLen == 0)
            err = GetLastError();
    }
    if (err == ERROR_SUCCESS)
    {
        HGLOBAL unicode = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(WCHAR) * (unicodeLen + 1)));
        if (unicode != NULL)
        {
            WCHAR* unicodeStr = (WCHAR*)HANDLES(GlobalLock(unicode));
            if (unicodeStr != NULL)
            {
                if (textLen > 0 && MultiByteToWideChar(CP_ACP, 0, str, textLen, unicodeStr, unicodeLen + 1) == 0)
                    err = GetLastError();
                unicodeStr[unicodeLen] = 0; // ending zero
                HANDLES(GlobalUnlock(unicode));
                if (err == ERROR_SUCCESS && SetClipboardData(CF_UNICODETEXT, unicode) == NULL)
                    err = GetLastError();
            }
            else
                err = GetLastError();
            if (err != ERROR_SUCCESS)
                NOHANDLES(GlobalFree(unicode));
        }
        else
            err = GetLastError();
    }
    if (err != ERROR_SUCCESS)
        TRACE_E("SetClipboardData failed for Unicode version of text. Error: " << GetErrorText(err));
    return err;
}

// ****************************************************************************

DWORD AddMultibyteToClipboard(const wchar_t* str, int textLen)
{
    DWORD err = ERROR_SUCCESS;
    int mbLen = textLen == 0 ? 0 : WideCharToMultiByte(CP_ACP, 0, str, textLen, NULL, 0, NULL, NULL);
    if (mbLen > 0 || textLen == 0)
    {
        HGLOBAL multibyte = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, mbLen + 1));
        if (multibyte != NULL)
        {
            char* multibyteStr = (char*)HANDLES(GlobalLock(multibyte));
            if (multibyteStr != NULL)
            {
                if (textLen > 0 && WideCharToMultiByte(CP_ACP, 0, str, textLen, multibyteStr, mbLen + 1, NULL, NULL) == 0)
                    err = GetLastError();
                multibyteStr[mbLen] = 0; // ending zero
                HANDLES(GlobalUnlock(multibyte));
                if (err == ERROR_SUCCESS && SetClipboardData(CF_TEXT, multibyte) == NULL)
                    err = GetLastError();
            }
            else
                err = GetLastError();
            if (err != ERROR_SUCCESS)
                NOHANDLES(GlobalFree(multibyte));
        }
        else
            err = GetLastError();
    }
    else
        err = GetLastError();

    if (err != ERROR_SUCCESS)
        TRACE_E("SetClipboardData failed for multibyte version of text. Error: " << GetErrorText(err));
    return err;
}

// ****************************************************************************

BOOL CopyHTextToClipboardW(HGLOBAL hGlobalText, int textLen)
{
    if (hGlobalText == NULL)
    {
        TRACE_E("hGlobalText == NULL");
        return FALSE;
    }

    DWORD err = ERROR_SUCCESS;

    if (OpenClipboard(NULL))
    {
        if (EmptyClipboard())
        {
            wchar_t* text = (wchar_t*)HANDLES(GlobalLock(hGlobalText));
            if (text != NULL)
            {
                if (textLen == -1)
                    textLen = (int)wcslen(text);
                err = AddMultibyteToClipboard(text, textLen); // we will first save the text in multibyte
                HANDLES(GlobalUnlock(hGlobalText));
            }
            else
                err = GetLastError();

            if (SetClipboardData(CF_UNICODETEXT, hGlobalText) == NULL) // then we save the multibyte text
                err = GetLastError();
        }
        else
            err = GetLastError();
        CloseClipboard();

        IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
        IdleCheckClipboard = TRUE; // we will also check the clipboard
    }
    else
    {
        err = GetLastError();
        TRACE_E("OpenClipboard() has failed!");
    }

    return err == ERROR_SUCCESS;
}

// ****************************************************************************

BOOL CopyTextToClipboardW(const wchar_t* text, int textLen, BOOL showEcho, HWND hEchoParent)
{
    if (text == NULL)
    {
        TRACE_E("text == NULL");
        return FALSE;
    }

    DWORD err = ERROR_SUCCESS;

    if (textLen == -1)
        textLen = (int)wcslen(text);

    HGLOBAL hglbCopy = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(wchar_t) * (textLen + 1)));
    if (hglbCopy != NULL)
    {
        wchar_t* lptstrCopy = (wchar_t*)HANDLES(GlobalLock(hglbCopy));
        if (lptstrCopy != NULL)
        {
            memcpy(lptstrCopy, text, sizeof(wchar_t) * textLen);
            lptstrCopy[textLen] = 0;
            HANDLES(GlobalUnlock(hglbCopy));

            if (!CopyHTextToClipboardW(hglbCopy, textLen))
                return FALSE;
        }
        else
            err = GetLastError();
    }
    else
        err = GetLastError();

    if (showEcho)
    {
        if (err != ERROR_SUCCESS)
            SalMessageBox(hEchoParent, GetErrorText(err), LoadStr(IDS_COPYTOCLIPBOARD),
                          MB_OK | MB_ICONEXCLAMATION);
        else
            SalMessageBox(hEchoParent, LoadStr(IDS_TEXTCOPIED), LoadStr(IDS_INFOTITLE),
                          MB_OK | MB_ICONINFORMATION);
    }
    return err == ERROR_SUCCESS;
}

// ****************************************************************************

BOOL CopyTextToClipboard(const char* text, int textLen, BOOL showEcho, HWND hEchoParent)
{
    if (text == NULL)
    {
        TRACE_E("text == NULL");
        return FALSE;
    }

    DWORD err = ERROR_SUCCESS;

    if (textLen == -1)
        textLen = lstrlen(text);

    HGLOBAL hglbCopy = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, textLen + 1));
    if (hglbCopy != NULL)
    {
        char* lptstrCopy = (char*)HANDLES(GlobalLock(hglbCopy));
        if (lptstrCopy != NULL)
        {
            memcpy(lptstrCopy, text, textLen);
            lptstrCopy[textLen] = 0;
            HANDLES(GlobalUnlock(hglbCopy));
            if (!CopyHTextToClipboard(hglbCopy, textLen, NULL, FALSE))
                return FALSE;
        }
        else
            err = GetLastError();
    }
    else
        err = GetLastError();

    if (showEcho)
    {
        if (err != ERROR_SUCCESS)
            SalMessageBox(hEchoParent, GetErrorText(err), LoadStr(IDS_COPYTOCLIPBOARD),
                          MB_OK | MB_ICONEXCLAMATION);
        else
            SalMessageBox(hEchoParent, LoadStr(IDS_TEXTCOPIED), LoadStr(IDS_INFOTITLE),
                          MB_OK | MB_ICONINFORMATION);
    }
    return err == ERROR_SUCCESS;
}

// ****************************************************************************

BOOL CopyHTextToClipboard(HGLOBAL hGlobalText, int textLen, BOOL showEcho, HWND hEchoParent)
{
    if (hGlobalText == NULL)
    {
        TRACE_E("hGlobalText == NULL");
        return FALSE;
    }

    DWORD err = ERROR_SUCCESS;

    if (OpenClipboard(NULL))
    {
        if (EmptyClipboard())
        {
            char* text = (char*)HANDLES(GlobalLock(hGlobalText));
            if (text != NULL)
            {
                if (textLen == -1)
                    textLen = lstrlen(text);
                err = AddUnicodeToClipboard(text, textLen); // we will first save the text in Unicode
                HANDLES(GlobalUnlock(hGlobalText));
            }
            else
                err = GetLastError();

            if (SetClipboardData(CF_TEXT, hGlobalText) == NULL) // then we save the multibyte text
                err = GetLastError();
        }
        else
            err = GetLastError();
        CloseClipboard();

        IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
        IdleCheckClipboard = TRUE; // we will also check the clipboard
    }
    else
    {
        err = GetLastError();
        TRACE_E("OpenClipboard() has failed!");
    }

    if (showEcho)
    {
        if (err != ERROR_SUCCESS)
            SalMessageBox(hEchoParent, GetErrorText(err), LoadStr(IDS_COPYTOCLIPBOARD),
                          MB_OK | MB_ICONEXCLAMATION);
        else
            SalMessageBox(hEchoParent, LoadStr(IDS_TEXTCOPIED), LoadStr(IDS_INFOTITLE),
                          MB_OK | MB_ICONINFORMATION);
    }
    return err == ERROR_SUCCESS;
}

//****************************************************************************
//
// Internal function for retrieving the content of a column
//

// initialized in the drawing routine before calling the callback
const CFileData* TransferFileData;
int TransferIsDir;
char TransferBuffer[TRANSFER_BUFFER_MAX];
int TransferLen;
DWORD TransferRowData;
CPluginDataInterfaceAbstract* TransferPluginDataIface;
DWORD TransferActCustomData;

int TransferAssocIndex;

void WINAPI InternalGetDosName()
{
    if (TransferFileData->DosName != NULL)
    {
        TransferLen = lstrlen(TransferFileData->DosName);
        CopyMemory(TransferBuffer, TransferFileData->DosName, TransferLen);
    }
    else
        TransferLen = 0;
}

void WINAPI InternalGetSize()
{
    if (TransferIsDir && !TransferFileData->SizeValid) // only directories without known size
    {
        memmove(TransferBuffer, DirColumnStr, DirColumnStrLen);
        TransferLen = DirColumnStrLen;
    }
    else
    {
        switch (Configuration.SizeFormat)
        {
        case SIZE_FORMAT_BYTES:
        {
            TransferLen = NumberToStr2(TransferBuffer, TransferFileData->Size);
            break;
        }

        case SIZE_FORMAT_KB: // Attention, the same code is in another place, search for this constant
        {
            PrintDiskSize(TransferBuffer, TransferFileData->Size, 3);
            TransferLen = (int)strlen(TransferBuffer);
            break;
        }

        case SIZE_FORMAT_MIXED:
        {
            PrintDiskSize(TransferBuffer, TransferFileData->Size, 0);
            TransferLen = (int)strlen(TransferBuffer);
            break;
        }
        }
    }
}

// Helper globals for InternalGetType()
char* InternalGetTypeAux1;
char* InternalGetTypeAux2;
char InternalGetTypeAux3[MAX_PATH + 4]; // suffix in lowercase, aligned to DWORDs

void WINAPI InternalGetType()
{
    if (TransferIsDir) // we will have to address it differently
    {
        TransferLen = TransferIsDir == 1 ? FolderTypeNameLen : UpDirTypeNameLen;
        memcpy(TransferBuffer, TransferIsDir == 1 ? FolderTypeName : UpDirTypeName, TransferLen);
    }
    else
    {
        if (TransferAssocIndex == -2) // We haven't searched for the suffix yet.
        {
            if (TransferFileData->Ext[0] != 0)
            {
                InternalGetTypeAux1 = InternalGetTypeAux3;
                InternalGetTypeAux2 = TransferFileData->Ext;
                while (*InternalGetTypeAux2 != 0)
                    *InternalGetTypeAux1++ = LowerCase[*InternalGetTypeAux2++];
                *((DWORD*)InternalGetTypeAux1) = 0;
                if (!Associations.GetIndex(InternalGetTypeAux3, TransferAssocIndex))
                    TransferAssocIndex = -1; // not found
            }
            else
                TransferAssocIndex = -1; // without suffix -> cannot be in Associations
        }

        if (TransferAssocIndex == -1)
            GetCommonFileTypeStr(TransferBuffer, &TransferLen, TransferFileData->Ext);
        else
        {
            InternalGetTypeAux1 = Associations[TransferAssocIndex].Type;
            if (InternalGetTypeAux1 != NULL) // valid file type
            {
                TransferLen = (int)strlen(InternalGetTypeAux1);
                memcpy(TransferBuffer, InternalGetTypeAux1, TransferLen);
            }
            else
                GetCommonFileTypeStr(TransferBuffer, &TransferLen, TransferFileData->Ext);
        }
    }
}

// We can afford this optimization because we are not called from multiple threads simultaneously.
static SYSTEMTIME InternalColumnST;
static FILETIME InternalColumnFT;

void WINAPI InternalGetDate()
{
    if ((TransferRowData & 0x00000001) == 0)
    {
        if (!FileTimeToLocalFileTime(&TransferFileData->LastWrite, &InternalColumnFT) ||
            !FileTimeToSystemTime(&InternalColumnFT, &InternalColumnST))
        {
            TransferLen = sprintf(TransferBuffer, LoadStr(IDS_INVALID_DATEORTIME));
            return;
        }
        TransferRowData |= 0x00000001;
    }
    TransferLen = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &InternalColumnST, NULL, TransferBuffer, TRANSFER_BUFFER_MAX) - 1;
    if (TransferLen < 0)
        TransferLen = sprintf(TransferBuffer, "%u.%u.%u", InternalColumnST.wDay, InternalColumnST.wMonth, InternalColumnST.wYear);
}

void WINAPI InternalGetDateOnlyForDisk()
{
    if ((TransferRowData & 0x00000001) == 0)
    {
        if (!FileTimeToLocalFileTime(&TransferFileData->LastWrite, &InternalColumnFT) ||
            !FileTimeToSystemTime(&InternalColumnFT, &InternalColumnST))
        {
            TransferLen = sprintf(TransferBuffer, LoadStr(IDS_INVALID_DATEORTIME));
            return;
        }
        TransferRowData |= 0x00000001;
    }
    if (TransferIsDir == 2 /* UP-DIR*/ &&
        InternalColumnST.wYear == 1602 && InternalColumnST.wMonth == 1 && InternalColumnST.wDay == 1 &&
        InternalColumnST.wHour == 0 && InternalColumnST.wMinute == 0 && InternalColumnST.wSecond == 0 &&
        InternalColumnST.wMilliseconds == 0)
    {
        TransferLen = 0;
        return;
    }
    TransferLen = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &InternalColumnST, NULL, TransferBuffer, TRANSFER_BUFFER_MAX) - 1;
    if (TransferLen < 0)
        TransferLen = sprintf(TransferBuffer, "%u.%u.%u", InternalColumnST.wDay, InternalColumnST.wMonth, InternalColumnST.wYear);
}

void WINAPI InternalGetTime()
{
    if ((TransferRowData & 0x00000001) == 0)
    {
        if (!FileTimeToLocalFileTime(&TransferFileData->LastWrite, &InternalColumnFT) ||
            !FileTimeToSystemTime(&InternalColumnFT, &InternalColumnST))
        {
            TransferLen = sprintf(TransferBuffer, LoadStr(IDS_INVALID_DATEORTIME));
            return;
        }
        TransferRowData |= 0x00000001;
    }
    TransferLen = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &InternalColumnST, NULL, TransferBuffer, TRANSFER_BUFFER_MAX) - 1;
    if (TransferLen < 0)
        TransferLen = sprintf(TransferBuffer, "%u:%02u:%02u", InternalColumnST.wHour, InternalColumnST.wMinute, InternalColumnST.wSecond);
}

void WINAPI InternalGetTimeOnlyForDisk()
{
    if ((TransferRowData & 0x00000001) == 0)
    {
        if (!FileTimeToLocalFileTime(&TransferFileData->LastWrite, &InternalColumnFT) ||
            !FileTimeToSystemTime(&InternalColumnFT, &InternalColumnST))
        {
            TransferLen = sprintf(TransferBuffer, LoadStr(IDS_INVALID_DATEORTIME));
            return;
        }
        TransferRowData |= 0x00000001;
    }
    if (TransferIsDir == 2 /* UP-DIR*/ &&
        InternalColumnST.wYear == 1602 && InternalColumnST.wMonth == 1 && InternalColumnST.wDay == 1 &&
        InternalColumnST.wHour == 0 && InternalColumnST.wMinute == 0 && InternalColumnST.wSecond == 0 &&
        InternalColumnST.wMilliseconds == 0)
    {
        TransferLen = 0;
        return;
    }
    TransferLen = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &InternalColumnST, NULL, TransferBuffer, TRANSFER_BUFFER_MAX) - 1;
    if (TransferLen < 0)
        TransferLen = sprintf(TransferBuffer, "%u:%02u:%02u", InternalColumnST.wHour, InternalColumnST.wMinute, InternalColumnST.wSecond);
}

void WINAPI InternalGetAttr()
{
    TransferLen = 0;
    // WARNING: If we want to display additional attributes, it is necessary to modify GetAttrsString() and the DISPLAYED_ATTRIBUTES mask !!!
    if (TransferFileData->Attr & FILE_ATTRIBUTE_READONLY)
        TransferBuffer[TransferLen++] = 'R';
    if (TransferFileData->Attr & FILE_ATTRIBUTE_HIDDEN)
        TransferBuffer[TransferLen++] = 'H';
    if (TransferFileData->Attr & FILE_ATTRIBUTE_SYSTEM)
        TransferBuffer[TransferLen++] = 'S';
    if (TransferFileData->Attr & FILE_ATTRIBUTE_ARCHIVE)
        TransferBuffer[TransferLen++] = 'A';
    if (TransferFileData->Attr & FILE_ATTRIBUTE_TEMPORARY)
        TransferBuffer[TransferLen++] = 'T';
    if (TransferFileData->Attr & FILE_ATTRIBUTE_COMPRESSED)
        TransferBuffer[TransferLen++] = 'C';
    if (TransferFileData->Attr & FILE_ATTRIBUTE_ENCRYPTED)
        TransferBuffer[TransferLen++] = 'E';
    if (TransferFileData->Attr & FILE_ATTRIBUTE_OFFLINE)
        TransferBuffer[TransferLen++] = 'O';
}

void WINAPI InternalGetDescr()
{
    TransferLen = 0;
}

//****************************************************************************
//
// Internal function for obtaining the index of simple icons for FS with custom icons (pitFromPlugin)
//

int WINAPI InternalGetPluginIconIndex()
{
    return 0;
}

//*****************************************************************************
//
// CSalamanderView
//

CSalamanderView::CSalamanderView(CFilesWindow* panel)
{
    Panel = panel;
    Panel->GetPluginIconIndex = InternalGetPluginIconIndex;
}

DWORD
CSalamanderView::GetViewMode()
{
    return Panel->ViewTemplate->Mode;
}

void CSalamanderView::SetViewMode(DWORD viewMode, DWORD validFileData)
{
    if (Panel->ViewTemplate->Mode != viewMode)
    {
        int templateIndex;
        switch (viewMode)
        {
        case VIEW_MODE_TREE:
            templateIndex = 0;
            break;
        case VIEW_MODE_BRIEF:
            templateIndex = 1;
            break;
        case VIEW_MODE_DETAILED:
            templateIndex = 2;
            break;
        case VIEW_MODE_ICONS:
            templateIndex = 3;
            break;
        case VIEW_MODE_THUMBNAILS:
            templateIndex = 4;
            break;
        case VIEW_MODE_TILES:
            templateIndex = 5;
            break;
        default:
            TRACE_E("Unknown viewMode=" << viewMode);
            templateIndex = 2;
            break;
        }
        Panel->SelectViewTemplate(templateIndex, FALSE, TRUE, validFileData);
    }
    else
        Panel->SelectViewTemplate(Panel->GetViewTemplateIndex(), FALSE, TRUE, validFileData);
}

void CSalamanderView::SetPluginSimpleIconCallback(FGetPluginIconIndex callback)
{
    Panel->GetPluginIconIndex = callback;
}

int CSalamanderView::GetColumnsCount()
{
    return Panel->Columns.Count;
}

const CColumn*
CSalamanderView::GetColumn(int index)
{
    return (index >= 0 && index < Panel->Columns.Count) ? &Panel->Columns[index] : NULL;
}

BOOL CSalamanderView::InsertColumn(int index, const CColumn* column)
{
    int low = 1;
    if (Panel->Columns.Count > 1 && Panel->Columns[1].ID == COLUMN_ID_EXTENSION)
        low++; // we must not let them wander between Name and Ext
    if (index < low || index > Panel->Columns.Count)
    {
        TRACE_E("CSalamanderView::InsertColumn(): index=" << index << " is incorrect.");
        return FALSE;
    }
    if (column->ID != COLUMN_ID_CUSTOM)
    {
        TRACE_E("CSalamanderView::InsertColumn(): column->ID != COLUMN_ID_CUSTOM.");
        return FALSE;
    }
    Panel->Columns.Insert(index, *column);
    if (!Panel->Columns.IsGood())
    {
        TRACE_E("CSalamanderView::InsertColumn(): Columns.Insert() failed");
        Panel->Columns.ResetState();
        return FALSE;
    }
    return TRUE;
}

BOOL CSalamanderView::InsertStandardColumn(int index, DWORD id)
{
    int low = 1;
    if (Panel->Columns.Count > 1 && Panel->Columns[1].ID == COLUMN_ID_EXTENSION)
        low++; // we must not let them wander between Name and Ext
    if (index < low || index > Panel->Columns.Count ||
        index != 1 && id == COLUMN_ID_EXTENSION)
    {
        TRACE_E("CSalamanderView::InsertStandardColumn(): index=" << index << " is incorrect.");
        return FALSE;
    }
    if (id == COLUMN_ID_NAME || id == COLUMN_ID_CUSTOM)
    {
        TRACE_E("CSalamanderView::InsertStandardColumn(): column->ID == COLUMN_ID_CUSTOM or COLUMN_ID_NAME.");
        return FALSE;
    }

    CColumDataItem* item = NULL;
    int i;
    for (i = 0; i < STANDARD_COLUMNS_COUNT; i++)
    {
        if (GetStdColumn(i, FALSE)->ID == id) // found the desired std. column
        {
            item = GetStdColumn(i, FALSE);
            break;
        }
    }
    if (item != NULL)
    {
        CColumn column;
        column.CustomData = 0;
        lstrcpy(column.Name, LoadStr(item->NameResID));
        lstrcpy(column.Description, LoadStr(item->DescResID));
        column.GetText = item->GetText;
        column.SupportSorting = item->SupportSorting;
        column.LeftAlignment = item->LeftAlignment;
        column.ID = item->ID;
        CColumnConfig* colCfg = Panel->ViewTemplate->Columns;
        BOOL leftPanel = Panel == MainWindow->LeftPanel;
        column.Width = leftPanel ? colCfg[i].LeftWidth : colCfg[i].RightWidth;
        column.FixedWidth = leftPanel ? colCfg[i].LeftFixedWidth : colCfg[i].RightFixedWidth;
        column.MinWidth = 0; // dummy - will be overwritten during HeaderLine dimensioning

        Panel->Columns.Insert(index, column);
        if (!Panel->Columns.IsGood())
        {
            TRACE_E("CSalamanderView::InsertStandardColumn(): Columns.Insert() failed");
            Panel->Columns.ResetState();
            return FALSE;
        }
        return TRUE;
    }
    else
    {
        TRACE_E("CSalamanderView::InsertStandardColumn(): id=" << id << " is unknown.");
        return FALSE;
    }
}

BOOL CSalamanderView::SetColumnName(int index, const char* name, const char* description)
{
    if (index < 0 || index >= Panel->Columns.Count)
    {
        TRACE_E("CSalamanderView::SetColumnName(): index=" << index << " is out of columns array range.");
        return FALSE;
    }
    if (name == NULL || *name == 0 || description == NULL || *description == 0)
    {
        TRACE_E("CSalamanderView::SetColumnName(): name or description is NULL or empty string.");
        return FALSE;
    }
    if (index == 0 && !Panel->IsExtensionInSeparateColumn() && (Panel->ValidFileData & VALID_DATA_EXTENSION))
    { // Check for double (twice null-terminated) strings + non-emptiness of the second one + their copies
        const char* s = name + strlen(name) + 1;
        const char* beg = s;
        while (s < name + COLUMN_NAME_MAX && *s != 0)
            s++;
        if (s == name + COLUMN_NAME_MAX || beg == s)
        {
            TRACE_E("CSalamanderView::SetColumnName(): name is not double string (names of Name and Ext columns are expected) or second string is empty.");
            return FALSE;
        }
        const char* s2 = description + strlen(description) + 1;
        beg = s2;
        while (s2 < description + COLUMN_DESCRIPTION_MAX && *s2 != 0)
            s2++;
        if (s2 == description + COLUMN_DESCRIPTION_MAX || beg == s2)
        {
            TRACE_E("CSalamanderView::SetColumnName(): description is not double string (descriptions of Name and Ext columns are expected) or second string is empty.");
            return FALSE;
        }
        // copy double strings
        int l = (int)(s - name);
        if (l >= COLUMN_NAME_MAX)
        {
            TRACE_E("CSalamanderView::SetColumnName(): name is too long! (index=" << index << ")");
            l = COLUMN_NAME_MAX - 1;
        }
        char* txt = Panel->Columns[index].Name;
        memcpy(txt, name, l);
        txt[l] = 0;
        l = (int)(s2 - description);
        if (l >= COLUMN_DESCRIPTION_MAX)
        {
            TRACE_E("CSalamanderView::SetColumnName(): desription is too long! (index=" << index << ")");
            l = COLUMN_DESCRIPTION_MAX - 1;
        }
        txt = Panel->Columns[index].Description;
        memcpy(txt, description, l);
        txt[l] = 0;
    }
    else
    {
        lstrcpyn(Panel->Columns[index].Name, name, COLUMN_NAME_MAX);
        lstrcpyn(Panel->Columns[index].Description, description, COLUMN_DESCRIPTION_MAX);
    }
    return TRUE;
}

BOOL CSalamanderView::DeleteColumn(int index)
{
    if (index < 0 || index >= Panel->Columns.Count)
    {
        TRACE_E("CSalamanderView::DeleteColumn(): index=" << index << " is out of columns array range.");
        return FALSE;
    }
    if (index == 0)
    {
        TRACE_E("CSalamanderView::DeleteColumn(): index=" << index << " Name column can't be deleted.");
        return FALSE;
    }
    Panel->Columns.Delete(index);
    if (!Panel->Columns.IsGood())
        Panel->Columns.ResetState(); // cannot fail, only the array was not reduced
    return TRUE;
}

//*****************************************************************************
//
// CFileHistoryItem, CFileHistory
//
// Keeps a list of files that the user has called View or Edit on.
//

CFileHistoryItem::CFileHistoryItem(CFileHistoryItemTypeEnum type, DWORD handlerID, const char* fileName)
{
    Type = type;
    HandlerID = handlerID;
    FileName = DupStr(fileName);
    HIcon = NULL;
    if (FileName == NULL)
        return;

    // trying to extract an icon from the system
    HIcon = GetFileOrPathIconAux(FileName, FALSE, FALSE);
}

CFileHistoryItem::~CFileHistoryItem()
{
    if (FileName != NULL)
        free(FileName);
    if (HIcon != NULL)
        HANDLES(DestroyIcon(HIcon));
}

BOOL CFileHistoryItem::Equal(CFileHistoryItemTypeEnum type, DWORD handlerID, const char* fileName)
{
    return (Type == type && HandlerID == handlerID && lstrcmp(FileName, fileName) == 0);
}

BOOL CFileHistoryItem::Execute()
{
    CALL_STACK_MESSAGE1("CFileHistoryItem::Execute()");
    CFilesWindow* panel = MainWindow->GetActivePanel();
    switch (Type)
    {
    case fhitView:
        panel->ViewFile(FileName, FALSE, HandlerID, -1, -1);
        break;
    case fhitEdit:
        panel->EditFile(FileName, HandlerID);
        break;
    case fhitOpen:
    {
        char buff[MAX_PATH];
        lstrcpy(buff, FileName);
        char* ptr = strrchr(buff, '\\');
        if (ptr != NULL)
        {
            *ptr = 0; // split the path and file name
            HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            MainWindow->SetDefaultDirectories(); // so that the starting process inherits the correct working directory
            ExecuteAssociation(panel->GetListBoxHWND(), buff, ptr + 1);
            SetCursor(hOldCur);
        }
        break;
    }

    default:
        TRACE_E("Unknown Type=" << Type);
    }

    return TRUE;
}

//****************************************************************************
//
// Set of functions for opening associations using SalOpen.exe
//

BOOL SalOpenInit()
{
    // Allocation of shared space in pagefile.sys
    SalOpenFileMapping = HANDLES(CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, // FIXME_X64 are we passing x86/x64 incompatible data?
                                                   MAX_PATH + 200, NULL));
    if (SalOpenFileMapping != NULL)
    {
        SalOpenSharedMem = HANDLES(MapViewOfFile(SalOpenFileMapping, FILE_MAP_WRITE, 0, 0, 0)); // FIXME_X64 are we passing x86/x64 incompatible data?
        if (SalOpenSharedMem == NULL)
            TRACE_E("Unable to allocate shared memory (map view of file) for SalOpen.");
        else
            return TRUE;
    }
    else
        TRACE_E("Unable to allocate shared memory (create file mapping) for SalOpen.");
    return FALSE;
}

BOOL SalOpenExecute(HWND hWindow, const char* fileName)
{
    CALL_STACK_MESSAGE2("SalOpenExecute(, %s)", fileName);

    // Initialization necessary for launching SalOpen
    static BOOL initCalled = FALSE;
    if (!initCalled)
    {
        initCalled = TRUE;
        SalOpenInit();
    }

    if (SalOpenSharedMem != NULL)
    {
        lstrcpyn((char*)SalOpenSharedMem, fileName, MAX_PATH + 200);

        char cmdline[MAX_PATH];
        cmdline[0] = '"';
        if (GetModuleFileName(NULL, cmdline + 1, MAX_PATH - 1) == 0)
            return FALSE;
        char* ptr = strrchr(cmdline, '\\');
        if (ptr == NULL)
            return FALSE;
        *ptr = 0;
        SalPathAppend(cmdline + 1, "utils\\salopen.exe", MAX_PATH - 1);
        char add[100];
        RECT r;
        MultiMonGetClipRectByWindow(GetTopVisibleParent(hWindow), &r, NULL);
        sprintf(add, "\" %u %Iu %u", GetCurrentProcessId(), (DWORD_PTR)SalOpenFileMapping, (DWORD)MAKELPARAM(r.left, r.top));
        if (strlen(cmdline) + strlen(add) >= MAX_PATH)
            return FALSE;
        strcat(cmdline, add);

        // start the process salopen.exe
        STARTUPINFO si;
        memset(&si, 0, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        PROCESS_INFORMATION pi;
        {
            CALL_STACK_MESSAGE1("SalOpenExecute::create-process");
            if (!HANDLES_Q(CreateProcess(NULL, cmdline, NULL, NULL, FALSE,
                                         CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                                         NULL, NULL, &si, &pi)))
            {
                DWORD err = GetLastError();
                TRACE_E("SalOpenExecute failed: \"" << cmdline << "\", " << GetErrorText(err));
                return FALSE;
            }
            else
            {
                { // When waiting, do not associate DDE (.html, .h, .cpp, etc.)
                    //          CALL_STACK_MESSAGE1("SalOpenExecute::wait-for-process");
                    //          WaitForSingleObject(pi.hProcess, INFINITE);
                }
                HANDLES(CloseHandle(pi.hProcess));
                HANDLES(CloseHandle(pi.hThread));
            }
        }

        return TRUE;
    }
    return FALSE;
}

void ReleaseSalOpen()
{
    if (SalOpenSharedMem != NULL)
        HANDLES(UnmapViewOfFile(SalOpenSharedMem));
    if (SalOpenFileMapping != NULL)
        HANDLES(CloseHandle(SalOpenFileMapping));
    SalOpenSharedMem = NULL;
    SalOpenFileMapping = NULL;
}

BOOL IsFileURLPath(const char* path)
{
    if (path == NULL)
        return FALSE;
    // skip whitespaces at the beginning of the string
    const char* s = path;
    while (*s != 0 && *s <= ' ')
        s++;
    // find the name of the file system
    const char* name = s;
    while (*s != 0 && *s != ':' && s - name < 4)
        s++;
    return *s == ':' && s - name == 4 && StrNICmp(name, "file", 4) == 0;
}

BOOL IsPluginFSPath(char* path, char* fsName, char** userPart)
{
    return IsPluginFSPath((const char*)path, fsName, (const char**)userPart);
}

BOOL IsPluginFSPath(const char* path, char* fsName, const char** userPart)
{
    CALL_STACK_MESSAGE2("IsPluginFSPath(%s, ,)", path);

    if (path == NULL)
        return FALSE;
    const char* start = path;
    // skip whitespaces at the beginning of the string
    while (*start >= 1 && *start <= ' ')
        start++;
    // find the name of the file system
    const char* name = start;
    while (LowerCase[*name] >= 'a' && LowerCase[*name] <= 'z' ||
           *name >= '0' && *name <= '9' || *name == '_' || *name == '-' || *name == '+')
        name++;
    // test if the name FS meets all conditions (followed by ':' and >= 2 characters)
    if (*name == ':' && name - start >= 2 && name - start < MAX_PATH)
    {
        // Copy of the FS name
        if (fsName != NULL)
        {
            memmove(fsName, start, name - start);
            fsName[name - start] = 0;
        }
        // pointer to the 'path' to the first character of the plugin-defined path (after the first ':')
        if (userPart != NULL)
            *userPart = name + 1;
        return TRUE;
    }
    else
        return FALSE;
}
