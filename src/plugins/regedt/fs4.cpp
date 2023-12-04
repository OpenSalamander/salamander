// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// globalni promenne, do ktery si ulozim ukazatele na globalni promenne v Salamanderovi
// pro archiv i pro FS - promenne se sdileji
const CFileData** TransferFileData = NULL;
int* TransferIsDir = NULL;
char* TransferBuffer = NULL;
int* TransferLen = NULL;
DWORD* TransferRowData = NULL;
CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
DWORD* TransferActCustomData = NULL;

int TypeDateColFW = 0; // LO/HI-WORD: levy/pravy panel: sloupec Type/Date: FixedWidth
int TypeDateColW = 0;  // LO/HI-WORD: levy/pravy panel: sloupec Type/Date: Width
int DataTimeColFW = 0; // LO/HI-WORD: levy/pravy panel: sloupec Data/Time: FixedWidth
int DataTimeColW = 0;  // LO/HI-WORD: levy/pravy panel: sloupec Data/Time: Width
int SizeColFW = 0;     // LO/HI-WORD: levy/pravy panel: sloupec Size: FixedWidth
int SizeColW = 0;      // LO/HI-WORD: levy/pravy panel: sloupec Size: Width

// ****************************************************************************
//
// CPluginDataInterface
//
//

void WINAPI GetTypeText()
{
    CALL_STACK_MESSAGE_NONE
    if (*TransferIsDir > 0)
    {
        // je to klic, vypiseme datum
        static SYSTEMTIME st;
        static int len;
        if ((*TransferFileData)->LastWrite.dwHighDateTime != 0 &&
            (*TransferFileData)->LastWrite.dwLowDateTime != 0)
        {
            FileTimeToSystemTime(&(*TransferFileData)->LastWrite, &st);
            len = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, TransferBuffer, 50) - 1;
            if (len < 0)
                len = sprintf(TransferBuffer, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
            *TransferLen = len;
        }
        else
        {
            *TransferLen = 0;
        }
    }
    else
    {
        // jeto hodnota vypiseme typ
        switch (((CPluginData*)(*TransferFileData)->PluginData)->Type)
        {
        case REG_BINARY:
            memcpy(TransferBuffer, Str_REG_BINARY, Len_REG_BINARY);
            *TransferLen = Len_REG_BINARY;
            break;
        case REG_DWORD:
            memcpy(TransferBuffer, Str_REG_DWORD, Len_REG_DWORD);
            *TransferLen = Len_REG_DWORD;
            break;
        case REG_DWORD_BIG_ENDIAN:
            memcpy(TransferBuffer, Str_REG_DWORD_BIG_ENDIAN, Len_REG_DWORD_BIG_ENDIAN);
            *TransferLen = Len_REG_DWORD_BIG_ENDIAN;
            break;
        case REG_EXPAND_SZ:
            memcpy(TransferBuffer, Str_REG_EXPAND_SZ, Len_REG_EXPAND_SZ);
            *TransferLen = Len_REG_EXPAND_SZ;
            break;
        case REG_LINK:
            memcpy(TransferBuffer, Str_REG_LINK, Len_REG_LINK);
            *TransferLen = Len_REG_LINK;
            break;
        case REG_MULTI_SZ:
            memcpy(TransferBuffer, Str_REG_MULTI_SZ, Len_REG_MULTI_SZ);
            *TransferLen = Len_REG_MULTI_SZ;
            break;
        case REG_NONE:
            memcpy(TransferBuffer, Str_REG_NONE, Len_REG_NONE);
            *TransferLen = Len_REG_NONE;
            break;
        case REG_QWORD:
            memcpy(TransferBuffer, Str_REG_QWORD, Len_REG_QWORD);
            *TransferLen = Len_REG_QWORD;
            break;
        case REG_RESOURCE_LIST:
            memcpy(TransferBuffer, Str_REG_RESOURCE_LIST, Len_REG_RESOURCE_LIST);
            *TransferLen = Len_REG_RESOURCE_LIST;
            break;
        case REG_SZ:
            memcpy(TransferBuffer, Str_REG_SZ, Len_REG_SZ);
            *TransferLen = Len_REG_SZ;
            break;

        case REG_FULL_RESOURCE_DESCRIPTOR:
            memcpy(TransferBuffer, Str_REG_FULL_RESOURCE_DESCRIPTOR, Len_REG_FULL_RESOURCE_DESCRIPTOR);
            *TransferLen = Len_REG_FULL_RESOURCE_DESCRIPTOR;
            break;

        case REG_RESOURCE_REQUIREMENTS_LIST:
            memcpy(TransferBuffer, Str_REG_RESOURCE_REQUIREMENTS_LIST, Len_REG_RESOURCE_REQUIREMENTS_LIST);
            *TransferLen = Len_REG_RESOURCE_REQUIREMENTS_LIST;
            break;

        default:
            TRACE_E("unknown value type");
            TransferBuffer[0] = '?';
            *TransferLen = 1;
        }
    }
}

