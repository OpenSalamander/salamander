// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndframe.h"
#include "translator.h"
#include "versinfo.h"

#include "wndout.h"
#include "datarh.h"

/* popis formatu check.lst
Same Size of Property Pages: 300 1105 2645  --seznam IDcek dialogu se oddeluje mezerou, delka seznamu libovolna, ale musi byt v jednom radku

Multi-Text Control: 150: 151: 13712 10149 10151 10153 10154 12087 10155       --1. cislo je dialog ID, 2. je ID controlu u nejz mame zkusit nastavit text na vsechny IDcka v nasledujicim seznamu (jde o test, jestli je control dost velky pro vsechny texty, ktere se v nem muzou objevit)
Multi-Text Control Bold: 150: 151: 13712 10149 10151 10153 10154 12087 10155  --1. cislo je dialog ID, 2. je ID controlu u nejz mame zkusit nastavit text na vsechny IDcka v nasledujicim seznamu (jde o test, jestli je control dost velky pro vsechny texty, ktere se v nem muzou objevit) - text omerovat BOLDem
Text Control Bold: 150: 151 --1. cislo je dialog ID, 2. je ID controlu u nejz mame zkusit, jestli je control dost velky pro obsazeny text - text omerovat BOLDem

Drop-Down Button: 190: 147  --1. cislo je dialog ID, 2. je ID controlu: jde o tlacitko, mame zkontrolovat, jestli se do nej vejde drop-down sipka (vpravo na tlacitku je symbol sipky dolu)

More Button: 190: 147       --1. cislo je dialog ID, 2. je ID controlu: jde o tlacitko, mame zkontrolovat, jestli se do nej vejde more sipka (vpravo na tlacitku je symbol dvou sipek dolu)

Combo Box: 205: 227: 13990 13991 13992 13993  --1. cislo je dialog ID, 2. je ID controlu: jde o combo box, mame zkontrolovat, jestli se do nej vejdou vsechny texty v seznamu

About Dialog Title: 10500: 10501      --specialitka pro About dialogy: 1. cislo je dialog ID, 2. je ID controlu u nejz mame zkusit do jeho textu pres sprintf vlozit verzi: "99.99 beta 10 (PB999 x86)" a text omerovat BOLDem

Progress Dialog Status: 150: 159  --specialitka pro Progress dialog: 1. cislo je dialog ID, 2. je ID controlu u nejz mame zkusit vytvorit text odpovidajici rutine v CProgressDialog::DialogProc(): WM_TIMER: IDT_UPDATESTATUS

Strings With CSV: 10400 10401 10402 10403 10404 10405 10406 10407 10408 --kontrola, ze sedi pocet carek v originalnim a prelozenem textu (jde o CSV=Comma Separated Values, pouziva se pro texty pro bottom baru): kontroluji se postupne stringy v seznamu (vzajemne nemusi mit stejny pocet carek)
*/

BOOL ProcessCheckLstLineAux(const char* overlapHeader, BOOL readDlgAndCtrlID, BOOL readList, int listMinCount,
                            int listMaxCount, const char*& p, const char* lineEnd, CCheckLstItem* item, BOOL& ret)
{
    ret = FALSE;
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
            BOOL ok = TRUE;
            if (readDlgAndCtrlID)
            {
                ok = FALSE;
                item->DialogID = 0;
                const char* numBeg = p;
                while (p < lineEnd && *p >= '0' && *p <= '9')
                    item->DialogID = 10 * item->DialogID + (*p++ - '0');
                if (p > numBeg && p < lineEnd && (*p == ':' || *p == ' ' || *p == '\t'))
                {
                    while (p < lineEnd && (*p == ' ' || *p == '\t'))
                        p++;
                    if (p < lineEnd && *p == ':')
                    {
                        p++;
                        while (p < lineEnd && (*p == ' ' || *p == '\t'))
                            p++;
                        item->ControlID = 0;
                        numBeg = p;
                        while (p < lineEnd && *p >= '0' && *p <= '9')
                            item->ControlID = 10 * item->ControlID + (*p++ - '0');
                        if (p > numBeg && (p == lineEnd || *p == ':' || *p == ' ' || *p == '\t'))
                        {
                            while (p < lineEnd && (*p == ' ' || *p == '\t'))
                                p++;
                            if (readList)
                            {
                                if (p < lineEnd && *p == ':')
                                {
                                    p++;
                                    ok = TRUE;
                                    while (p < lineEnd && (*p == ' ' || *p == '\t'))
                                        p++;
                                }
                            }
                            else
                                ok = TRUE;
                        }
                    }
                }
            }
            if (ok && readList)
            {
                ok = FALSE;
                do
                {
                    WORD dlgID = 0;
                    const char* numBeg = p;
                    while (p < lineEnd && *p >= '0' && *p <= '9')
                        dlgID = 10 * dlgID + (*p++ - '0');
                    if (p > numBeg && (p == lineEnd || *p == ' ' || *p == '\t'))
                    {
                        item->IDList.Add(dlgID);
                        if (!item->IDList.IsGood())
                        {
                            TRACE_E("Low memory");
                            item->IDList.ResetState();
                            return TRUE;
                        }
                    }
                    while (p < lineEnd && (*p == ' ' || *p == '\t'))
                        p++;
                    if (p == lineEnd && item->IDList.Count >= listMinCount &&
                        (listMaxCount == -1 || item->IDList.Count <= listMaxCount))
                    {
                        ok = TRUE;
                    }
                } while (p < lineEnd && *p >= '0' && *p <= '9');
            }
            if (ok && p == lineEnd)
                ret = TRUE;
        }
    }
    return ret;
}

