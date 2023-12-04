// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//*****************************************************************************
//
// CStrData
//

CStrData::CStrData()
{
    ID = 0;
    TLangID = 0;
    ZeroMemory(OStrings, sizeof(OStrings));
    ZeroMemory(TStrings, sizeof(TStrings));
    for (int i = 0; i < 16; i++)
        TState[i] = PROGRESS_STATE_UNTRANSLATED;
}

CStrData::~CStrData()
{
    for (int i = 0; i < 16; i++)
    {
        if (OStrings[i] != NULL)
        {
            free(OStrings[i]);
            OStrings[i] = NULL;
        }
        if (TStrings[i] != NULL)
        {
            free(TStrings[i]);
            TStrings[i] = NULL;
        }
    }
}

BOOL CStrData::LoadStrings(WORD group, const wchar_t* original, const wchar_t* translated, CData* data)
{
    const wchar_t* oIter = original;
    const wchar_t* tIter = translated;
    int i;
    for (i = 0; i < 16; i++)
    {
        WORD oLen = *oIter;
        WORD tLen = *tIter;
        if ((oLen == 0 && tLen != 0) || (oLen != 0 && tLen == 0))
        {
            if (!data->MUIMode)
            {
                MessageBox(GetMsgParent(), "Original and translated file has different string ids.",
                           ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
        }
        oIter++;
        tIter++;
        if (oLen == 0)
        {
            OStrings[i] = NULL;
            TStrings[i] = NULL;
        }
        else
        {
            if (!DecodeString(oIter, oLen, &OStrings[i]))
                return FALSE;
            if (!DecodeString(tIter, tLen, &TStrings[i]))
                return FALSE;
            oIter += oLen;
            tIter += tLen;
        }
    }
    WORD state;
    for (i = 0; i < 16; i++)
    {
        if (TStrings[i] != NULL)
        {
            if (!data->MUIMode)
            {
                WORD strID = ((group - 1) << 4) | i;
                state = data->QueryTranslationState(tteStrings, strID, 0, OStrings[i], TStrings[i]);
            }
            else
            {
                state = PROGRESS_STATE_TRANSLATED;
            }
        }
        else
            state = PROGRESS_STATE_UNTRANSLATED;
        TState[i] = state;
    }
    return TRUE;
}

//*****************************************************************************
//
// CData
//

BOOL CData::SaveStrings(HANDLE hUpdateRes)
{
    wchar_t buff[200000];

    for (int i = 0; i < StrData.Count; i++)
    {
        CStrData* strData = StrData[i];
        wchar_t* iter = buff;
        for (int j = 0; j < 16; j++)
        {
            WORD len = 0;
            if (strData->TStrings[j] != NULL)
                len = (WORD)wcslen(strData->TStrings[j]);
            if (len > 0)
            {
                wchar_t c;
                wchar_t *start, *end;
                start = strData->TStrings[j];
                end = start + len;
                WORD strLen = 0;
                while (start < end)
                {
                    if (__GetNextChar(c, start, end))
                    {
                        iter[strLen + 1] = c;
                        strLen++;
                    }
                    else
                        break;
                }
                iter[0] = strLen;
                iter += strLen + 1;
            }
            else
            {
                *iter = 0;
                iter++;
            }
        }

        BOOL result = TRUE;
        if (strData->TLangID != MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)) // resource neni "neutral", musime ho smaznout, aby ve vyslednem .SLG nebyly stringy dva
        {
            result = UpdateResource(hUpdateRes, RT_STRING, MAKEINTRESOURCE(strData->ID),
                                    strData->TLangID,
                                    NULL, 0);
        }
        if (result)
        {
            result = UpdateResource(hUpdateRes, RT_STRING, MAKEINTRESOURCE(strData->ID),
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                                    buff, (iter - buff) * sizeof(wchar_t));
        }
        if (!result)
            return FALSE;
    }
    return TRUE;
}

BOOL CData::StringsAddTranslationStates()
{
    for (int i = 0; i < StrData.Count; i++)
    {
        CStrData* strData = StrData[i];
        for (int j = 0; j < 16; j++)
        {
            if (strData->TStrings[j] != NULL)
            {
                WORD strID = ((strData->ID - 1) << 4) | j;
                if (!AddTranslationState(tteStrings, strID, 0, strData->TState[j]))
                {
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

int CData::FindStrData(WORD id)
{
    for (int i = 0; i < StrData.Count; i++)
        if (id == StrData[i]->ID)
            return i;
    return -1;
}

BOOL CData::FindStringWithID(WORD id, int* index, int* subIndex, int* lvIndex)
{
    // prohledame strings
    for (int i = 0; i < StrData.Count; i++)
    {
        CStrData* strData = StrData[i];
        int lvI = 0;
        for (int j = 0; j < 16; j++)
        {
            wchar_t* str = strData->TStrings[j];

            if (str == NULL || wcslen(str) == 0)
                continue;

            WORD strID = ((strData->ID - 1) << 4) | j;

            if (strID == id)
            {
                if (index != NULL)
                    *index = i;
                if (subIndex != NULL)
                    *subIndex = j;
                if (lvIndex != NULL)
                    *lvIndex = lvI;
                return TRUE;
            }
            lvI++;
        }
    }
    return FALSE;
}

BOOL CData::GetStringWithID(WORD id, wchar_t* buf, int bufSize)
{
    // prohledame strings
    for (int i = 0; i < StrData.Count; i++)
    {
        CStrData* strData = StrData[i];
        for (int j = 0; j < 16; j++)
        {
            wchar_t* str = strData->TStrings[j];

            if (str == NULL || wcslen(str) == 0)
                continue;

            WORD strID = ((strData->ID - 1) << 4) | j;

            if (strID == id)
            {
                lstrcpynW(buf, str, bufSize);
                return TRUE;
            }
        }
    }
    return FALSE;
}
