// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndrh.h"
#include "wndout.h"
#include "translator.h"
#include "datarh.h"

#include "oe_data.h"

CDataRH DataRH;

// propojeni na OPENEDIT pro otevirani souboru v MSVC

BOOL CALLBACK FindMSVCEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    char title[200];
    if (GetWindowText(hwnd, title, 200) != 0)
    {
        if (strstr(title, "Microsoft Visual C++") != NULL)
        {
            *((HWND*)lParam) = hwnd;
            return FALSE; // uz jsme ho nasli
        }
    }
    return TRUE;
}

void GotoEditor(const char* fileName, int row)
{
    HANDLE mailSlot = CreateFile(MAILSLOT_NAME, GENERIC_WRITE, FILE_SHARE_READ,
                                 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (mailSlot != INVALID_HANDLE_VALUE)
    {
        // povolime pouziti SetForegroundWindow, jinak se MSVC nebude moct fokusnout
        HMODULE USER32DLL = LoadLibrary("user32.dll");
        if (USER32DLL != NULL)
        {
            typedef BOOL(WINAPI * FAllowSetForegroundWindow)(DWORD dwProcessId);
            FAllowSetForegroundWindow allowSetForegroundWindow = (FAllowSetForegroundWindow)GetProcAddress(USER32DLL, "AllowSetForegroundWindow");
            if (allowSetForegroundWindow != NULL)
            {
                HWND hwnd = NULL;
                EnumWindows(FindMSVCEnumWindowsProc, (LPARAM)&hwnd);
                DWORD MSVC_PID;
                if (hwnd != NULL)
                    GetWindowThreadProcessId(hwnd, &MSVC_PID);
                allowSetForegroundWindow(MSVC_PID);
            }
            FreeLibrary(USER32DLL);
        }

        COpenEditPacket packet; // packet s informacemi pro server
        lstrcpyn(packet.File, fileName, MAX_PATH);
        packet.File[MAX_PATH - 1] = 0;
        packet.Line = row;
        packet.Column = 0;
        DWORD written;
        if (WriteFile(mailSlot, &packet, sizeof(packet), &written, NULL) &&
            written == sizeof(packet)) // zapis packetu
        {
            HANDLE check = OpenEvent(EVENT_MODIFY_STATE, FALSE, CHECK_MAILSLOT_EVENT_NAME);
            if (check != NULL)
            {
                SetEvent(check); // notifikace o pridani zpravy do slotu
                CloseHandle(check);
            }
            else
                TRACE_E("Unable to open check-mailslot event");
        }
        else
            TRACE_E("Unable to write to mailslot");
        CloseHandle(mailSlot);
    }
    else
        TRACE_E("Unable to open mailslot");
}

//*****************************************************************************
//
// CDataRH
//

CDataRH::CDataRH()
    : Items(1000, 500), FileMarks(10, 5), IncompleteItems(10, 5)
{
    Data = NULL;
    DataSize = 0;
    ContainsUnknownIdentifier = FALSE;
}

CDataRH::~CDataRH()
{
    Clean();
}

void CDataRH::Clean()
{
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        if (Items[i].Name != NULL)
            free(Items[i].Name);
    }
    Items.DestroyMembers();

    for (i = 0; i < FileMarks.Count; i++)
    {
        if (FileMarks[i].Name != NULL)
            free(FileMarks[i].Name);
    }
    FileMarks.DestroyMembers();

    CleanIncompleteItems();

    if (Data != NULL)
    {
        free(Data);
        Data = NULL;
        DataSize = 0;
    }
}

void CDataRH::CleanIncompleteItems()
{
    for (int i = 0; i < IncompleteItems.Count; i++)
    {
        if (IncompleteItems[i].Name != NULL)
            free(IncompleteItems[i].Name);
        if (IncompleteItems[i].SymbVal != NULL)
            free(IncompleteItems[i].SymbVal);
    }
    IncompleteItems.DestroyMembers();
}

