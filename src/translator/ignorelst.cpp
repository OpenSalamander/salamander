// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndframe.h"
#include "translator.h"
#include "versinfo.h"

#include "wndout.h"
#include "datarh.h"

/* popis formatu ignore.lst
Overlap in dialog: 580: 590 x 591      --1. cislo je dialog ID, 2. a 3. jsou IDcka koliznich controlu

Clipped text in dialog: 500: 502       --1. cislo je dialog ID, 2. je ID controlu u nejz mame ignorovat oriznuti textu

Size of icon is small: 750: 776        --1. cislo je dialog ID, 2. je ID ikony, ktera je mala (16x16) a ne velka (32x32), tedy je treba ignorovat velikost a misto ni pouzit 10x10 dlg-units

Hotkeys collision in dialog: 1000: 1004 x 1003  --1. cislo je dialog ID, 2. a 3. jsou IDcka koliznich controlu

Too close to dialog frame: 2500: 2510  --1. cislo je dialog ID, 2. je ID controlu, ktery muze byt az tesne na okraji

Misaligned controls in dialog: 150: 152 x 159  --1. cislo je dialog ID, 2. a 3. jsou IDcka nezarovnanych controlu

Different sized controls in dialog: 560: 567 x 571  --1. cislo je dialog ID, 2. a 3. jsou IDcka mirne se velikosti lisicich controlu

Different spacing between controls: 2500: 2510  --1. cislo je dialog ID, 2. je ID controlu, ktery muze byt nepravidelne umisten mezi ostatnimi

Not standard control size: 2500: 2510  --1. cislo je dialog ID, 2. je ID controlu, ktery muze byt nestandardne veliky

Incorrectly placed label: 560: 567 x 571  --1. cislo je dialog ID, 2. a 3. jsou IDcka nespravne zarovnaneho labelu a jeho controlu

Missing colon at end of text: 1017  -- 1. cislo je ID stringu, ve kterem proti originalu chybi dvojtecka na konci (diskcopy: "...drive %c:." v nemcine "Laufwerk %c: nicht bestimmen." - takze text ending je ruzny, ale chyba to neni)

Inconsistent text endings in control: 535: 1153  --1. cislo je dialog ID, 2. je ID controlu, u ktereho se nekontroluje zakonceni textu proti originalni verzi (prvni az predposledni radek z odstavce)

Inconsistent text endings in string: 10401  -- 1. cislo je ID stringu, u ktereho se nekontroluje zakonceni textu proti originalni verzi

Inconsistent text beginnings in string: 10401  -- 1. cislo je ID stringu, u ktereho se nekontroluje zacatek textu proti originalni verzi

Inconsistent control characters in string: 1210  -- 1. cislo je ID stringu, u ktereho se nekontroluji control-chars (\r, \n, \t) proti originalni verzi

Inconsistent hot keys in string: 12137  -- 1. cislo je ID stringu, u ktereho se nekontroluji hotkeys ('&') proti originalni verzi

Control is progress bar: 2500: 2510  --1. cislo je dialog ID, 2. je ID controlu, na jehoz miste bude progress bar (proste nejde o static text, jak by se mohlo zdat)

Inconsistent format specifier in string: 1210  -- 1. cislo je ID stringu, u ktereho se nekontroluji format-specifiers (%d, %f, atd.) proti originalni verzi

Inconsistent format specifier in control: 2500: 2510  --1. cislo je dialog ID, 2. je ID controlu, u ktereho se nekontroluji format-specifiers (%d, %f, atd.) proti originalni verzi
*/

