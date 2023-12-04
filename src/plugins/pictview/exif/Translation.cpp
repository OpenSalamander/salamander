// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <tchar.h>
#include <windows.h>
#include <crtdbg.h>
#include <stdlib.h>
#include <stdio.h>
#include "exif.h"

typedef struct tagTextPair
{
    char *orig, *translated;
} sTextPair, *psTextPair;

typedef struct tagTranslInfo
{
    int nItems;
    int nItemsAlloced;
    psTextPair pTexts;
} sTranslInfo;

sTranslInfo g_Translations = {0, 0, NULL};

const char* TranslateText(const char* txt)
{
    int low, mid, hi, cmp;
    psTextPair pTexts = g_Translations.pTexts;

    if (!pTexts || !txt)
    {
        return txt;
    }

    low = 0;
    hi = g_Translations.nItems;

    do
    {
        mid = (hi + low) / 2;
        cmp = strcmp(pTexts[mid].orig, txt);
        if (!cmp)
        {
            return pTexts[mid].translated;
        }
        if (cmp < 0)
        {
            low = mid + 1;
        }
        else
        {
            hi = mid;
        }
    } while (hi > low);
    // not found -> return the original text
    return txt;
}

void FreeTranslations(void)
{
    psTextPair pTexts = g_Translations.pTexts;

    if (!pTexts)
    {
        return;
    }
    for (int i = g_Translations.nItems; i; i--, pTexts++)
    {
        free(pTexts->orig);
        free(pTexts->translated);
    }
    free(g_Translations.pTexts);
    g_Translations.pTexts = NULL;
    g_Translations.nItems = g_Translations.nItemsAlloced = 0;
}

static char* GetRawStr(char* s)
{
    while (*s && (*s != '"'))
        s++;
    if (!*s)
    { // no dbl quot.marks???
        return NULL;
    }
    char* s2 = ++s;
    while (*s2 && (*s2 != '"'))
    {
        if (*s2 == '\\')
        {
            switch (s2[1])
            {
            case '"':
                strcpy(s2, s2++ + 1);
                break;
            case 'n':
                strcpy(s2, s2 + 1);
                s2[0] = '\n';
                break;
            case 'r':
                strcpy(s2, s2 + 1);
                s2[0] = '\r';
                break;
            default:
                _ASSERTE((s2[1] == '"') || (s2[1] == 'n') || (s2[1] == 'r'));
            }
        }
        else
            s2++;
    }
    *s2 = 0;
    return s;
}

void WINAPI ConvertUTF8ToUCS2(const char* in, LPWSTR out)
{
    while (*in)
    {
        if (!(*in & 0x80))
        {
            *out++ = *in++;
        }
        else if ((*in & 0xe0) == 0xc0)
        {
            if (!in[1])
            {
                break; // incomplete 2-byte sequence
            }
            else if ((in[1] & 0xc0) != 0x80)
            {
                break; // not in UCS2
            }
            else
            {
                *out++ = (wchar_t)(((*in & 0x1f) << 6) | (in[1] & 0x3f));
                in += 2;
            }
        }
        else if ((*in & 0xf0) == 0xe0)
        {
            if (!in[1] || !in[2])
            {
                break; // incomplete 3-byte sequence
            }
            else if ((in[1] & 0xc0) != 0x80 || (in[2] & 0xc0) != 0x80)
            {
                break; // not in UCS2
            }
            else
            {
                *out++ = (wchar_t)(((*in & 0x0f) << 12) | ((in[1] & 0x3f) << 6) | (in[2] & 0x3f));
                in += 3;
            }
        }
        else
        {
            break; // not in UCS2
        }
    }
    *out = 0;
}

/* An excerpt from de.po
#: libexif/exif-entry.c:253
#, c-format
msgid ""
"Tag 'UserComment' had invalid format '%s'. Format has been set to "
"'undefined'."
msgstr ""
"Der Tag \"UserComment\" hatte das ungA1ltige Format Â»%sÂ«. Das Format wurde "
"auf \"undefiniert\" geA¤ndert."
*/

#define ITEMS_ALLOCED_AT_A_TIME 32
#define BUF_SIZE 2048

typedef enum eState
{
    TS_LookForID,
    TS_LookForTransl
} eState;

