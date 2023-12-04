// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "data.h"
#include "parser.h"
#include "renderer.h"
#include "dialogs.h"
#include "dbviewer.h"
#include "dbviewer.rh"
#include "dbviewer.rh2"
#include "lang\lang.rh"

//****************************************************************************
//
// CDatabase
//

CDatabase::CDatabase()
    : Columns(50, 50), VisibleColumns(50, 50)
{
    FileName = NULL;
    Parser = NULL;
    Renderer = NULL;
    IsUnicode = IsUTF8 = FALSE;
}

CDatabase::~CDatabase()
{
    Close();
}

BOOL CDatabase::Open(const char* fileName)
{
    if (IsOpened())
    {
        Close();
    }

    BOOL ret = TRUE;

    // zkusim soubor otevrit jako DBF
    Parser = new CParserInterfaceDBF();
    if (Parser != NULL)
    {
        CParserStatusEnum status;
        status = Parser->OpenFile(fileName);
        if (status != psOK)
        {
            // nevyslo to, zkusim ho otevrit jako CSV
            delete Parser;
            CParserInterfaceCSV* pCSVParser;
            Parser = pCSVParser = new CParserInterfaceCSV(&Renderer->Viewer->CfgCSV);
            if (Parser == NULL)
                goto OUT_OF_MEMORY;
            status = Parser->OpenFile(fileName);
            if (status == psOK)
            {
                IsUnicode = pCSVParser->GetIsUnicode();
                IsUTF8 = pCSVParser->GetIsUTF8();
            }
        }
        if (status == psOK)
        {
            Columns.DestroyMembers();

            CDatabaseColumn column;
            CFieldInfo fieldInfo;

            char type[100];
            fieldInfo.Type = type;

            HDC hDC = GetDC(NULL);
            HFONT oldFont = (HFONT)SelectObject(hDC, Renderer->HFont);
            SIZE sz;

            int skippedColumns = 0;
            DWORD i;
            for (i = 0; i < Parser->GetFieldCount(); i++)
            {
                // vytahneme potrebnou velikost bufferu pro nazev sloupce
                fieldInfo.Name = NULL;
                if (!Parser->GetFieldInfo(i, &fieldInfo))
                    break;

                fieldInfo.Name = (char*)malloc(fieldInfo.NameMax);
                if (fieldInfo.Name == NULL)
                {
                    Parser->ShowParserError(Renderer->HWindow, psOOM);
                    break;
                }
                column.Type = SalGeneral->DupStr(type);
                if (column.Type == NULL)
                {
                    Parser->ShowParserError(Renderer->HWindow, psOOM);
                    break;
                }

                // vytahneme nazev sloupce
                Parser->GetFieldInfo(i, &fieldInfo);

                column.Name = fieldInfo.Name;
                column.LeftAlign = fieldInfo.LeftAlign;
                column.Length = fieldInfo.TextMax;
                column.FieldLen = fieldInfo.FieldLen;
                column.Decimals = fieldInfo.Decimals;

                // sirku sloupce napocitame z poctu znaku v sloupci
                // pokud je sirsi hlavicka sloupce, pouzijeme tu
                if (!IsUnicode)
                    GetTextExtentPoint32A(hDC, column.Name, (int)strlen(column.Name), &sz);
                else
                    GetTextExtentPoint32W(hDC, (LPWSTR)column.Name, (int)wcslen((LPWSTR)column.Name), &sz);
                column.Width = sz.cx;

                // pokud parser dokaze odhadnout maximalni pocet znaku, odhadneme sirku sloupce
                if (fieldInfo.TextMax > 0)
                {
                    int width = Renderer->CharAvgWidth * fieldInfo.TextMax;
                    if (width > column.Width)
                        column.Width = width;
                }
                column.Width += 2 * Renderer->LeftTextMargin + 1;

                column.OriginalIndex = i;
                column.Visible = TRUE;

                Columns.Add(column);
                if (!Columns.IsGood())
                {
                    Columns.ResetState();
                    free(column.Name);
                    SelectObject(hDC, oldFont);
                    ReleaseDC(NULL, hDC);
                    Close();
                    ret = FALSE;
                    Parser->ShowParserError(Renderer->HWindow, psOOM);
                    break;
                }
                ColumnsDirty = TRUE;
            }

            SelectObject(hDC, oldFont);
            ReleaseDC(NULL, hDC);

            FileName = SalGeneral->DupStr(fileName);
            if (FileName == NULL)
            {
                Close();
                ret = FALSE;
                Parser->ShowParserError(Renderer->HWindow, psOOM);
            }

            if (Columns.Count == 0)
            {
                Close();
                ret = FALSE;
                SalGeneral->SalMessageBox(Renderer->HWindow, LoadStr(IDS_NOVALIDCOLUMN),
                                          LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONEXCLAMATION);
            }
        }
        else
        {
            Close();
            ret = FALSE;
            Parser->ShowParserError(Renderer->HWindow, status);
        }
    }
    else
    {
    OUT_OF_MEMORY:
        ret = FALSE;
        if (Parser)
            Parser->ShowParserError(Renderer->HWindow, psOOM);
        else
            SalGeneral->SalMessageBox(Renderer->HWindow, LoadStr(IDS_DBFE_OOM),
                                      LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONEXCLAMATION);
    }

    return ret;
}