BOOL CData::ProcessCheckLstLine(const char* line, const char* lineEnd, int row)
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
        CCheckLstItem* item = new CCheckLstItem;
        if (item != NULL)
        {
            if (ProcessCheckLstLineAux("Same Size of Property Pages", FALSE, TRUE, 2, -1, p, lineEnd, item, ret))
                item->Type = cltPropPgSameSize;
            else if (ProcessCheckLstLineAux("Multi-Text Control Bold", TRUE, TRUE, 1, -1, p, lineEnd, item, ret))
                item->Type = cltMultiTextControlBold;
            else if (ProcessCheckLstLineAux("Text Control Bold", TRUE, FALSE, 0, -1, p, lineEnd, item, ret))
                item->Type = cltMultiTextControlBold;
            else if (ProcessCheckLstLineAux("Multi-Text Control", TRUE, TRUE, 1, -1, p, lineEnd, item, ret))
                item->Type = cltMultiTextControl;
            else if (ProcessCheckLstLineAux("Drop-Down Button", TRUE, FALSE, 0, -1, p, lineEnd, item, ret))
                item->Type = cltDropDownButton;
            else if (ProcessCheckLstLineAux("More Button", TRUE, FALSE, 0, -1, p, lineEnd, item, ret))
                item->Type = cltMoreButton;
            else if (ProcessCheckLstLineAux("Combo Box", TRUE, TRUE, 1, -1, p, lineEnd, item, ret))
                item->Type = cltComboBox;
            else if (ProcessCheckLstLineAux("About Dialog Title", TRUE, FALSE, 0, -1, p, lineEnd, item, ret))
                item->Type = cltAboutDialogTitle;
            else if (ProcessCheckLstLineAux("Progress Dialog Status", TRUE, FALSE, 0, -1, p, lineEnd, item, ret))
                item->Type = cltProgressDialogStatus;
            else if (ProcessCheckLstLineAux("Strings With CSV", FALSE, TRUE, 1, -1, p, lineEnd, item, ret))
                item->Type = cltStringsWithCSV;

            if (ret)
            {
                CheckLstItems.Add(item);
                if (CheckLstItems.IsGood())
                    item = NULL;
                else
                {
                    TRACE_E("Low memory");
                    CheckLstItems.ResetState();
                    ret = FALSE;
                }
            }
            if (item != NULL)
                delete item;
        }
        else
        {
            TRACE_E("Low memory");
            ret = FALSE;
        }
    }
    return ret;
}

BOOL CData::LoadCheckLst(const char* fileName)
{
    if (fileName[0] != 0)
    {
        wchar_t buff[2 * MAX_PATH];
        swprintf_s(buff, L"Reading CHECKLST file: %hs", fileName);
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

    DWORD checkLstDataSize = GetFileSize(hFile, NULL);
    if (checkLstDataSize == 0xFFFFFFFF)
    {
        char buf[MAX_PATH + 100];
        sprintf_s(buf, "Error reading file %s.", fileName);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    char* checkLstData = (char*)malloc(checkLstDataSize + 1);
    if (checkLstData == NULL)
    {
        TRACE_E("Nedostatek pameti");
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, checkLstData, checkLstDataSize, &read, NULL) || read != checkLstDataSize)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        free(checkLstData);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }
    checkLstData[checkLstDataSize] = 0; // vlozime terminator

    const char* lineStart = checkLstData;
    const char* lineEnd = checkLstData;
    int line = 1;
    while (lineEnd < checkLstData + checkLstDataSize)
    {
        while (*lineEnd != '\r' && *lineEnd != '\n' && lineEnd < checkLstData + checkLstDataSize)
            lineEnd++;

        if (!ProcessCheckLstLine(lineStart, lineEnd, line))
        {
            char errbuf[MAX_PATH + 200];
            char lineText[100];
            lstrcpyn(lineText, lineStart, min(100, (lineEnd - lineStart) + 1));
            sprintf_s(errbuf, "Error reading CheckList data from file:\n"
                              "%s\n"
                              "\n"
                              "Syntax error on line %d, line text:\n"
                              "\n"
                              "%s",
                      fileName, line, lineText);
            MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            //free(checkLstData);
            //HANDLES(CloseHandle(hFile));
            //return FALSE;
        }

        if (lineEnd < checkLstData + checkLstDataSize)
        {
            if (*lineEnd == '\r' && lineEnd + 1 < checkLstData + checkLstDataSize && *(lineEnd + 1) == '\n')
                lineEnd++;
            lineEnd++;
            line++;
            lineStart = lineEnd;
        }
    }
    free(checkLstData);

    HANDLES(CloseHandle(hFile));
    return TRUE;
}

BOOL CCheckLstItem::ContainsDlgID(WORD dlgID)
{
    for (int i = 0; i < IDList.Count; i++)
        if (IDList[i] == dlgID)
            return TRUE;
    return FALSE;
}
