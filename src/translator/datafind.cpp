// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndout.h"
#include "wndframe.h"
#include "datarh.h"
#include "config.h"

BOOL IsNotAlphaW(wchar_t ch)
{
    return (!IsCharAlphaW(ch)) && (!IsCharAlphaNumericW(ch));
}

BOOL CData::CompareStrings(const wchar_t* _string, const wchar_t* _pattern)
{
    BOOL wholeWords = Config.FindWords;
    BOOL caseSensitive = Config.FindCase;
    BOOL ignoreAmpersand = Config.FindIgnoreAmpersand;
    BOOL ignoreDash = Config.FindIgnoreDash;

    static wchar_t string[10000];
    static wchar_t pattern[10000];

    int stringLen = wcslen(_string);
    int patternLen = wcslen(_pattern);
    if (stringLen > 9999 || patternLen > 9999)
    {
        TRACE_E("String is too long.");
        return FALSE;
    }

    memcpy(string, _string, (stringLen + 1) * 2);
    memcpy(pattern, _pattern, (patternLen + 1) * 2);

    if (ignoreAmpersand || ignoreDash)
    {
        wchar_t* p;

        int len = stringLen;
        p = string + len - 1;
        while (p >= string)
        {
            if (ignoreAmpersand && *p == L'&' || ignoreDash && *p == L'-')
            {
                memmove(p, p + 1, (len - (p - string)) * 2);
                stringLen--;
            }
            p--;
        }

        len = patternLen;
        p = pattern + len - 1;
        while (p >= pattern)
        {
            if (ignoreAmpersand && *p == L'&' || ignoreDash && *p == L'-')
            {
                memmove(p, p + 1, (len - (p - pattern)) * 2);
                stringLen--;
            }
            p--;
        }
    }

    if (!caseSensitive)
    {
        CharLowerW(string);
        CharLowerW(pattern);
    }

    wchar_t* iterator = string;
    wchar_t* str = NULL;
    do
    {
        str = wcsstr(iterator, pattern);
        if (str == NULL)
            break;
        if (wholeWords)
        {
            if ((str == string || IsNotAlphaW(*(str - 1))) &&
                (*(str + patternLen) == 0 || IsNotAlphaW(*(str + patternLen))))
            {
                break; // found
            }
            iterator = str + patternLen; // zkusime dalsi vyskyt
            str = NULL;
        }
    } while (str == NULL);

    return str != NULL;
}