void WINAPI GetDataText()
{
    CALL_STACK_MESSAGE_NONE
    if (*TransferIsDir > 0)
    {
        // je to klic, vypiseme cas
        static SYSTEMTIME st;
        static int len;
        if ((*TransferFileData)->LastWrite.dwHighDateTime != 0 &&
            (*TransferFileData)->LastWrite.dwLowDateTime != 0)
        {
            FileTimeToSystemTime(&(*TransferFileData)->LastWrite, &st);
            len = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, TransferBuffer, 50) - 1;
            if (len < 0)
                len = sprintf(TransferBuffer, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            *TransferLen = len;
        }
        else
        {
            *TransferLen = 0;
        }
    }
    else
    {
        // je to hodnota vypiseme ji
        static CPluginData* pluginData;
        pluginData = ((CPluginData*)(*TransferFileData)->PluginData);

        if ((*TransferFileData)->Size > CQuadWord(0, 0))
        {
            switch (pluginData->Type)
            {
            case REG_MULTI_SZ:
            case REG_EXPAND_SZ:
            case REG_SZ:
                if (pluginData->Data && pluginData->DataSize)
                {
                    memcpy(TransferBuffer, (void*)pluginData->Data, pluginData->DataSize);
                    *TransferLen = pluginData->DataSize - 1;
                }
                else
                    *TransferLen = 0;
                break;

            case REG_DWORD_BIG_ENDIAN:
            case REG_DWORD:
                if ((*TransferFileData)->Size == CQuadWord(4, 0))
                    *TransferLen = sprintf(TransferBuffer, "0x%08x (%u)", (DWORD)pluginData->Data, (DWORD)pluginData->Data); // FIXME_X64 - overity pretypovani na (DWORD)
                else
                    *TransferLen = 0;
                break;

            case REG_QWORD:
                if ((*TransferFileData)->Size == CQuadWord(8, 0))
                {
                    QWORD q = *(LPQWORD)pluginData->Data;
                    *TransferLen = sprintf(TransferBuffer, "0x%016I64x (%I64u)", q, q);
                }
                else
                    *TransferLen = 0;
                break;

            default:
                if (pluginData->Data)
                    memcpy(TransferBuffer, (void*)pluginData->Data, pluginData->DataSize);
                *TransferLen = pluginData->DataSize;
            }
        }
        else
        {
            *TransferLen = NULL;
        }
    }
}

void WINAPI GetSizeText()
{
    CALL_STACK_MESSAGE_NONE
    if (*TransferIsDir > 0)
    {
        // je to klic, vypiseme "KEY"
        memcpy(TransferBuffer, KeyText, KeyTextLen);
        *TransferLen = KeyTextLen;
    }
    else
    {
        // je to hodnota vypiseme jeji velikost
        SG->NumberToStr(TransferBuffer, (*TransferFileData)->Size);
        *TransferLen = (int)strlen(TransferBuffer);
    }
}

int WINAPI
PluginSimpleIconCallback()
{
    CALL_STACK_MESSAGE_NONE
    if (*TransferIsDir > 0)
    {
        return 0;
    }
    else
    {
        switch (((CPluginData*&)(*TransferFileData)->PluginData)->Type)
        {
        case REG_EXPAND_SZ:
        case REG_MULTI_SZ:
        case REG_SZ:
            return 1;

        case REG_BINARY:
        case REG_DWORD:
        case REG_DWORD_BIG_ENDIAN:
        case REG_QWORD:
        case REG_LINK:
        case REG_RESOURCE_LIST:
        case REG_FULL_RESOURCE_DESCRIPTOR:
        case REG_RESOURCE_REQUIREMENTS_LIST:
            return 2;

        case REG_NONE:
        default:
            return 3;
        }
    }
}

HIMAGELIST
CPluginDataInterface::GetSimplePluginIcons(int iconSize)
{
    CALL_STACK_MESSAGE1("CPluginDataInterface::GetSimplePluginIcons()");
    return ImageList;
}

void CPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                     const CFileData* upperDir)
{
    CALL_STACK_MESSAGE2("CPluginDataInterface::SetupView(, , %s,)", archivePath);
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen, TransferRowData,
                               TransferPluginDataIface, TransferActCustomData);

    view->SetPluginSimpleIconCallback(PluginSimpleIconCallback);

    if (view->GetViewMode() == VIEW_MODE_DETAILED) // upravime sloupce
    {
        view->SetViewMode(VIEW_MODE_DETAILED, VALID_DATA_NONE); // odstranime vsechny sloupce

        int i = 1;
        CColumn column;

        // pridame sloupec Type/Date
        strcpy(column.Name, LoadStr(IDS_TYPE));
        strcpy(column.Description, LoadStr(IDS_TYPE));
        column.GetText = GetTypeText;
        column.SupportSorting = 0;
        column.LeftAlignment = 1;
        column.ID = COLUMN_ID_CUSTOM;
        column.Width = leftPanel ? LOWORD(TypeDateColW) : HIWORD(TypeDateColW);
        column.FixedWidth = leftPanel ? LOWORD(TypeDateColFW) : HIWORD(TypeDateColFW);
        column.CustomData = 1;
        view->InsertColumn(i++, &column);

        // pridame sloupec Data/Time
        strcpy(column.Name, LoadStr(IDS_DATA));
        strcpy(column.Description, LoadStr(IDS_DATA));
        column.GetText = GetDataText;
        column.SupportSorting = 0;
        column.LeftAlignment = 1;
        column.ID = COLUMN_ID_CUSTOM;
        column.Width = leftPanel ? LOWORD(DataTimeColW) : HIWORD(DataTimeColW);
        column.FixedWidth = leftPanel ? LOWORD(DataTimeColFW) : HIWORD(DataTimeColFW);
        column.CustomData = 2;
        view->InsertColumn(i++, &column);

        // pridame sloupec Size
        strcpy(column.Name, LoadStr(IDS_SIZE));
        strcpy(column.Description, LoadStr(IDS_SIZE));
        column.GetText = GetSizeText;
        column.SupportSorting = 1;
        column.LeftAlignment = 0;
        column.ID = COLUMN_ID_CUSTOM;
        column.Width = leftPanel ? LOWORD(SizeColW) : HIWORD(SizeColW);
        column.FixedWidth = leftPanel ? LOWORD(SizeColFW) : HIWORD(SizeColFW);
        column.CustomData = 3;
        view->InsertColumn(i++, &column);
    }
}

void CPluginDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    if (leftPanel)
    {
        switch (column->CustomData)
        {
        case 1:
            TypeDateColFW = MAKELONG(newFixedWidth, HIWORD(TypeDateColFW));
            break;
        case 2:
            DataTimeColFW = MAKELONG(newFixedWidth, HIWORD(DataTimeColFW));
            break;
        case 3:
            SizeColFW = MAKELONG(newFixedWidth, HIWORD(SizeColFW));
            break;
        }
    }
    else
    {
        switch (column->CustomData)
        {
        case 1:
            TypeDateColFW = MAKELONG(LOWORD(TypeDateColFW), newFixedWidth);
            break;
        case 2:
            DataTimeColFW = MAKELONG(LOWORD(DataTimeColFW), newFixedWidth);
            break;
        case 3:
            SizeColFW = MAKELONG(LOWORD(SizeColFW), newFixedWidth);
            break;
        }
    }
    if (newFixedWidth)
        ColumnWidthWasChanged(leftPanel, column, column->Width);
}

void CPluginDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    if (leftPanel)
    {
        switch (column->CustomData)
        {
        case 1:
            TypeDateColW = MAKELONG(newWidth, HIWORD(TypeDateColW));
            break;
        case 2:
            DataTimeColW = MAKELONG(newWidth, HIWORD(DataTimeColW));
            break;
        case 3:
            SizeColW = MAKELONG(newWidth, HIWORD(SizeColW));
            break;
        }
    }
    else
    {
        switch (column->CustomData)
        {
        case 1:
            TypeDateColW = MAKELONG(LOWORD(TypeDateColW), newWidth);
            break;
        case 2:
            DataTimeColW = MAKELONG(LOWORD(DataTimeColW), newWidth);
            break;
        case 3:
            SizeColW = MAKELONG(LOWORD(SizeColW), newWidth);
            break;
        }
    }
}

const char* WINAPI FSInfoLineName(HWND parent, void* param)
{
    CALL_STACK_MESSAGE1("FSInfoLineName(, )");
    return (const char*)param;
}

const char* WINAPI FSInfoLineSize(HWND parent, void* param)
{
    CALL_STACK_MESSAGE1("FSInfoLineSize(, )");
    GetSizeText();
    TransferBuffer[*TransferLen] = 0;
    return TransferBuffer;
}

