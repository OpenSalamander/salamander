// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndframe.h"
#include "translator.h"
#include "versinfo.h"

#include "wndout.h"
#include "datarh.h"

/* ignore.lst format description
Overlap in dialog: 580: 590 x 591      -- first number is the dialog ID; the second and third are IDs of the overlapping controls

Clipped text in dialog: 500: 502       -- first number is the dialog ID; the second is the control ID whose clipping should be ignored

Size of icon is small: 750: 776        -- first number is the dialog ID; the second is the icon ID that stays small (16x16) instead of large (32x32), so the size check must be skipped and 10x10 dialog units used instead

Hotkeys collision in dialog: 1000: 1004 x 1003  -- first number is the dialog ID; the second and third are IDs of controls with colliding hotkeys

Too close to dialog frame: 2500: 2510  -- first number is the dialog ID; the second is the control ID that may sit right at the edge

Misaligned controls in dialog: 150: 152 x 159  -- first number is the dialog ID; the second and third are IDs of misaligned controls

Different sized controls in dialog: 560: 567 x 571  -- first number is the dialog ID; the second and third are IDs of controls whose sizes differ slightly

Different spacing between controls: 2500: 2510  -- first number is the dialog ID; the second is the control ID that may be spaced irregularly among its neighbours

Not standard control size: 2500: 2510  -- first number is the dialog ID; the second is the control ID that can have a non-standard size

Incorrectly placed label: 560: 567 x 571  -- first number is the dialog ID; the second and third are IDs of a label and its control that may be misaligned

Missing colon at end of text: 1017  -- first number is the string ID that intentionally lacks the colon present in the source (diskcopy: "...drive %c:." versus German "Laufwerk %c: nicht bestimmen."), so the different ending is acceptable

Inconsistent text endings in control: 535: 1153  -- first number is the dialog ID; the second is the control ID whose paragraph ending is not compared with the source (we allow differences from the first up to the penultimate line)

Inconsistent text endings in string: 10401  -- first number is the string ID whose ending is not compared with the source version

Inconsistent text beginnings in string: 10401  -- first number is the string ID whose beginning is not compared with the source version

Inconsistent control characters in string: 1210  -- first number is the string ID whose control characters (\r, \n, \t) are not compared with the source version

Inconsistent hot keys in string: 12137  -- first number is the string ID whose hotkeys ('&') are not compared with the source version

Control is progress bar: 2500: 2510  -- first number is the dialog ID; the second is the control ID that represents a progress bar (so it is not a static text as one might assume)

Inconsistent format specifier in string: 1210  -- first number is the string ID whose format specifiers (%d, %f, etc.) are not compared with the source version

Inconsistent format specifier in control: 2500: 2510  -- first number is the dialog ID; the second is the control ID whose format specifiers (%d, %f, etc.) are not compared with the source version
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

    // skip leading whitespace
    while (p < lineEnd && (*p == ' ' || *p == '\t'))
        p++;

    // trim whitespace at the end of the line
    while ((lineEnd - 1) > line && (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t'))
        lineEnd--;

    BOOL ret = TRUE;
    if (p < lineEnd) // only non-empty lines are relevant
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
        TRACE_E("Out of memory");
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
    ignoreLstData[ignoreLstDataSize] = 0; // append a terminator

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