void CData::Find()
{
    if (Config.FindHistory[0] == NULL)
        return;

    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();

    wchar_t buff[10000];

    // display intro
    swprintf_s(buff, L"Searching for '%ls'...", Config.FindHistory[0]);
    OutWindow.AddLine(buff, mteInfo);

    int i, j;
    WORD lvIndex; // index v ListView

    if (Config.FindTexts || Config.FindOriginal)
    {
        // prohledame dialogy
        for (i = 0; i < DlgData.Count; i++)
        {
            CDialogData* dialog = DlgData[i];
            lvIndex = 0;
            for (j = 0; j < dialog->Controls.Count; j++)
            {
                CControl* control = dialog->Controls[j];
                if (!control->ShowInLVWithControls(j))
                    continue;

                BOOL found = FALSE; // nechceme duplicity
                if (Config.FindTexts)
                {
                    const wchar_t* text = control->TWindowName;
                    if (CompareStrings(text, Config.FindHistory[0]))
                    {
                        swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), text);
                        OutWindow.AddLine(buff, mteInfo, rteDialog, dialog->ID, lvIndex);
                        found = TRUE;
                    }
                }
                if (!found && Config.FindOriginal)
                {
                    const wchar_t* text = control->OWindowName;
                    if (CompareStrings(text, Config.FindHistory[0]))
                    {
                        swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), text);
                        OutWindow.AddLine(buff, mteInfo, rteDialog, dialog->ID, lvIndex);
                    }
                }
                lvIndex++;
            }
        }

        // prohledame menu
        for (i = 0; i < MenuData.Count; i++)
        {
            CMenuData* menu = MenuData[i];
            lvIndex = 0;
            for (j = 0; j < menu->Items.Count; j++)
            {
                CMenuItem* item = &menu->Items[j];
                if (wcslen(item->TString) == 0)
                    continue;

                BOOL found = FALSE; // nechceme duplicity
                if (Config.FindTexts)
                {
                    const wchar_t* text = item->TString;
                    if (CompareStrings(text, Config.FindHistory[0]))
                    {
                        swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), text);
                        OutWindow.AddLine(buff, mteInfo, rteMenu, menu->ID, lvIndex);
                        found = TRUE;
                    }
                }
                if (!found && Config.FindOriginal)
                {
                    const wchar_t* text = item->OString;
                    if (CompareStrings(text, Config.FindHistory[0]))
                    {
                        swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), text);
                        OutWindow.AddLine(buff, mteInfo, rteMenu, menu->ID, lvIndex);
                    }
                }

                lvIndex++;
            }
        }

        // prohledame strings
        for (i = 0; i < StrData.Count; i++)
        {
            CStrData* strData = StrData[i];
            lvIndex = 0;
            for (j = 0; j < 16; j++)
            {
                BOOL found = FALSE; // nechceme duplicity
                if (Config.FindTexts)
                {
                    wchar_t* str = strData->TStrings[j];

                    if (str == NULL || wcslen(str) == 0)
                        continue;

                    if (CompareStrings(str, Config.FindHistory[0]))
                    {
                        swprintf_s(buff, L"String %hs: %ls", DataRH.GetIdentifier((strData->ID - 1) << 4 | j), str);
                        OutWindow.AddLine(buff, mteInfo, rteString, strData->ID, lvIndex);
                        found = TRUE;
                    }
                }
                if (!found && Config.FindOriginal)
                {
                    wchar_t* str = strData->OStrings[j];

                    if (str == NULL || wcslen(str) == 0)
                        continue;

                    if (CompareStrings(str, Config.FindHistory[0]))
                    {
                        swprintf_s(buff, L"String %hs: %ls", DataRH.GetIdentifier((strData->ID - 1) << 4 | j), str);
                        OutWindow.AddLine(buff, mteInfo, rteString, strData->ID, lvIndex);
                    }
                }

                lvIndex++;
            }
        }
    }

    if (Config.FindSymbols)
    {
        for (int ii = 0; ii < DataRH.Items.Count; ii++)
        {
            CDataRHItem* item = &DataRH.Items[ii];

            swprintf_s(buff, L"%hs %d", item->Name, item->ID);
            if (CompareStrings(buff, Config.FindHistory[0]))
            {
                WORD id = item->ID;

                // prohledame dialogy
                for (i = 0; i < DlgData.Count; i++)
                {
                    CDialogData* dialog = DlgData[i];
                    lvIndex = 0;
                    if (dialog->ID == id)
                    {
                        swprintf_s(buff, L"Dialog %hs: <%hs = %d>", DataRH.GetIdentifier(dialog->ID), item->Name, item->ID);
                        OutWindow.AddLine(buff, mteInfo, rteDialog, dialog->ID, lvIndex);
                    }
                    for (j = 0; j < dialog->Controls.Count; j++)
                    {
                        CControl* control = dialog->Controls[j];
                        if (!control->ShowInLVWithControls(j))
                            continue;

                        if (control->ID == id)
                        {
                            swprintf_s(buff, L"Dialog %hs: <%hs = %d>", DataRH.GetIdentifier(dialog->ID), item->Name, item->ID);
                            OutWindow.AddLine(buff, mteInfo, rteDialog, dialog->ID, lvIndex);
                        }
                        lvIndex++;
                    }
                }

                // prohledame menu
                for (i = 0; i < MenuData.Count; i++)
                {
                    CMenuData* menu = MenuData[i];
                    lvIndex = 0;

                    if (menu->ID == id)
                    {
                        swprintf_s(buff, L"Menu %hs: <%hs = %d>", DataRH.GetIdentifier(menu->ID), item->Name, item->ID);
                        OutWindow.AddLine(buff, mteInfo, rteMenu, menu->ID, lvIndex);
                    }
                    for (j = 0; j < menu->Items.Count; j++)
                    {
                        CMenuItem* menuItem = &menu->Items[j];
                        if (wcslen(menuItem->TString) == 0)
                            continue;

                        if (menuItem->ID == id)
                        {
                            swprintf_s(buff, L"Menu %hs: <%hs = %d>", DataRH.GetIdentifier(menu->ID), item->Name, item->ID);
                            OutWindow.AddLine(buff, mteInfo, rteMenu, menu->ID, lvIndex);
                        }
                        lvIndex++;
                    }
                }

                // prohledame strings
                for (i = 0; i < StrData.Count; i++)
                {
                    CStrData* strData = StrData[i];
                    lvIndex = 0;
                    for (j = 0; j < 16; j++)
                    {
                        wchar_t* str = strData->TStrings[j];

                        if (str == NULL || wcslen(str) == 0)
                            continue;

                        WORD strID = ((strData->ID - 1) << 4) | j;

                        if (strID == id)
                        {
                            swprintf_s(buff, L"String %hs: %ls", DataRH.GetIdentifier(strID), str);
                            // sprintf_s(buff, "String (%d): <%s = %d>", strID, item->Name, item->ID);
                            OutWindow.AddLine(buff, mteInfo, rteString, strData->ID, lvIndex);
                        }
                        lvIndex++;
                    }
                }
            }
        }
    }

    // display outro
    DWORD count = OutWindow.GetSelectableLinesCount();
    if (count == 0)
        swprintf_s(buff, L"Cannot find the string '%ls'.", Config.FindHistory[0]);
    else
        swprintf_s(buff, L"%d occurrence(s) have been found.", count);
    OutWindow.AddLine(buff, mteSummary);
    if (count == 0)
        OutWindow.FocusLastItem();
    else
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_WND_OUTPUT, 0); // aktivace Output okna

    OutWindow.EnablePaint(TRUE);

    SetCursor(hOldCursor);
}