BOOL ProcessIgnoreLstLineAux(const char* overlapHeader, BOOL readCtrlID1, BOOL readCtrlID2, const char*& p,
                             const char* lineEnd, int& dlgID, int& ctrID1, int& ctrID2,
                             BOOL& ret)
{
    int overlapHeaderLen = strlen(overlapHeader);
    if (lineEnd - p > overlapHeaderLen && _strnicmp(p, overlapHeader, overlapHeaderLen) == 0)
    {
        p += overlapHeaderLen;
        while (p < lineEnd && (*p == ' ' || *p == '\t'))
            p++;
        if (p < lineEnd && *p == ':')
        {
            p++;
            while (p < lineEnd && (*p == ' ' || *p == '\t'))
                p++;
            while (p < lineEnd && *p >= '0' && *p <= '9')
                dlgID = 10 * dlgID + (*p++ - '0');
            while (p < lineEnd && (*p == ' ' || *p == '\t'))
                p++;
            if (readCtrlID1)
            {
                if (p < lineEnd && *p == ':')
                {
                    p++;
                    while (p < lineEnd && (*p == ' ' || *p == '\t'))
                        p++;
                    while (p < lineEnd && *p >= '0' && *p <= '9')
                        ctrID1 = 10 * ctrID1 + (*p++ - '0');
                    while (p < lineEnd && (*p == ' ' || *p == '\t'))
                        p++;

                    if (readCtrlID2)
                    {
                        if (p < lineEnd && *p == 'x')
                        {
                            p++;
                            while (p < lineEnd && (*p == ' ' || *p == '\t'))
                                p++;
                            while (p < lineEnd && *p >= '0' && *p <= '9')
                                ctrID2 = 10 * ctrID2 + (*p++ - '0');
                            while (p < lineEnd && (*p == ' ' || *p == '\t'))
                                p++;
                            if (p == lineEnd)
                                ret = TRUE;
                        }
                    }
                    else
                    {
                        if (p == lineEnd)
                            ret = TRUE;
                    }
                }
            }
            else
            {
                if (p == lineEnd)
                    ret = TRUE;
            }
        }
        return TRUE;
    }
    else
        return FALSE;
}