void CDataRH::SortItems(int left, int right)
{
    int i = left, j = right;
    WORD pivotID = Items[(i + j) / 2].ID;

    do
    {
        while ((Items[i].ID < pivotID) && i < right)
            i++;
        while ((pivotID < Items[j].ID) && j > left)
            j--;

        if (i <= j)
        {
            CDataRHItem swap = Items[i];
            Items[i] = Items[j];
            Items[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    if (left < j)
        SortItems(left, j);
    if (i < right)
        SortItems(i, right);
}

BOOL CDataRH::GetIDIndex(WORD id, int& index)
{
    if (Items.Count == 0)
    {
        ContainsUnknownIdentifier = TRUE;
        return FALSE;
    }

    int l = 0, r = Items.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = Items[m].ID - id;
        if (res == 0)
        {
            index = m;
            return TRUE; // nalezeno
        }
        else
        {
            if (res > 0)
            {
                if (l == r || l > m - 1)
                {
                    ContainsUnknownIdentifier = TRUE;
                    return FALSE; // nenalezeno
                }
                r = m - 1;
            }
            else
            {
                if (l == r)
                {
                    ContainsUnknownIdentifier = TRUE;
                    return FALSE; // nenalezeno
                }
                l = m + 1;
            }
        }
    }
}

const char*
CDataRH::GetIdentifier(WORD id, BOOL inclNum)
{
    static char buff[5000];
    static char* actBuf = buff;
    // musime vypnout plneni celeho bufferu v sprintf_s bajtem 0xFD, jinak pri
    // presunu 'actBuf' zpet na zacatek 'buff' dojde k promaznuti celeho bufferu buff
    // a tim ke ztrate predchozich retezcu
    size_t prevFillTr = _CrtSetDebugFillThreshold(0);
    int index;
    if (GetIDIndex(id, index))
    {
        int first = index;
        while (first > 0 && Items[first - 1].ID == id)
            first--;
        int last = index;
        while (last + 1 < Items.Count && Items[last + 1].ID == id)
            last++;
        if (first < last)
        {
            char* curBuf = actBuf;
            for (int i = first; i <= last; i++)
            {
                int res = sprintf_s(curBuf, _countof(buff) - (curBuf - buff), i < last ? "%s / " : "%s", Items[i].Name);
                if (res != -1)
                    curBuf += res;
            }
            if (inclNum)
                sprintf_s(curBuf, _countof(buff) - (curBuf - buff), " <%d>", id);
        }
        else
        {
            if (!inclNum)
            {
                _CrtSetDebugFillThreshold(prevFillTr);
                return Items[index].Name;
            }
            sprintf_s(actBuf, _countof(buff) - (actBuf - buff), "%s <%d>", Items[index].Name, id);
        }
    }
    else
        sprintf_s(actBuf, _countof(buff) - (actBuf - buff), "<%d>", id);
    char* ret = actBuf;
    actBuf += strlen(actBuf) + 1;
    if (actBuf > buff + _countof(buff) - 500)
        actBuf = buff;
    _CrtSetDebugFillThreshold(prevFillTr);
    return ret;
}

void CDataRH::ShowIdentifier(WORD id)
{
    int index;
    if (RHWindow.ListBox.HWindow != NULL && GetIDIndex(id, index))
    {
        int row = Items[index].Row - 1;
        ListView_SetItemState(RHWindow.ListBox.HWindow, row, (LVIS_FOCUSED | LVIS_SELECTED), (LVIS_FOCUSED | LVIS_SELECTED));
        int count = ListView_GetItemCount(RHWindow.ListBox.HWindow);
        ListView_EnsureVisible(RHWindow.ListBox.HWindow, count - 1, FALSE);
        ListView_EnsureVisible(RHWindow.ListBox.HWindow, max(0, row - RHWindow.VisibleItems / 2), FALSE);
    }
}

BOOL CDataRH::FindEqualItems()
{
    CDataRHItem* last = NULL;
    for (int i = 0; i < Items.Count; i++)
    {
        if (last != NULL && Items[i].ID == last->ID)
        {
            wchar_t buff[2000];
            swprintf_s(buff, L"SYMBOLS file: ID (%d) conflict on row %d with row %d: %hs / %hs",
                       last->ID, Items[i].Row, last->Row, Items[i].Name, last->Name);
            OutWindow.AddLine(buff, mteWarning);
        }
        last = &Items[i];
    }
    return TRUE;
}

BOOL CDataRH::GetIDForIdentifier(const char* identifier, WORD* id)
{
    // cache s jednim prvkem (jinak kvadraticka slozitost)
    int static LastIDIndex = -1;
    if (LastIDIndex > 0 && LastIDIndex < Items.Count)
    {
        if (strcmp(Items[LastIDIndex].Name, identifier) == 0)
        {
            *id = Items[LastIDIndex].ID;
            return TRUE;
        }
    }

    for (int i = 0; i < Items.Count; i++)
    {
        if (strcmp(Items[i].Name, identifier) == 0)
        {
            LastIDIndex = i;
            if (id != NULL)
                *id = Items[LastIDIndex].ID;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CDataRH::GetID(const char* param, int row, WORD* id, BOOL* isIncomplete, CDataRHIncompleteItem* incomplItem)
{
    *isIncomplete = FALSE;

    // vyhodime white spaces na zacatku
    const char* p = param;
    while (*p == ' ' || *p == '\t')
        p++;
    BOOL openParenthesis = FALSE;
    if (*p == '(')
    {
        p++;
        openParenthesis = TRUE;
    }

    char buff[1000];
    strcpy_s(buff, p);

    // a na konci
    char* p2 = buff + strlen(buff) - 1;
    while (p2 >= buff && (*p2 == ' ' || *p2 == '\t'))
        p2--;
    if (p > buff)
    {
        *(p2 + 1) = 0;

        // jde o cislo?
        BOOL isNumber = TRUE;
        p = buff;
        while (*p != 0)
        {
            if (*p < '0' || *p > '9')
            {
                isNumber = FALSE;
                break;
            }
            p++;
        }

        if (isNumber)
        {
            *id = atoi(buff);
            return TRUE;
        }

        // jde pravdepodobne o symbolickou hodnotu, zkusime ji urcit
        if (*p2 == ')')
        {
            if (openParenthesis)
                *p2 = 0;
            else
                return FALSE; // mame zaviraci zavorku, ale ne oteviraci
        }

        // podporujeme tvar "SYMBOLIC_CONST" a "SYMBOLIC_CONST + xxx"
        char symbolicName[1000];
        p = buff;

        while ((*p >= 'a' && *p <= 'z') ||
               (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') ||
               *p == '_')
            p++;
        memcpy(symbolicName, buff, (p - buff));
        symbolicName[p - buff] = 0;

        WORD foundID;

        if (!GetIDForIdentifier(symbolicName, &foundID))
        {
            int len = strlen(symbolicName) + 1;
            incomplItem->SymbVal = (char*)malloc(len);
            if (incomplItem->SymbVal == NULL)
            {
                TRACE_E("Low memory");
                return FALSE;
            }
            strcpy_s(incomplItem->SymbVal, len, symbolicName);
            incomplItem->AddConst = 0;
            *isIncomplete = TRUE;
            foundID = 0;
        }

        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == 0)
        {
            *id = foundID;
            return TRUE;
        }

        if (*p == '+')
        {
            p++;
            while (*p == ' ' || *p == '\t')
                p++;

            // nyni musi nasledovat cislo
            isNumber = TRUE;
            const char* p3 = p;
            while (*p3 != 0)
            {
                if (*p3 < '0' || *p3 > '9')
                {
                    isNumber = FALSE;
                    break;
                }
                p3++;
            }

            if (isNumber)
            {
                int num = atoi(p);
                if (*isIncomplete)
                    incomplItem->AddConst = num;
                *id = foundID + num;
                return TRUE;
            }
        }
    }
    if (*isIncomplete)
    {
        free(incomplItem->SymbVal);
        *isIncomplete = FALSE;
    }
    return FALSE;
}

BOOL CDataRH::ProcessLine(const char* line, const char* lineEnd, int row)
{
    // pokud jde o komentar nebo prazdny radek, ignorujeme jej
    const char* p = line;

    // preskocim uvodni mezery
    while (p < lineEnd && (*p == ' ' || *p == '\t'))
        p++;

    if (p >= lineEnd)
        return TRUE;

    if (*p == '/')
    {
        p++;
        if (p < lineEnd && *p == '/')
        {
            p++;
            if (p < lineEnd && *p == '/')
            {
                p++;
                if (p < lineEnd && *p == '*')
                {
                    // file mark

                    p++;
                    // preskocim uvodni mezery
                    while (p < lineEnd && (*p == ' ' || *p == '\t'))
                        p++;

                    if (p < lineEnd)
                    {
                        int filePathLen = lineEnd - p;
                        CDataRHItem mark;
                        mark.ID = 0; //dummy
                        mark.Row = row;
                        mark.Name = (char*)malloc(filePathLen + 1);
                        if (mark.Name == NULL)
                        {
                            TRACE_E("Low memory");
                            return FALSE;
                        }
                        strncpy_s(mark.Name, filePathLen + 1, p, filePathLen);

                        //orizneme mezery z konce
                        while (filePathLen - 1 > 0 && (mark.Name[filePathLen - 1] == ' ' || mark.Name[filePathLen - 1] == '\t'))
                        {
                            filePathLen--;
                            mark.Name[filePathLen] = 0;
                        }

                        FileMarks.Add(mark);
                        if (!FileMarks.IsGood())
                        {
                            free(mark.Name);
                            TRACE_E("Low memory");
                            return FALSE;
                        }
                    }
                }
            }
            return TRUE; // jde o komentar
        }
    }
    else
    {
        if (*p == '#')
        {
            // napocitam pocet znaku do mezery nebo konce radku
            const char* iter = p;
            while (iter < lineEnd && *iter != ' ' && *iter != '\t')
                iter++;

            // zahodime vybrana klicova slova
            if (p + 6 < lineEnd && strncmp(p + 1, "pragma", 6) == 0)
                return TRUE;
            if (p + 6 < lineEnd && strncmp(p + 1, "ifndef", 6) == 0)
                return TRUE;
            if (p + 5 < lineEnd && strncmp(p + 1, "endif", 5) == 0)
                return TRUE;
            if (p + 5 < lineEnd && strncmp(p + 1, "error", 5) == 0)
                return TRUE;
            if (p + 2 < lineEnd && strncmp(p + 1, "if", 2) == 0)
                return TRUE;
            if (p + 7 < lineEnd && strncmp(p + 1, "include", 7) == 0)
                return TRUE;

            // zahodime pomocne konstanty MSVC resource editoru
            if (p + 17 < lineEnd && strncmp(p + 1, "define _APS_NEXT_", 17) == 0)
                return TRUE;

            if (p + 6 < lineEnd && strncmp(p + 1, "define", 6) == 0)
            {
                // hledame nazev konstanty
                while (iter < lineEnd && (*iter == ' ' || *iter == '\t'))
                    iter++;
                if (iter < lineEnd && *iter != '/')
                {
                    const char* name = iter;
                    int nameLen = 0;
                    while (iter < lineEnd && *iter != ' ' && *iter != '\t' && *iter != '/')
                        iter++;
                    nameLen = iter - name;

                    if (*iter == '/')
                        return TRUE;

                    // hledame konstantu
                    while (iter < lineEnd && (*iter == ' ' || *iter == '\t'))
                        iter++;
                    if (iter < lineEnd)
                    {
                        const char* id = iter;
                        int idLen = 0;
                        while (iter < lineEnd && *iter != '/')
                            iter++;
                        idLen = iter - id;

                        if (nameLen > 999 || idLen > 999)
                        {
                            TRACE_E("Too long line " << row << " : " << p);
                            return FALSE;
                        }

                        if (idLen > 0)
                        {
                            char buff[1000];
                            strncpy_s(buff, id, idLen);
                            BOOL isIncomplete = FALSE;
                            CDataRHIncompleteItem incomplItem;
                            CDataRHItem item;
                            if (GetID(buff, row, &item.ID, &isIncomplete, &incomplItem))
                            {
                                if (isIncomplete)
                                {
                                    incomplItem.Name = (char*)malloc(nameLen + 1);
                                    if (incomplItem.Name == NULL)
                                    {
                                        free(incomplItem.SymbVal);
                                        TRACE_E("Low memory");
                                        return FALSE;
                                    }
                                    strncpy_s(incomplItem.Name, nameLen + 1, name, nameLen);

                                    incomplItem.Row = row;

                                    IncompleteItems.Add(incomplItem);
                                    if (!IncompleteItems.IsGood())
                                    {
                                        free(incomplItem.Name);
                                        free(incomplItem.SymbVal);
                                        TRACE_E("Low memory");
                                        return FALSE;
                                    }
                                }
                                else
                                {
                                    item.Name = (char*)malloc(nameLen + 1);
                                    if (item.Name == NULL)
                                    {
                                        TRACE_E("Low memory");
                                        return FALSE;
                                    }
                                    strncpy_s(item.Name, nameLen + 1, name, nameLen);

                                    item.Row = row;

                                    Items.Add(item);
                                    if (!Items.IsGood())
                                    {
                                        free(item.Name);
                                        TRACE_E("Low memory");
                                        return FALSE;
                                    }
                                }
                            }
                            else
                            {
                                wchar_t buffW[2 * MAX_PATH];
                                char nameBuf[1000];
                                lstrcpyn(nameBuf, name, nameLen + 1);
                                swprintf_s(buffW, L"SYMBOLS file: invalid ID (%hs) on row %d", nameBuf, row);
                                OutWindow.AddLine(buffW, mteError);
                            }
                        }
                    }
                }
                return TRUE;
            }
        }
    }
    return FALSE;
}

// preskoci mezery, tabelatory a EOLy, vraci TRUE pokud 's' nedospelo ke konci retezce ('end')
BOOL SkipWSAndEOLs(const char*& s, const char* end, int* line)
{
    while (s < end && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n'))
    {
        if (*s == '\r' || *s == '\n')
            *line += 1;
        if (*s == '\r' && (s + 1) < end && *(s + 1) == '\n')
            s++;
        s++;
    }
    return s < end;
}

// preskoci mezery, tabelatory, EOLy a komentare, vraci TRUE pokud 's' nedospelo ke konci retezce ('end')
// po EOLu musi nalsedovat radek uvedeny sekvenci "//*"
BOOL SkipWSAndEOLAndCmnt(const char*& s, const char* end, int* line)
{
AGAIN:
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    if (s < end)
    {
        if (*s == '#') // sezobneme komentare
        {
            while (s < end && *s != '\r' && *s != '\n')
                s++;
        }
        if (s < end)
        {

            if (*s == '\r' || *s == '\n')
                *line += 1;
            else
                return s < end;

            if (*s == '\r' && (s + 1) < end && *(s + 1) == '\n')
                s++;
            s++;
            if (s < end && *s == '/')
            {
                s++;
                if (s < end && *s == '/')
                {
                    s++;
                    if (s < end && *s == '*')
                    {
                        s++;
                        if (s < end && (*s == ' ' || *s == '\t' || *s == '#' || *s == '\r' || *s == '\n'))
                            goto AGAIN;
                        return s < end;
                    }
                }
            }
        }
    }
    return FALSE;
}

// preskoci mezery a tabelatory, vraci TRUE pokud 's' nedospelo ke konci retezce ('end')
BOOL SkipWS(const char*& s, const char* end)
{
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    return s < end;
}

// preskoci zbytek radky, vraci TRUE pokud 's' nedospelo ke konci retezce ('end')
BOOL SkipRestOfLine(const char*& s, const char* end)
{
    while (s < end && *s != '\r' && *s != '\n')
        s++;
    return s < end;
}

// preskoci komentar // nebo /* */, vraci TRUE pokud 's' nedospelo ke konci retezce ('end')
BOOL SkipComment(const char*& s, const char* end, int* line)
{
    // s ukazuje na prvni znak '/', ktery preskocime
    s++;
    // nasledovat muze znak '/' nebo '*'
    if (s < end)
    {
        if (*s == '/')
            SkipRestOfLine(s, end); // preskocime "// ---"
        else
        {
            if (*s == '*') // preskocime "/* --- */"
            {
                while (s < end)
                {
                    if (*s == '*' && (s + 1) < end && *(s + 1) == '/') // konec komentare
                        break;
                    if (*s == '\r' || *s == '\n')
                        *line += 1;
                    if (*s == '\r' && (s + 1) < end && *(s + 1) == '\n')
                        s++;
                    s++;
                }
            }
        }
    }
    return s < end;
}

BOOL SkipIdentifier(const char*& s, const char* end, int* errorResID, int emptyErrID)
{
    if (s < end && (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z' || *s == '_'))
    {
        s++;
        while (s < end && (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z' ||
                           *s >= '0' && *s <= '9' || *s == '_'))
            s++;
        return TRUE;
    }
    else
    {
        *errorResID = emptyErrID;
        return FALSE;
    }
}

static const char* FunctionNames[] =
    {
        "Layout",
        "MeasureWidthChange",
        "Move",
        "Resize",
        NULL // terminator!
};

// v poli FunctionNames vyhleda funkci a vrati jeji 'index' a TRUE
// pokud ji nenajde, vrati FALSE
BOOL FindFunction(const char* name, int* index)
{
    for (int i = 0; FunctionNames[i] != NULL; i++)
    {
        if (strcmp(FunctionNames[i], name) == 0)
        {
            *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

// zpracuje jednotliva pravidla "xxx = yyyyy(....)" nebo "yyyyy(...)"
BOOL CompileRule(const char*& rh, const char* rhEnd, int* line, int* errorResID)
{
    const char* beg = rh;
    if (!SkipIdentifier(rh, rhEnd, errorResID, IDS_ERR_MISSINGFUNCORVAR))
        return FALSE;

    char ident1[100];
    char ident2[100];

    lstrcpyn(ident1, beg, min(100, rh - beg + 1));
    if (!SkipWSAndEOLAndCmnt(rh, rhEnd, line) || (*rh != '=' && *rh != '('))
    {
        *errorResID = IDS_ERR_MISSINGFUNCPARS;
        return FALSE;
    }

    // muze nasledovat '=' (pak byl ident1 nazev promenne)
    //            nebo '(' (pak slo o nazev funkce)
    const char* result;
    const char* function;
    if (*rh == '=')
    {
        rh++;

        if (!SkipWSAndEOLAndCmnt(rh, rhEnd, line))
        {
            *errorResID = IDS_ERR_MISSINGFUNC;
            return FALSE;
        }

        beg = rh;
        if (!SkipIdentifier(rh, rhEnd, errorResID, IDS_ERR_MISSINGFUNCORVAR))
            return FALSE;
        lstrcpyn(ident2, beg, min(100, rh - beg + 1));

        result = ident1;
        function = ident2;
    }
    else
    {
        result = NULL;
        function = ident1;
    }

    // funkce musi byt ze seznamu znamych funkci
    int fncIndex;
    if (!FindFunction(function, &fncIndex))
    {
        *errorResID = IDS_ERR_UNKNOWNFUNC;
        return FALSE;
    }

    // ted uz musi nasledovat zavorka '('
    if (!SkipWSAndEOLAndCmnt(rh, rhEnd, line) || *rh != '(')
    {
        *errorResID = IDS_ERR_MISSINGFUNCPARS;
        return FALSE;
    }
    rh++;

    // 'lastItem' funguje k rizeni stavoveho automatu
    BOOL lastItem = 0; // 0:oteviraci_zavorka 1:konstanta 2:carka 3:zaviraci_zavorka

    // nasleduji parametry funkce oddelene znakem ',' a ukoncene zavorkou ')'
    while (*rh != ')')
    {
        // preskocime smeti
        if (!SkipWSAndEOLAndCmnt(rh, rhEnd, line))
        {
            *errorResID = IDS_ERR_MISSINGBLOCKEND;
            return FALSE;
        }
        // zaviraci zavorku preskocime, ukonci se while cyklus
        if (*rh != ')')
        {
            if (*rh == ',')
            {
                if (lastItem == 0 || lastItem == 2) // carka nesmi nasledovat po oteviraci zavorce nebo jine carce
                {
                    *errorResID = IDS_ERR_MISSINGFUNCPAR;
                    return FALSE;
                }
                rh++;
                lastItem = 2;
            }
            else
            {
                // neni to carka, mel by to byt identifikator
                char par1[100];
                beg = rh;
                if (!SkipIdentifier(rh, rhEnd, errorResID, IDS_ERR_MISSINGFUNCPAR))
                    return FALSE;
                lstrcpyn(par1, beg, min(100, rh - beg + 1));
                lastItem = 1;
            }
        }
        else
            lastItem = 3;
    }
    rh++;
    // jeste overime, ze nedoslo k predcasnemu zavreni seznamu
    if (lastItem == 2)
    {
        *errorResID = IDS_ERR_MISSINGFUNCPAR;
        return FALSE;
    }

    return TRUE;
}

// zpracuje blok { .... }
BOOL CompileRulesBlock(const char*& rh, const char* rhEnd, int* line, int* errorResID)
{
    // seznam musi byt uveden zavorkou '{'
    if (!SkipWSAndEOLAndCmnt(rh, rhEnd, line) || *rh != '{')
    {
        *errorResID = IDS_ERR_MISSINGBLOCKBEGIN;
        return FALSE;
    }
    rh++;

    // nasleduji jednotliva pravidla pripadne zaviraci zavorka '}'
    while (*rh != '}')
    {
        if (!SkipWSAndEOLAndCmnt(rh, rhEnd, line))
        {
            *errorResID = IDS_ERR_MISSINGBLOCKEND;
            return FALSE;
        }
        if (*rh != '}') // zaviraci zavorka
        {
            if (!CompileRule(rh, rhEnd, line, errorResID))
                return FALSE;
        }
    }
    rh++;

    // za blokem { } uz by nemelo nic nasledovat
    if (SkipWSAndEOLAndCmnt(rh, rhEnd, line))
    {
        *errorResID = IDS_ERR_UNEXPECTEDSYMBOL;
        return FALSE;
    }

    return TRUE;
}

BOOL CompileLayout(const char*& rh, const char* rhEnd, int* line, int* errorResID)
{
    // sezobneme Layout ( IDD_xxx )
    if (SkipWS(rh, rhEnd))
    {
        if (*rh == '(')
        {
            rh++;
            if (SkipWS(rh, rhEnd))
            {
                const char* beg = rh;
                if (SkipIdentifier(rh, rhEnd, errorResID, IDS_ERR_MISSINGFUNCPARS))
                {
                    char dlgName[100];
                    lstrcpyn(dlgName, beg, min(100, rh - beg + 1));

                    // overim, ze je identifikator definovany
                    if (!DataRH.GetIDForIdentifier(dlgName, NULL))
                    {
                        *errorResID = IDS_ERR_UNKNOWNIDENTIFIER;
                        return FALSE;
                    }
                    if (SkipWS(rh, rhEnd))
                    {
                        if (*rh == ')')
                        {
                            rh++;

                            // nyni musi nasledovat seznam pravidel ohranicenych v { } zavorkach
                            return CompileRulesBlock(rh, rhEnd, line, errorResID);
                        }
                    }
                }
            }
        }
    }
    *errorResID = IDS_ERR_MISSINGFUNCPARS;
    return FALSE;
}

BOOL CompileSlash(const char*& rh, const char* rhEnd, int* line, int* errorResID)
{
    rh++; // preskocime prvni '/'
    if (rh < rhEnd && *rh == '/')
    {
        rh++; // preskocime druhy '/'
        if (rh < rhEnd && *rh == '*')
        {
            rh++;                         // preskocime '*'
            if (rh < rhEnd && *rh == ' ') // Petr: .rh soubory obsahuji radky "//*******", aby se nepletly s layoutovacimi pravidly, musi pravidla zacinat "//* "
            {
                rh++; // preskocime ' '
                if (SkipWS(rh, rhEnd))
                {
                    if (*rh != '#')
                    {
                        const char* beg = rh;
                        if (SkipIdentifier(rh, rhEnd, errorResID, IDS_ERR_LAYOUTEXPECTED))
                        {
                            char functionName[100];
                            lstrcpyn(functionName, beg, min(100, rh - beg + 1));
                            if (strcmp(functionName, "Layout") == 0)
                                return CompileLayout(rh, rhEnd, line, errorResID);
                            else
                                *errorResID = IDS_ERR_LAYOUTEXPECTED;
                        }
                        return FALSE;
                    }
                }
            }
        }
        SkipRestOfLine(rh, rhEnd);
        return TRUE;
    }

    return FALSE;
}

BOOL CDataRH::GetOriginalFile(int line, char* originalFile, int buffSize, int* originalFileLine)
{
    CDataRHItem* lastMark = NULL;
    for (int i = 0; i < FileMarks.Count; i++)
    {
        if (FileMarks[i].Row > line)
            break;
        lastMark = &FileMarks[i];
    }
    if (lastMark == NULL)
    {
        strcpy_s(originalFile, buffSize, "UNKNOWN");
        *originalFileLine = 0;
    }
    else
    {
        lstrcpyn(originalFile, lastMark->Name, buffSize);
        originalFile[buffSize - 1] = 0;
        *originalFileLine = line - lastMark->Row;
    }
    return TRUE;
}

BOOL CDataRH::Load(const char* fileName)
{
    Clean();

    {
        wchar_t buff[2 * MAX_PATH];
        swprintf_s(buff, L"Reading SYMBOLS file: %hs", fileName);
        OutWindow.AddLine(buff, mteInfo);
    }

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

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0xFFFFFFFF)
    {
        char buf[MAX_PATH + 100];
        sprintf_s(buf, "Error reading file %s.", fileName);
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    Data = (char*)malloc(size + 1);
    if (Data == NULL)
    {
        TRACE_E("Nedostatek pameti");
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, Data, size, &read, NULL) || read != size)
    {
        char buf[MAX_PATH + 100];
        DWORD err = GetLastError();
        sprintf_s(buf, "Error reading file %s.\n%s", fileName, GetErrorText(err));
        MessageBox(GetMsgParent(), buf, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        free(Data);
        Data = NULL;
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }
    Data[size] = 0; // vlozime terminator
    DataSize = size;

    CleanIncompleteItems();

    // vyhledame jednotlive #define a ulozime konstanty do pole Items
    const char* lineStart = Data;
    const char* lineEnd = Data;
    int line = 1;
    while (lineEnd < Data + size)
    {
        while (*lineEnd != '\r' && *lineEnd != '\n' && lineEnd < Data + size)
            lineEnd++;

        if (!ProcessLine(lineStart, lineEnd, line))
        {
            char errbuf[MAX_PATH + 100];
            sprintf_s(errbuf, "Error reading Resource Symbols from file\n"
                              "%s\n"
                              "\n"
                              "Syntax error on line %d\n"
                              "\n"
                              "Open file in MSVC?",
                      fileName, line);
            int ret = MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OKCANCEL | MB_ICONEXCLAMATION);
            if (ret == IDOK)
                GotoEditor(fileName, line);

            free(Data);
            Data = NULL;
            HANDLES(CloseHandle(hFile));
            return FALSE;
        }
        if (lineEnd < Data + size)
        {
            if (*lineEnd == '\r' && lineEnd + 1 < Data + size && *(lineEnd + 1) == '\n')
                lineEnd++;
            lineEnd++;
            line++;
            lineStart = lineEnd;
        }
    }

    if (IncompleteItems.Count > 0)
    {
        BOOL cycle = TRUE;
        while (cycle)
        {
            cycle = FALSE;
            for (int i = 0; i < IncompleteItems.Count; i++)
            {
                CDataRHIncompleteItem* incomplItem = &IncompleteItems[i];

                WORD foundID;
                if (GetIDForIdentifier(incomplItem->SymbVal, &foundID))
                {
                    cycle = TRUE; // aspon jedna zmena, dalsi cyklus ma smysl (pokud bude vubec potreba)

                    CDataRHItem item; // presuneme jmeno, radek a hodnotu
                    item.Name = incomplItem->Name;
                    item.Row = incomplItem->Row;
                    item.ID = foundID + incomplItem->AddConst;

                    free(incomplItem->SymbVal);  // uvolnime jiz nepotrebne jmeno symbolicke hodnoty
                    IncompleteItems.Delete(i--); // zahodime zpracovanou nekompletni hodnotu z pole

                    Items.Add(item);
                    if (!Items.IsGood())
                    {
                        TRACE_E("Low memory");
                        free(item.Name);
                        free(Data);
                        Data = NULL;
                        HANDLES(CloseHandle(hFile));
                    }
                }
            }
        }
        // nezname hodnoty vypiseme do Output okna
        for (int i = 0; i < IncompleteItems.Count; i++)
        {
            CDataRHIncompleteItem* incomplItem = &IncompleteItems[i];

            wchar_t buff[2 * MAX_PATH];
            swprintf_s(buff, L"SYMBOLS file: invalid ID (%hs) on row %d: %hs is not defined",
                       incomplItem->Name, incomplItem->Row, incomplItem->SymbVal);
            OutWindow.AddLine(buff, mteError);
        }
        CleanIncompleteItems();
    }

    // pole seradime
    if (Items.Count > 1)
        SortItems(0, Items.Count - 1);
    // a vyhazime z nej redundance
    FindEqualItems();

    // projedeme soubor jeste jednou a hledame //* bloky obsahujici layoutovaci pravidla
    int errorResID;
    BOOL error = FALSE;
    const char* rh = Data;
    const char* rhBeg = rh;
    const char* rhEnd = rh + size;
    line = 1;
    while (rh < rhEnd)
    {
        if (SkipWSAndEOLs(rh, rhEnd, &line))
        {
            if (*rh == '/')
            {
                if (!CompileSlash(rh, rhEnd, &line, &errorResID))
                {
                    error = TRUE;
                    break;
                }
            }
            else
                SkipRestOfLine(rh, rhEnd);
        }
    }
    if (error)
    {
        char errbuf[MAX_PATH + 100];
        char originalFile[MAX_PATH];
        int originalFileLine;
        GetOriginalFile(line, originalFile, MAX_PATH, &originalFileLine);

        sprintf_s(errbuf, "Error reading Resource Symbols. Original file name:\n"
                          "%s\n"
                          "\n"
                          "Syntax error on line %d: %s\n"
                          "\n"
                          "Open file in MSVC?",
                  originalFile, originalFileLine, LoadStr(errorResID));
        int ret = MessageBox(GetMsgParent(), errbuf, ERROR_TITLE, MB_OKCANCEL | MB_ICONEXCLAMATION);
        if (ret == IDOK)
            GotoEditor(originalFile, originalFileLine);

        free(Data);
        Data = NULL;
        HANDLES(CloseHandle(hFile));
        return FALSE;
    }

    HANDLES(CloseHandle(hFile));
    return TRUE;
}

void CDataRH::FillListBox()
{
    char buff[2000];

    HWND hListBox = RHWindow.ListBox.HWindow;
    if (hListBox == NULL)
        return;

    ListView_DeleteAllItems(hListBox);
    if (Data == NULL)
        return;

    SendMessage(hListBox, WM_SETREDRAW, FALSE, 0); // zakazeme zbytecne kresleni behem pridavani polozek

    const char* lineStart = Data;
    const char* lineEnd = Data;
    while (lineEnd < Data + DataSize)
    {
        while (*lineEnd != '\r' && *lineEnd != '\n' && lineEnd < Data + DataSize)
            lineEnd++;

        lstrcpyn(buff, lineStart, min(lineEnd - lineStart + 1, 2000));

        int count = ListView_GetItemCount(hListBox);
        LVITEM lvi;
        lvi.mask = LVIF_TEXT | LVIF_STATE;
        lvi.iItem = count;
        lvi.iSubItem = 0;
        lvi.state = 0;
        lvi.pszText = buff;
        SendMessage(hListBox, LVM_INSERTITEM, 0, (LPARAM)&lvi);

        if (lineEnd < Data + DataSize)
        {
            if (*lineEnd == '\r' && lineEnd + 1 < Data + DataSize && *(lineEnd + 1) == '\n')
                lineEnd++;
            lineEnd++;
            lineStart = lineEnd;
        }
    }
    RHWindow.GetVisibleItems();
    SendMessage(hListBox, WM_SETREDRAW, TRUE, 0); // povolime prekreslovani
}

int CDataRH::RowToIndex(int row)
{
    for (int i = 0; i < Items.Count; i++)
    {
        if (Items[i].Row == row)
            return i;
    }
    return -1;
}