void CDatabase::Close()
{
    if (FileName != NULL)
    {
        SalGeneral->Free(FileName);
        FileName = NULL;
    }
    if (Parser != NULL)
    {
        Parser->CloseFile();
        delete Parser;
        Parser = NULL;
    }
    int i;
    for (i = 0; i < Columns.Count; i++)
    {
        if (Columns[i].Name != NULL)
            free(Columns[i].Name);
        if (Columns[i].Type != NULL)
            SalGeneral->Free(Columns[i].Type);
    }
    Columns.DestroyMembers();
    ColumnsDirty = FALSE;
    VisibleColumns.DestroyMembers();
    VisibleColumnCount = 0;
    VisibleColumnsWidth = 0;
}

const char*
CDatabase::GetParserName()
{
    return Parser == NULL ? "" : Parser->GetParserName();
}

BOOL CDatabase::GetFileInfo(HWND hEdit)
{
    if (Parser == NULL)
    {
        TRACE_E("Chybne volani CDatabase::GetFileInfo: Parser == NULL");
        return 0;
    }
    return Parser->GetFileInfo(hEdit);
}

int CDatabase::GetRowCount()
{
    if (Parser == NULL)
    {
        TRACE_E("Chybne volani CDatabase::GetRowCount: Parser == NULL");
        return 0;
    }
    return Parser->GetRecordCount();
}

void CDatabase::UpdateColumnsInfo()
{
    if (ColumnsDirty)
    {
        VisibleColumnsWidth = 0;
        VisibleColumnCount = 0;
        VisibleColumns.DestroyMembers();
        int i;
        for (i = 0; i < Columns.Count; i++)
        {
            CDatabaseColumn* column = &Columns[i];
            if (column->Visible)
            {
                VisibleColumns.Add(i);
                VisibleColumnsWidth += column->Width;
                VisibleColumnCount++;
            }
        }
        ColumnsDirty = FALSE;
    }
}

const CDatabaseColumn*
CDatabase::GetVisibleColumn(int index)
{
    if (ColumnsDirty)
        UpdateColumnsInfo();
    return &Columns[VisibleColumns[index]];
}

int CDatabase::GetVisibleColumnsWidth()
{
    if (ColumnsDirty)
        UpdateColumnsInfo();
    return VisibleColumnsWidth;
}

int CDatabase::GetVisibleColumnCount()
{
    if (ColumnsDirty)
        UpdateColumnsInfo();
    return VisibleColumnCount;
}

int CDatabase::GetVisibleColumnX(int colIndex)
{
    int x = 0;
    int j = 0;
    int i;
    for (i = 0; i < Columns.Count; i++)
    {
        CDatabaseColumn* column = &Columns[i];
        if (column->Visible)
        {
            if (colIndex == j)
                return x;
            x += column->Width;
            j++;
        }
    }
    return -1;
}

BOOL CDatabase::FetchRecord(HWND hParent, DWORD rowIndex)
{
    if (Parser == NULL)
    {
        TRACE_E("Chybne volani CDatabase::FetchRecord");
        return FALSE;
    }

    if (rowIndex >= Parser->GetRecordCount())
        return FALSE;

    CParserStatusEnum status = Parser->FetchRecord(rowIndex);
    if (status != psOK)
    {
        Parser->ShowParserError(hParent, status);
        return FALSE;
    }

    return TRUE;
}

const char*
CDatabase::GetCellText(const CDatabaseColumn* column, size_t* textLen)
{
    if (Parser == NULL)
    {
        TRACE_E("Chybne volani CDatabase::GetCellText");
        return FALSE;
    }
    return Parser->GetCellText(column->OriginalIndex, textLen);
}

const wchar_t*
CDatabase::GetCellTextW(const CDatabaseColumn* column, size_t* textLen)
{
    if (Parser == NULL)
    {
        TRACE_E("Chybne volani CDatabase::GetCellTextW");
        return FALSE;
    }
    return Parser->GetCellTextW(column->OriginalIndex, textLen);
}

BOOL CDatabase::IsRecordDeleted()
{
    if (Parser == NULL)
    {
        TRACE_E("Chybne volani CDatabase::IsRecordDeleted()");
        return FALSE;
    }
    return Parser->IsRecordDeleted();
}