BOOL CData::ProcessIgnoreLstLine(const char* line, const char* lineEnd, int row)
{
    const char* p = line;

    // preskocim uvodni whitespacy
    while (p < lineEnd && (*p == ' ' || *p == '\t'))
        p++;

    // orizneme whitespacy na konci radku
    while ((lineEnd - 1) > line && (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t'))
        lineEnd--;

    BOOL ret = TRUE;
    if (p < lineEnd) // zajimaji nas jen neprazdne radky
    {
        CIgnoreLstItemType type;
        int dlgID = 0;
        int ctrID1 = 0;
        int ctrID2 = 0;
        ret = FALSE;

        if (ProcessIgnoreLstLineAux("Overlap in dialog", TRUE, TRUE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
            type = iltOverlap;
        else
        {
            if (ProcessIgnoreLstLineAux("Clipped text in dialog", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                type = iltClip;
            else
            {
                if (ProcessIgnoreLstLineAux("Size of icon is small", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                    type = iltSmIcon;
                else
                {
                    if (ProcessIgnoreLstLineAux("Hotkeys collision in dialog", TRUE, TRUE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                        type = iltHotkeysInDlg;
                    else
                    {
                        if (ProcessIgnoreLstLineAux("Too close to dialog frame", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                            type = iltTooClose;
                        else
                        {
                            if (ProcessIgnoreLstLineAux("Misaligned controls in dialog", TRUE, TRUE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                type = iltMisaligned;
                            else
                            {
                                if (ProcessIgnoreLstLineAux("Different sized controls in dialog", TRUE, TRUE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                    type = iltDiffSized;
                                else
                                {
                                    if (ProcessIgnoreLstLineAux("Different spacing between controls", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                        type = iltDiffSpacing;
                                    else
                                    {
                                        if (ProcessIgnoreLstLineAux("Not standard control size", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                            type = iltNotStdSize;
                                        else
                                        {
                                            if (ProcessIgnoreLstLineAux("Incorrectly placed label", TRUE, TRUE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                type = iltIncorPlLbl;
                                            else
                                            {
                                                if (ProcessIgnoreLstLineAux("Missing colon at end of text", FALSE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                    type = iltMisColAtEnd;
                                                else
                                                {
                                                    if (ProcessIgnoreLstLineAux("Inconsistent text endings in control", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                        type = iltInconTxtEnds;
                                                    else
                                                    {
                                                        if (ProcessIgnoreLstLineAux("Inconsistent text endings in string", FALSE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                            type = iltInconStrEnds;
                                                        else
                                                        {
                                                            if (ProcessIgnoreLstLineAux("Inconsistent text beginnings in string", FALSE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                                type = iltInconStrBegs;
                                                            else
                                                            {
                                                                if (ProcessIgnoreLstLineAux("Inconsistent control characters in string", FALSE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                                    type = iltInconCtrlChars;
                                                                else
                                                                {
                                                                    if (ProcessIgnoreLstLineAux("Inconsistent hot keys in string", FALSE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                                        type = iltInconHotKeys;
                                                                    else
                                                                    {
                                                                        if (ProcessIgnoreLstLineAux("Control is progress bar", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                                            type = iltProgressBar;
                                                                        else
                                                                        {
                                                                            if (ProcessIgnoreLstLineAux("Inconsistent format specifier in string", FALSE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                                                type = iltInconFmtSpecif;
                                                                            else
                                                                            {
                                                                                if (ProcessIgnoreLstLineAux("Inconsistent format specifier in control", TRUE, FALSE, p, lineEnd, dlgID, ctrID1, ctrID2, ret))
                                                                                    type = iltInconFmtSpecifCtrl;
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (ret)
        {
            CIgnoreLstItem* item = new CIgnoreLstItem(type, dlgID, ctrID1, ctrID2);
            if (item != NULL)
            {
                IgnoreLstItems.Add(item);
                if (IgnoreLstItems.IsGood())
                    item = NULL;
                else
                {
                    TRACE_E("Low memory");
                    IgnoreLstItems.ResetState();
                    ret = FALSE;
                }
            }
            else
            {
                TRACE_E("Low memory");
                ret = FALSE;
            }
            if (item != NULL)
                delete item;
        }
    }
    return ret;
}

BOOL CData::LoadIgnoreLst(const char* fileName)
{
    if (fileName[0] != 0)
    {
        wchar_t buff[2 * MAX_PATH];
        swprintf_s(buff, L"Reading IGNORELST file: %hs", fileName);
        OutWindow.AddLine(buff, mteInfo);
    }
    else
        return TRUE;

    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error opening file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    DWORD ignoreLstDataSize = GetFileSize(hFile, NULL);
    if (ignoreLstDataSize == 0xFFFFFFFF)
    {
        char buf[MAX_PATH + 100];
        sprintf_s(buf, "Error reading file %s.", fileName);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    char* ignoreLstData = (char*)malloc(ignoreLstDataSize + 1);
    if (ignoreLstData == NULL)
    {
        TRACE_E("Nedostatek pameti");
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, ignoreLstData, ignoreLstDataSize, &read, NULL) || read != ignoreLstDataSize)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        free(ignoreLstData);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }
    ignoreLstData[ignoreLstDataSize] = 0; // vlozime terminator

    const char* lineStart = ignoreLstData;
    const char* lineEnd = ignoreLstData;
    int line = 1;
    while (lineEnd < ignoreLstData + ignoreLstDataSize)
    {
        while (*lineEnd != '\r' && *lineEnd != '\n' && lineEnd < ignoreLstData + ignoreLstDataSize)
            lineEnd++;

        if (!ProcessIgnoreLstLine(lineStart, lineEnd, line))
        {
            char errbuf[MAX_PATH + 100];
            sprintf_s(errbuf, "Error reading IgnoreList data from file\n"
                              "%s\n"
                              "\n"
                              "Syntax error on line %d",
                      fileName, line);
            MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            free(ignoreLstData);
            HANDLES(CloseHandle(hFile));
            return FALSE;
        }

        if (lineEnd < ignoreLstData + ignoreLstDataSize)
        {
            if (*lineEnd == '\r' && lineEnd + 1 < ignoreLstData + ignoreLstDataSize && *(lineEnd + 1) == '\n')
                lineEnd++;
            lineEnd++;
            line++;
            lineStart = lineEnd;
        }
    }
    free(ignoreLstData);

    HANDLES(CloseHandle(hFile));
    return TRUE;
}

BOOL CData::IgnoreProblem(CIgnoreLstItemType type, WORD dialogID, WORD controlID1, WORD controlID2)
{
    for (int i = 0; i < IgnoreLstItems.Count; i++)
    {
        CIgnoreLstItem* item = IgnoreLstItems[i];
        if (type == item->Type && dialogID == item->DialogID &&
            (type == iltMisColAtEnd || type == iltInconStrEnds || type == iltInconStrBegs ||
             type == iltInconCtrlChars || type == iltInconFmtSpecif || type == iltInconHotKeys ||
             controlID1 == item->ControlID1) &&
            (type != iltOverlap && type != iltHotkeysInDlg && type != iltMisaligned &&
                 type != iltDiffSized && type != iltIncorPlLbl ||
             controlID2 == item->ControlID2))
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CData::IgnoreIconSizeIconIsSmall(WORD dialogID, WORD controlID)
{
    return IgnoreProblem(iltSmIcon, dialogID, controlID, 0);
}

BOOL CData::IgnoreDlgHotkeysConflict(WORD dialogID, WORD controlID1, WORD controlID2)
{
    return IgnoreProblem(iltHotkeysInDlg, dialogID, controlID1, controlID2) ||
           IgnoreProblem(iltHotkeysInDlg, dialogID, controlID2, controlID1);
}

BOOL CData::IgnoreTooCloseToDlgFrame(WORD dialogID, WORD controlID)
{
    return IgnoreProblem(iltTooClose, dialogID, controlID, 0);
}

BOOL CData::IgnoreStaticItIsProgressBar(WORD dialogID, WORD controlID)
{
    return IgnoreProblem(iltProgressBar, dialogID, controlID, 0);
}

BOOL CData::IgnoreDifferentSpacing(WORD dialogID, WORD controlID)
{
    return IgnoreProblem(iltDiffSpacing, dialogID, controlID, 0);
}

BOOL CData::IgnoreNotStdSize(WORD dialogID, WORD controlID)
{
    return IgnoreProblem(iltNotStdSize, dialogID, controlID, 0);
}

BOOL CData::IgnoreMissingColonAtEnd(WORD stringID)
{
    return IgnoreProblem(iltMisColAtEnd, stringID, 0, 0);
}

BOOL CData::IgnoreInconTxtEnds(WORD dialogID, WORD controlID)
{
    return IgnoreProblem(iltInconTxtEnds, dialogID, controlID, 0);
}

BOOL CData::IgnoreInconStringEnds(WORD stringID)
{
    return IgnoreProblem(iltInconStrEnds, stringID, 0, 0);
}

BOOL CData::IgnoreInconStringBegs(WORD stringID)
{
    return IgnoreProblem(iltInconStrBegs, stringID, 0, 0);
}

BOOL CData::IgnoreInconCtrlChars(WORD stringID)
{
    return IgnoreProblem(iltInconCtrlChars, stringID, 0, 0);
}

BOOL CData::IgnoreInconHotKeys(WORD stringID)
{
    return IgnoreProblem(iltInconHotKeys, stringID, 0, 0);
}

BOOL CData::IgnoreInconFmtSpecif(WORD stringID)
{
    return IgnoreProblem(iltInconFmtSpecif, stringID, 0, 0);
}

BOOL CData::IgnoreInconFmtSpecifCtrl(WORD dialogID, WORD controlID)
{
    return IgnoreProblem(iltInconFmtSpecifCtrl, dialogID, controlID, 0);
}