static int compare(const void* arg1, const void* arg2)
{
    psTextPair l = (psTextPair)arg1;
    psTextPair r = (psTextPair)arg2;

    return strcmp(l->orig, r->orig);
}

void WINAPI EXIFInitTranslations(LPCTSTR fname)
{
    FILE* f;
    char* buf;
    char *line, *orig, *transl;
    wchar_t translW[BUF_SIZE];
    eState state = TS_LookForID;
    int skipNext = 0;

    buf = (char*)malloc(3 * BUF_SIZE);
    if (!buf)
    {
        return;
    }
    line = buf;
    orig = buf + BUF_SIZE;
    transl = orig + BUF_SIZE;
    orig[0] = transl[0] = 0;
    f = _tfopen(fname, _T("rt"));

    if (!f)
    {
        free(buf);
        return;
    }
    while (!feof(f))
    {
        char* s;

        if (!fgets(line, BUF_SIZE, f))
        {
            break;
        }
        line[BUF_SIZE - 1] = 0;
        s = line;
        while ((*s == ' ') || (*s == 9))
            s++;
        if (*s == '#')
        { // comment line
            if (strstr(s, ", fuzzy"))
            {
                // Computer-guessed translation, almost certainly wrong -> discard the next one,
                // but still store the current one in orig & transl
                skipNext = 2;
            }
            continue;
        }
        if ((*s == '\r') || (*s == '\n'))
            continue; // empty line
        switch (state)
        {
        case TS_LookForID:
            if (!strncmp(s, "msgid ", 6))
            {
                if (*orig && *transl && strcmp(orig, transl))
                {
                    if (skipNext != 1)
                    {
                        // store translated strings only
                        if (g_Translations.nItems == g_Translations.nItemsAlloced)
                        {
                            psTextPair pTexts = (psTextPair)realloc(g_Translations.pTexts, sizeof(sTextPair) * (g_Translations.nItemsAlloced + ITEMS_ALLOCED_AT_A_TIME));
                            if (pTexts)
                            {
                                g_Translations.pTexts = pTexts;
                                g_Translations.nItemsAlloced += ITEMS_ALLOCED_AT_A_TIME;
                            }
                        }
                        if (g_Translations.nItems <= g_Translations.nItemsAlloced)
                        {
                            psTextPair pTexts = g_Translations.pTexts + g_Translations.nItems++;
                            pTexts->orig = _strdup(orig);
                            if (pTexts->orig)
                            {
                                // CP_UTF8 supported only by newer Windows versions
                                ConvertUTF8ToUCS2(transl, translW);
                                // Note: MBCS is never longer than UTF8 representation -> result always fits & is NULL-terminated
                                WideCharToMultiByte(CP_ACP, 0, translW, -1, transl, BUF_SIZE, NULL, NULL);
                                pTexts->translated = _strdup(transl);
                                if (!pTexts->translated)
                                {
                                    free(pTexts->orig);
                                    g_Translations.nItems--;
                                }
                            }
                            else
                            {
                                g_Translations.nItems--;
                            }
                        }
                    }
                }
                skipNext--;
                orig[0] = transl[0] = 0;
                s = GetRawStr(s + 6 /*skip "msgid "*/);
                if (s)
                {
                    strcpy(orig, s);
                    state = TS_LookForTransl;
                }
            }
            else
            {
                s = GetRawStr(s);
                if (s)
                {
                    // multiline translated text
                    _ASSERTE(BUF_SIZE - strlen(transl) > strlen(s));
                    strncat(transl, s, BUF_SIZE - strlen(transl));
                }
            }
            break;
        case TS_LookForTransl:
            if (!strncmp(s, "msgstr ", 7))
            {
                s = GetRawStr(s + 6);
                if (s)
                {
                    strcpy(transl, s);
                    state = TS_LookForID;
                }
            }
            else
            {
                s = GetRawStr(s);
                if (s)
                {
                    // multiline original text
                    _ASSERTE(BUF_SIZE - strlen(orig) > strlen(s));
                    strncat(orig, s, BUF_SIZE - strlen(orig));
                }
            }
            break;
        }
    }
    fclose(f);
    free(buf);
    qsort(g_Translations.pTexts, g_Translations.nItems, sizeof(g_Translations.pTexts[0]), compare);
}