void CData::FindUntranslated(BOOL* completelyUntranslated)
{
    HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();

    wchar_t buff[10000];

    // display intro
    swprintf_s(buff, L"Searching for untranslated texts...");
    OutWindow.AddLine(buff, mteInfo);

    int i, j;
    WORD lvIndex; // index v ListView

    BOOL complUntrl = TRUE;

    // prohledame dialogy
    for (i = 0; i < DlgData.Count; i++)
    {
        CDialogData* dialog = DlgData[i];
        lvIndex = 0;
        for (j = 0; j < dialog->Controls.Count; j++)
        {
            CControl* control = dialog->Controls[j];
            if (!control->ShowInLVWithControls(j))
                continue;

            if (control->State == PROGRESS_STATE_UNTRANSLATED)
            {
                swprintf_s(buff, L"Dialog %hs: %ls", DataRH.GetIdentifier(dialog->ID), control->TWindowName);
                OutWindow.AddLine(buff, mteInfo, rteDialog, dialog->ID, lvIndex);
            }
            else
            {
                if (control->OWindowName[0] != 0 ||
                    control->TWindowName != NULL && HIWORD(control->TWindowName) != 0x0000 &&
                        control->TWindowName[0] != 0)
                {
                    complUntrl = FALSE;
                }
            }
            lvIndex++;
        }
    }

    // prohledame menu
    for (i = 0; i < MenuData.Count; i++)
    {
        CMenuData* menu = MenuData[i];
        lvIndex = 0;
        for (j = 0; j < menu->Items.Count; j++)
        {
            CMenuItem* item = &menu->Items[j];
            if (wcslen(item->TString) == 0)
                continue;

            if (item->State == PROGRESS_STATE_UNTRANSLATED)
            {
                swprintf_s(buff, L"Menu %hs: %ls", DataRH.GetIdentifier(menu->ID), item->TString);
                OutWindow.AddLine(buff, mteInfo, rteMenu, menu->ID, lvIndex);
            }
            else
                complUntrl = FALSE;
            lvIndex++;
        }
    }

    // prohledame strings
    for (i = 0; i < StrData.Count; i++)
    {
        CStrData* strData = StrData[i];
        lvIndex = 0;
        for (j = 0; j < 16; j++)
        {
            wchar_t* str = strData->TStrings[j];

            if (str == NULL || wcslen(str) == 0)
                continue;

            if (strData->TState[j] == PROGRESS_STATE_UNTRANSLATED)
            {
                swprintf_s(buff, L"String %hs: %ls", DataRH.GetIdentifier((strData->ID - 1) << 4 | j), str);
                OutWindow.AddLine(buff, mteInfo, rteString, strData->ID, lvIndex);
            }
            else
                complUntrl = FALSE;
            lvIndex++;
        }
    }

    // display outro
    int count = OutWindow.GetInfoLines();
    if (count <= 1)
        swprintf_s(buff, L"All strings seems to be translated.");
    else
        swprintf_s(buff, L"%d occurrence(s) have been found.", count - 1);
    OutWindow.AddLine(buff, mteSummary);
    if (count <= 1)
        PostMessage(FrameWindow.HWindow, WM_FOCLASTINOUTPUTWND, 0, 0); // Output okno jeste nemusi existovat
    else
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_WND_OUTPUT, 0); // aktivace Output okna

    OutWindow.EnablePaint(TRUE);

    SetCursor(hOldCursor);

    if (completelyUntranslated != NULL)
        *completelyUntranslated = complUntrl;
}

void CData::FindDialogsForRelayout()
{
    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();

    wchar_t buff[10000];

    // display intro
    swprintf_s(buff, L"Searching for dialogs needing relayout...");
    OutWindow.AddLine(buff, mteInfo);

    for (int i = 0; i < Relayout.Count; i++)
    {
        swprintf_s(buff, L"Dialog %hs", DataRH.GetIdentifier(Relayout[i]));
        OutWindow.AddLine(buff, mteInfo, rteDialog, Relayout[i]);
    }

    if (Relayout.Count == 0)
        swprintf_s(buff, L"No dialog needs relayout.");
    else
        swprintf_s(buff, L"%d occurrence(s) have been found.", Relayout.Count);
    OutWindow.AddLine(buff, mteSummary);

    OutWindow.EnablePaint(TRUE);
}

void CData::FindDialogsWithOuterControls()
{
    OutWindow.EnablePaint(FALSE);
    OutWindow.Clear();

    wchar_t buff[10000];

    // display intro
    swprintf_s(buff, L"Searching for dialogs with outer controls...");
    OutWindow.AddLine(buff, mteInfo);

    int found = 0;
    for (int i = 0; i < DlgData.Count; i++)
    {
        if (DlgData[i]->HasOuterControls())
        {
            WORD dlgID = DlgData[i]->ID;
            swprintf_s(buff, L"Dialog %hs", DataRH.GetIdentifier(dlgID));
            OutWindow.AddLine(buff, mteInfo, rteDialog, dlgID);
            found++;
        }
    }

    if (found == 0)
        swprintf_s(buff, L"No dialog with outer controls.");
    else
        swprintf_s(buff, L"%d occurrence(s) have been found.", found);
    OutWindow.AddLine(buff, mteSummary);

    OutWindow.EnablePaint(TRUE);
}