const char* WINAPI FSInfoLineDate(HWND parent, void* param)
{
    CALL_STACK_MESSAGE1("FSInfoLineDate(, )");
    GetTypeText();
    TransferBuffer[*TransferLen] = 0;
    return TransferBuffer;
}

const char* WINAPI FSInfoLineTime(HWND parent, void* param)
{
    CALL_STACK_MESSAGE1("FSInfoLineTime(, )");
    GetDataText();
    TransferBuffer[*TransferLen] = 0;
    return TransferBuffer;
}

CSalamanderVarStrEntry FSInfoLine[] =
    {
        {"Name", FSInfoLineName},
        {"Size", FSInfoLineSize},
        {"Date", FSInfoLineDate},
        {"Time", FSInfoLineTime},
        {NULL, NULL}};

int ExpandPluralSelection(char* buffer, int bufferSize, int files, int dirs)
{
    CALL_STACK_MESSAGE4("ExpandPluralSelection(, %d, %d, %d)", bufferSize, files,
                        dirs);
    char formatA[200];

    if (files > 0 && dirs > 0)
    {
        CQuadWord parametersArray[] = {CQuadWord(files, 0), CQuadWord(dirs, 0)};
        SG->ExpandPluralString(formatA, 200, LoadStr(IDS_SELECTED3), 2, parametersArray);
        return SalPrintf(buffer, bufferSize, formatA, files, dirs);
    }
    CQuadWord param = CQuadWord(files + dirs, 0);
    SG->ExpandPluralString(formatA, 200, LoadStr(files ? IDS_SELECTED1 : IDS_SELECTED2), 1, &param);
    return SalPrintf(buffer, bufferSize, formatA, files + dirs);
}

BOOL CPluginDataInterface::GetInfoLineContent(
    int panel, const CFileData* file, BOOL isDir, int selectedFiles,
    int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
    char* buffer, DWORD* hotTexts, int& hotTextsCount)
{
    CALL_STACK_MESSAGE8("CPluginDataInterface::GetInfoLineContent(%d, , %d, %d, "
                        "%d, %d, %g, , , %d)",
                        panel, isDir, selectedFiles,
                        selectedDirs, displaySize, selectedSize.GetDouble(),
                        hotTextsCount);
    if (file != NULL)
    {
        char localTransferBuffer[TRANSFER_BUFFER_MAX + 1];
        const CFileData** oldTransferFileData = TransferFileData;
        int* oldTransferIsDir = TransferIsDir;
        char* oldTransferBuffer = TransferBuffer;
        int* oldTransferLen = TransferLen;
        //DWORD            *TransferRowData = NULL;
        //CPluginDataInterfaceAbstract **TransferPluginDataIface = NULL;
        //DWORD            *TransferActCustomData = NULL;
        TransferFileData = &file;
        TransferIsDir = &isDir;
        TransferBuffer = localTransferBuffer;
        int localTransferLen;
        TransferLen = &localTransferLen;

        hotTextsCount = 100;
        if (!SG->ExpandVarString(SG->GetMsgBoxParent(), "$(Name): $(Size), $(Date), $(Time)",
                                 buffer, 1000, FSInfoLine, file->Name, FALSE, hotTexts,
                                 &hotTextsCount))
        {
            strcpy(buffer, "Error!");
            hotTextsCount = 0;
        }

        TransferFileData = oldTransferFileData;
        TransferIsDir = oldTransferIsDir;
        TransferBuffer = oldTransferBuffer;
        TransferLen = oldTransferLen;

        return TRUE;
    }
    else
    {
        if (selectedFiles == 0 && selectedDirs == 0) // Information Line pro prazdny panel
            return FALSE;                            // text at vypise Salamander

        int prefix = 0;
        if (displaySize)
        {
            // pro jednoduchost nepouzivame "plural" stringy (viz SalamanderGeneral->ExpandPluralString())
            char formatA[200];
            SG->ExpandPluralString(formatA, 200, LoadStr(IDS_SELECTEDSIZE), 1, (CQuadWord*)&selectedSize);
            prefix = SalPrintf(buffer, 1000, formatA, selectedSize);

            /*    // ukazka pouziti standardniho retezce
      SalamanderGeneral->ExpandPluralBytesFilesDirs(buffer, 1000, selectedSize, selectedFiles,
                                                    selectedDirs, TRUE);
*/
        }
        ExpandPluralSelection(buffer + prefix, 1000 - prefix, selectedFiles, selectedDirs);
        return SG->LookForSubTexts(buffer, hotTexts, &hotTextsCount);
    }
}
