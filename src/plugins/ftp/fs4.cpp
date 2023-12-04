// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CPluginFSInterface
//

void WINAPI GetTextFromGeneralTextColumn()
{
    char* s = *(char**)(((char*)((*TransferFileData)->PluginData)) + (*TransferActCustomData));
    if (s != NULL)
    {
        *TransferLen = (int)strlen(s);
        memcpy(TransferBuffer, s, *TransferLen);
    }
    else
        *TransferLen = 0;
}

SYSTEMTIME GlobalGeneralDateTimeStruct = {2002, 1, 0, 1, 0, 0, 0, 0}; // pomocna globalka, sloupce se plni jen v hl. threadu, zadne synchr. problemy

void WINAPI GetTextFromGeneralDateColumn()
{
    CFTPDate* date = (CFTPDate*)(((char*)((*TransferFileData)->PluginData)) + (*TransferActCustomData));
    if (date->Day != 0) // nema se zobrazit ""
    {
        GlobalGeneralDateTimeStruct.wDay = date->Day;
        GlobalGeneralDateTimeStruct.wMonth = date->Month;
        GlobalGeneralDateTimeStruct.wYear = date->Year;
        int len;
        if ((len = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &GlobalGeneralDateTimeStruct,
                                 NULL, TransferBuffer, TRANSFER_BUFFER_MAX)) == 0)
        {
            len = 1 + sprintf(TransferBuffer, "%u.%u.%u", GlobalGeneralDateTimeStruct.wDay,
                              GlobalGeneralDateTimeStruct.wMonth, GlobalGeneralDateTimeStruct.wYear);
        }
        *TransferLen = len - 1;
    }
    else
        *TransferLen = 0;
}

void WINAPI GetTextFromGeneralTimeColumn()
{
    CFTPTime* time = (CFTPTime*)(((char*)((*TransferFileData)->PluginData)) + (*TransferActCustomData));
    if (time->Hour != 24) // nema se zobrazit ""
    {
        GlobalGeneralDateTimeStruct.wHour = time->Hour;
        GlobalGeneralDateTimeStruct.wMinute = time->Minute;
        GlobalGeneralDateTimeStruct.wSecond = time->Second;
        GlobalGeneralDateTimeStruct.wMilliseconds = time->Millisecond;
        int len;
        if ((len = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &GlobalGeneralDateTimeStruct,
                                 NULL, TransferBuffer, TRANSFER_BUFFER_MAX)) == 0)
        {
            len = 1 + sprintf(TransferBuffer, "%u:%02u:%02u", GlobalGeneralDateTimeStruct.wHour,
                              GlobalGeneralDateTimeStruct.wMinute, GlobalGeneralDateTimeStruct.wSecond);
        }
        *TransferLen = len - 1;
    }
    else
        *TransferLen = 0;
}

void WINAPI GetTextFromGeneralNumberColumn()
{
    __int64 int64Val = *(__int64*)(((char*)((*TransferFileData)->PluginData)) + (*TransferActCustomData));
    if (int64Val >= 0)
    {
        SalamanderGeneral->NumberToStr(TransferBuffer, CQuadWord().SetUI64((unsigned __int64)int64Val));
        *TransferLen = (int)strlen(TransferBuffer);
    }
    else
    {
        if (int64Val != INT64_EMPTYNUMBER) // nema se zobrazit ""
        {
            TransferBuffer[0] = '-';
            SalamanderGeneral->NumberToStr(TransferBuffer + 1, CQuadWord().SetUI64((unsigned __int64)(-int64Val)));
            *TransferLen = (int)strlen(TransferBuffer);
        }
        else
            *TransferLen = 0;
    }
}

void CFTPListingPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view,
                                               const char* archivePath, const CFileData* upperDir)
{
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen,
                               TransferRowData, TransferPluginDataIface, TransferActCustomData);

    // sloupce upravujeme jen v detailed rezimu
    if (view->GetViewMode() == VIEW_MODE_DETAILED)
    {
        BOOL sepExt = FALSE; // TRUE = samostatny sloupec "Extension", FALSE = pripona soucasti sloupce "Name"
        if (view->GetColumnsCount() > 1 && view->GetColumn(1)->ID == COLUMN_ID_EXTENSION)
            sepExt = TRUE;

        view->SetViewMode(VIEW_MODE_DETAILED, VALID_DATA_NONE); // zrusime ostatni sloupce, nechame jen sloupec Name

        char tmpName[COLUMN_NAME_MAX];
        char tmpDescr[COLUMN_DESCRIPTION_MAX];
        int colCount = 1;
        int i;
        for (i = 0; i < Columns->Count; i++)
        {
            CSrvTypeColumn* col = Columns->At(i);

            if (col->Visible) // sloupec ukazeme jen je-li viditelny
            {
                char bufName[STC_NAME_MAX_SIZE];
                char* colName;
                if (col->NameID != -1)
                {
                    LoadStdColumnStrName(bufName, STC_NAME_MAX_SIZE, col->NameID);
                    colName = bufName;
                }
                else
                    colName = HandleNULLStr(col->NameStr);

                char bufDescr[STC_DESCR_MAX_SIZE];
                char* colDescr;
                if (col->DescrID != -1)
                {
                    LoadStdColumnStrDescr(bufDescr, STC_DESCR_MAX_SIZE, col->DescrID);
                    colDescr = bufDescr;
                }
                else
                    colDescr = HandleNULLStr(col->DescrStr);

                switch (col->Type)
                {
                case stctName: // sloupec Name uz je vlozeny (nelze smazat), jen upravime jmeno+popis
                {
                    lstrcpyn(tmpName, colName, COLUMN_NAME_MAX);
                    SalamanderGeneral->AddStrToStr(tmpName, COLUMN_NAME_MAX, "a"); // dummy nazev "Ext" sloupce (nutne vzdy, protoze sloupec "Name" je v tuto chvili v panelu jediny)
                    lstrcpyn(tmpDescr, colDescr, COLUMN_DESCRIPTION_MAX);
                    SalamanderGeneral->AddStrToStr(tmpDescr, COLUMN_DESCRIPTION_MAX, "a"); // dummy popis "Ext" sloupce (nutne vzdy, protoze sloupec "Name" je v tuto chvili v panelu jediny)
                    view->SetColumnName(i, tmpName, tmpDescr);

                    CColumn* c = (CColumn*)(view->GetColumn(0));

                    break;
                }

                case stctExt:
                {
                    if (sepExt) // user preferuje samostatny sloupec pro priponu
                    {
                        view->InsertStandardColumn(colCount, COLUMN_ID_EXTENSION);
                        view->SetColumnName(colCount, colName, colDescr);
                        colCount++;
                    }
                    else // user preferuje jmeno a priponu v jednom sloupci
                    {
                        strcpy(tmpName, view->GetColumn(0)->Name);
                        SalamanderGeneral->AddStrToStr(tmpName, COLUMN_NAME_MAX, colName);
                        strcpy(tmpDescr, view->GetColumn(0)->Description);
                        SalamanderGeneral->AddStrToStr(tmpDescr, COLUMN_DESCRIPTION_MAX, colDescr);
                        view->SetColumnName(0, tmpName, tmpDescr);
                    }
                    break;
                }

                case stctSize:
                {
                    view->InsertStandardColumn(colCount, COLUMN_ID_SIZE);
                    view->SetColumnName(colCount, colName, colDescr);
                    colCount++;
                    break;
                }

                case stctDate:
                {
                    view->InsertStandardColumn(colCount, COLUMN_ID_DATE);
                    view->SetColumnName(colCount, colName, colDescr);
                    colCount++;
                    break;
                }

                case stctTime:
                {
                    view->InsertStandardColumn(colCount, COLUMN_ID_TIME);
                    view->SetColumnName(colCount, colName, colDescr);
                    colCount++;
                    break;
                }

                case stctType:
                {
                    view->InsertStandardColumn(colCount, COLUMN_ID_TYPE);
                    view->SetColumnName(colCount, colName, colDescr);
                    colCount++;
                    break;
                }

                case stctGeneralText:
                case stctGeneralDate:
                case stctGeneralTime:
                case stctGeneralNumber:
                {
                    CColumn column;
                    lstrcpyn(column.Name, colName, COLUMN_NAME_MAX);
                    lstrcpyn(column.Description, colDescr, COLUMN_DESCRIPTION_MAX);
                    switch (col->Type)
                    {
                    case stctGeneralText:
                        column.GetText = GetTextFromGeneralTextColumn;
                        break;
                    case stctGeneralDate:
                        column.GetText = GetTextFromGeneralDateColumn;
                        break;
                    case stctGeneralTime:
                        column.GetText = GetTextFromGeneralTimeColumn;
                        break;
                    case stctGeneralNumber:
                        column.GetText = GetTextFromGeneralNumberColumn;
                        break;
                    default:
                        column.GetText = NULL;
                        TRACE_E("Fatal error in CFTPListingPluginDataInterface::SetupView(): unknown column type!");
                        break;
                    }
                    column.CustomData = DataOffsets[i];
#ifdef _DEBUG
                    if (column.CustomData == -1)
                        TRACE_E("Fatal error in CFTPListingPluginDataInterface::SetupView(): invalid column data offset!");
#endif
                    column.SupportSorting = 0;
                    column.LeftAlignment = col->LeftAlignment;
                    column.ID = COLUMN_ID_CUSTOM;
                    column.Width = leftPanel ? LOWORD(col->ColWidths->Width) : HIWORD(col->ColWidths->Width);
                    column.FixedWidth = leftPanel ? LOWORD(col->ColWidths->FixedWidth) : HIWORD(col->ColWidths->FixedWidth);
                    view->InsertColumn(colCount++, &column);
                    break;
                }
                }
            }
        }
    }
}

void CFTPListingPluginDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    int i;
    for (i = 0; i < Columns->Count; i++)
    {
        if (DataOffsets[i] == column->CustomData)
        {
            CSrvTypeColumn* col = Columns->At(i);
            if (leftPanel)
                col->ColWidths->FixedWidth = MAKELONG(newFixedWidth, HIWORD(col->ColWidths->FixedWidth));
            else
                col->ColWidths->FixedWidth = MAKELONG(LOWORD(col->ColWidths->FixedWidth), newFixedWidth);
            if (newFixedWidth)
                ColumnWidthWasChanged(leftPanel, column, column->Width);
            return;
        }
    }
    TRACE_E("CFTPListingPluginDataInterface::ColumnFixedWidthShouldChange(): unexpected situation: column not found!");
}

void CFTPListingPluginDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    int i;
    for (i = 0; i < Columns->Count; i++)
    {
        if (DataOffsets[i] == column->CustomData)
        {
            CSrvTypeColumn* col = Columns->At(i);
            if (leftPanel)
                col->ColWidths->Width = MAKELONG(newWidth, HIWORD(col->ColWidths->Width));
            else
                col->ColWidths->Width = MAKELONG(LOWORD(col->ColWidths->Width), newWidth);
            return;
        }
    }
    TRACE_E("CFTPListingPluginDataInterface::ColumnWidthWasChanged(): unexpected situation: column not found!");
}

void AddStrAux(char*& s, char* end, const char* text)
{
    while (s < end && *text != 0)
        *s++ = *text++;
}

BOOL CFTPListingPluginDataInterface::GetInfoLineContent(int panel, const CFileData* file, BOOL isDir,
                                                        int selectedFiles, int selectedDirs,
                                                        BOOL displaySize, const CQuadWord& selectedSize,
                                                        char* buffer, DWORD* hotTexts, int& hotTextsCount)
{
    char buf[1000];
    char num1[50];
    char num2[50];
    if (file == NULL)
    {
        if (selectedFiles == 0 && selectedDirs == 0)                                  // Information Line pro prazdny panel
            return FALSE;                                                             // text at vypise Salamander
        if (BytesColumnOffset == -1 && BlocksColumnOffset == -1 || selectedDirs != 0) // neni velikost v bytech ani blocich nebo jsou oznacene i adresare
            return FALSE;                                                             // pocty oznacenych souboru a adresaru at vypise Salamander
        // jsou-li oznacene jen soubory (u adresaru velikost v blocich nezname)
        // zjistime soucet poctu bloku
        DWORD numOffset = (BytesColumnOffset != -1 ? BytesColumnOffset : BlocksColumnOffset);
        __int64 size = 0;
        int index = 0;
        const CFileData* file2 = NULL;
        while ((file2 = SalamanderGeneral->GetPanelSelectedItem(panel, &index, NULL)) != NULL)
        {
            __int64 s = *(__int64*)(((char*)(file2->PluginData)) + numOffset);
            if (s != INT64_EMPTYNUMBER)
                size += s;
            else
                return FALSE; // je-li ve sloupci prazdna hodnota, nelze napocitat celkovou velikost, proto koncime ... pocty oznacenych souboru a adresaru at vypise Salamander
        }

        CQuadWord params[2];
        params[0].SetUI64(size);
        if (BytesColumnOffset == -1) // velikost v blocich
        {
            params[1].Set(selectedFiles, 0);
            SalamanderGeneral->NumberToStr(num1, params[0]);
            SalamanderGeneral->NumberToStr(num2, params[1]);
            SalamanderGeneral->ExpandPluralString(buf, 1000, LoadStr(IDS_PLSTR_BLOCKSINSELFILES), 2, params);
            _snprintf_s(buffer, 1000, _TRUNCATE, buf, num1, num2);
        }
        else // velikost v bytech
            SalamanderGeneral->ExpandPluralBytesFilesDirs(buffer, 1000, params[0], selectedFiles, 0, TRUE);
        return SalamanderGeneral->LookForSubTexts(buffer, hotTexts, &hotTextsCount);
    }
    else
    {
        char* s = buffer;
        char* end = buffer + 1000;
        DWORD* hot = hotTexts;
        DWORD* hotEnd = hotTexts + 100;
        FILETIME ft;
        SYSTEMTIME st;
        BOOL stEmpty = TRUE;
        BOOL separate = FALSE;
        char* beg;
        int i;
        for (i = 0; i < Columns->Count; i++)
        {
            switch (Columns->At(i)->Type)
            {
            case stctName:
            {
                int fileNameFormat;
                SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT, &fileNameFormat,
                                                      sizeof(fileNameFormat), NULL);
                char formatedFileName[MAX_PATH]; // CFileData::Name je omezeno na MAX_PATH-5 znaku
                SalamanderGeneral->AlterFileName(formatedFileName, file->Name, fileNameFormat, 0, isDir);

                beg = s;
                AddStrAux(s, end, formatedFileName);
                if (hot < hotEnd && beg < s)
                    *hot++ = MAKELONG(beg - buffer, s - beg);
                AddStrAux(s, end, ": ");
                break;
            }

                // case stctExt: break;  // je soucasti "Name"

            case stctSize:
            {
                if (separate)
                    AddStrAux(s, end, ", ");
                else
                    separate = TRUE;
                beg = s;
                if (isDir)
                    AddStrAux(s, end, LoadStr(IDS_SRVTYPE_SIZEISDIR));
                else
                {
                    SalamanderGeneral->NumberToStr(num1, file->Size);
                    AddStrAux(s, end, num1);
                }
                if (hot < hotEnd && beg < s)
                    *hot++ = MAKELONG(beg - buffer, s - beg);
                break;
            }

            case stctDate:
            case stctTime:
            {
                if (stEmpty)
                {
                    FileTimeToLocalFileTime(&file->LastWrite, &ft);
                    FileTimeToSystemTime(&ft, &st);
                    stEmpty = FALSE;
                }
                if (separate)
                    AddStrAux(s, end, ", ");
                else
                    separate = TRUE;
                beg = s;
                if (Columns->At(i)->Type == stctDate)
                {
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buf, 1000) == 0)
                        sprintf(buf, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                }
                else
                {
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buf, 1000) == 0)
                        sprintf(buf, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                }
                AddStrAux(s, end, buf);
                if (hot < hotEnd && beg < s)
                    *hot++ = MAKELONG(beg - buffer, s - beg);
                break;
            }

                // case stctType: break; // do infoline nedavame

            case stctGeneralText:
            {
                if (separate)
                    AddStrAux(s, end, ", ");
                else
                    separate = TRUE;
                beg = s;
                char* txt = *(char**)(((char*)(file->PluginData)) + DataOffsets[i]);
                if (txt != NULL)
                {
                    AddStrAux(s, end, txt);
                    if (hot < hotEnd && beg < s)
                        *hot++ = MAKELONG(beg - buffer, s - beg);
                }
                break;
            }

            case stctGeneralDate:
            {
                if (separate)
                    AddStrAux(s, end, ", ");
                else
                    separate = TRUE;
                beg = s;
                CFTPDate* date = (CFTPDate*)(((char*)(file->PluginData)) + DataOffsets[i]);
                if (date->Day != 0) // nema se zobrazit ""
                {
                    GlobalGeneralDateTimeStruct.wDay = date->Day;
                    GlobalGeneralDateTimeStruct.wMonth = date->Month;
                    GlobalGeneralDateTimeStruct.wYear = date->Year;
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &GlobalGeneralDateTimeStruct,
                                      NULL, buf, 1000) == 0)
                    {
                        sprintf(buf, "%u.%u.%u", GlobalGeneralDateTimeStruct.wDay,
                                GlobalGeneralDateTimeStruct.wMonth, GlobalGeneralDateTimeStruct.wYear);
                    }
                    AddStrAux(s, end, buf);
                    if (hot < hotEnd && beg < s)
                        *hot++ = MAKELONG(beg - buffer, s - beg);
                }
                break;
            }

            case stctGeneralTime:
            {
                if (separate)
                    AddStrAux(s, end, ", ");
                else
                    separate = TRUE;
                beg = s;
                CFTPTime* time = (CFTPTime*)(((char*)(file->PluginData)) + DataOffsets[i]);
                if (time->Hour != 24) // nema se zobrazit ""
                {
                    GlobalGeneralDateTimeStruct.wHour = time->Hour;
                    GlobalGeneralDateTimeStruct.wMinute = time->Minute;
                    GlobalGeneralDateTimeStruct.wSecond = time->Second;
                    GlobalGeneralDateTimeStruct.wMilliseconds = time->Millisecond;
                    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &GlobalGeneralDateTimeStruct,
                                      NULL, buf, 1000) == 0)
                    {
                        sprintf(buf, "%u:%02u:%02u", GlobalGeneralDateTimeStruct.wHour,
                                GlobalGeneralDateTimeStruct.wMinute, GlobalGeneralDateTimeStruct.wSecond);
                    }
                    AddStrAux(s, end, buf);
                    if (hot < hotEnd && beg < s)
                        *hot++ = MAKELONG(beg - buffer, s - beg);
                }
                break;
            }

            case stctGeneralNumber:
            {
                if (separate)
                    AddStrAux(s, end, ", ");
                else
                    separate = TRUE;
                beg = s;
                __int64 int64Val = *(__int64*)(((char*)(file->PluginData)) + DataOffsets[i]);
                if (int64Val >= 0)
                {
                    SalamanderGeneral->NumberToStr(buf, CQuadWord().SetUI64((unsigned __int64)int64Val));
                    AddStrAux(s, end, buf);
                    if (hot < hotEnd && beg < s)
                        *hot++ = MAKELONG(beg - buffer, s - beg);
                }
                else
                {
                    if (int64Val != INT64_EMPTYNUMBER) // nema se zobrazit ""
                    {
                        buf[0] = '-';
                        SalamanderGeneral->NumberToStr(buf + 1, CQuadWord().SetUI64((unsigned __int64)(-int64Val)));
                        AddStrAux(s, end, buf);
                        if (hot < hotEnd && beg < s)
                            *hot++ = MAKELONG(beg - buffer, s - beg);
                    }
                }
                break;
            }
            }
        }
        hotTextsCount = (int)(hot - hotTexts);
        if (s < end)
            *s = 0;
        else
            *(s - 1) = 0;
        return TRUE;
    }
}

//
// ****************************************************************************
// CPluginFSInterface
//

CFTPQueueItem* CreateItemForDeleteOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                            CFTPListingPluginDataInterface* dataIface,
                                            CFTPQueueItemType* type, BOOL* ok, BOOL isTopLevelDir,
                                            int hiddenFileDel, int hiddenDirDel,
                                            CFTPQueueItemState* state, DWORD* problemID,
                                            int* skippedItems, int* uiNeededItems)
{
    CFTPQueueItem* item = NULL;
    *type = fqitNone;
    BOOL isFile = TRUE;
    if (rightsCol != -1 && IsUNIXLink(dataIface->GetStringFromColumn(*f, rightsCol)))
    { // link
        *type = fqitDeleteLink;
        item = new CFTPQueueItemDel;
        if (item != NULL && !((CFTPQueueItemDel*)item)->SetItemDel(f->Hidden))
            *ok = FALSE;
    }
    else
    {
        if (isDir) // adresar
        {
            isFile = FALSE;
            *type = fqitDeleteExploreDir;
            item = new CFTPQueueItemDelExplore;
            if (item != NULL && !((CFTPQueueItemDelExplore*)item)->SetItemDelExplore(isTopLevelDir, f->Hidden))
                *ok = FALSE;
        }
        else // soubor
        {
            *type = fqitDeleteFile;
            item = new CFTPQueueItemDel;
            if (item != NULL && !((CFTPQueueItemDel*)item)->SetItemDel(f->Hidden))
                *ok = FALSE;
        }
    }
    if (f->Hidden)
    {
        if (isFile)
        {
            switch (hiddenFileDel)
            {
            case HIDDENFILEDEL_USERPROMPT:
                *state = sqisUserInputNeeded;
                *problemID = ITEMPR_FILEISHIDDEN;
                (*uiNeededItems)++;
                break;
            case HIDDENFILEDEL_DELETEIT:
                break;
            case HIDDENFILEDEL_SKIP:
                *state = sqisSkipped;
                *problemID = ITEMPR_FILEISHIDDEN;
                (*skippedItems)++;
                break;
            }
        }
        else
        {
            switch (hiddenDirDel)
            {
            case HIDDENDIRDEL_USERPROMPT:
                *state = sqisUserInputNeeded;
                *problemID = ITEMPR_DIRISHIDDEN;
                (*uiNeededItems)++;
                break;
            case HIDDENDIRDEL_DELETEIT:
                break;
            case HIDDENDIRDEL_SKIP:
                *state = sqisSkipped;
                *problemID = ITEMPR_DIRISHIDDEN;
                (*skippedItems)++;
                break;
            }
        }
    }
    return item;
}

BOOL CPluginFSInterface::Delete(const char* fsName, int mode, HWND parent, int panel,
                                int selectedFiles, int selectedDirs, BOOL& cancelOrError)
{
    CALL_STACK_MESSAGE6("CPluginFSInterface::Delete(%s, %d, , %d, %d, %d, )",
                        fsName, mode, panel, selectedFiles, selectedDirs);

    if (ControlConnection == NULL)
    {
        TRACE_E("Unexpected situation in CPluginFSInterface::Delete(): ControlConnection == NULL!");
        cancelOrError = TRUE; // preruseni operace
        return TRUE;
    }

    // otestujeme jestli neni v panelu jen simple listing -> v tom pripade aspon zatim nic neumime
    CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)SalamanderGeneral->GetPanelPluginData(panel);
    if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_NEEDPARSEDLISTING),
                                         LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
        cancelOrError = TRUE; // preruseni operace
        return TRUE;
    }

    // priprava textu popisujiciho to, s cim pracujeme ("file "test.txt"", atp.)
    char subjectSrc[MAX_PATH + 100];
    SalamanderGeneral->GetCommonFSOperSourceDescr(subjectSrc, MAX_PATH + 100, panel,
                                                  selectedFiles, selectedDirs, NULL, FALSE, FALSE);
    char dlgSubjectSrc[MAX_PATH + 100];
    SalamanderGeneral->GetCommonFSOperSourceDescr(dlgSubjectSrc, MAX_PATH + 100, panel,
                                                  selectedFiles, selectedDirs, NULL, FALSE, TRUE);
    cancelOrError = FALSE;
    if (mode == 1)
    {
        BOOL CnfrmFileDirDel;
        SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEDIRDEL, &CnfrmFileDirDel, 4, NULL);

        if (CnfrmFileDirDel)
        {
            // sestaveni dotazu na mazani
            char subject[MAX_PATH + 200];
            sprintf(subject, LoadStr(IDS_DELETEFROMFTP), subjectSrc);

            // otevreme messagebox s dotazem na delete
            const char* Shell32DLLName = "shell32.dll";
            HINSTANCE Shell32DLL;
            Shell32DLL = HANDLES(LoadLibraryEx(Shell32DLLName, NULL, LOAD_LIBRARY_AS_DATAFILE));
            HICON hIcon = NULL;
            if (Shell32DLL != NULL)
            {
                hIcon = (HICON)HANDLES(LoadImage(Shell32DLL, MAKEINTRESOURCE(WindowsVistaAndLater ? 16777 : 161), // delete icon
                                                 IMAGE_ICON, 32, 32, SalamanderGeneral->GetIconLRFlags()));
                HANDLES(FreeLibrary(Shell32DLL));
            }
            INT_PTR res = CConfirmDeleteDlg(parent, subject, hIcon).Execute();
            UpdateWindow(SalamanderGeneral->GetMainWindowHWND()); // aby na zbytek dialogu user nemusel koukat celou operaci
            if (hIcon != NULL)
                HANDLES(DestroyIcon(hIcon));

            if (res != IDOK)
            {
                cancelOrError = TRUE; // preruseni operace
                return TRUE;
            }
        }
    }

    // ignorujeme hodnoty "Confirm on" z konfigurace (zakryva je pluginove nastaveni reseni situaci kolem Delete operace)
    // BOOL ConfirmOnNotEmptyDirDelete, ConfirmOnSystemHiddenFileDelete, ConfirmOnSystemHiddenDirDelete;
    // SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMNEDIRDEL, &ConfirmOnNotEmptyDirDelete, 4, NULL);
    // SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEDEL, &ConfirmOnSystemHiddenFileDelete, 4, NULL);
    // SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHDIRDEL, &ConfirmOnSystemHiddenDirDelete, 4, NULL);

    cancelOrError = TRUE; // predpripravime cancel/error operace
    // zalozime objekt operace
    CFTPOperation* oper = new CFTPOperation;
    if (oper != NULL)
    {
        oper->SetEncryptControlConnection(ControlConnection->GetEncryptControlConnection());
        oper->SetEncryptDataConnection(ControlConnection->GetEncryptDataConnection());
        CCertificate* cert = ControlConnection->GetCertificate();
        oper->SetCertificate(cert);
        if (cert)
            cert->Release();
        oper->SetCompressData(ControlConnection->GetCompressData());
        if (ControlConnection->InitOperation(oper)) // inicializace spojeni se serverem podle "control connection"
        {
            oper->SetBasicData(dlgSubjectSrc, (AutodetectSrvType ? NULL : LastServerType));
            char path[2 * MAX_PATH];
            sprintf(path, "%s:", fsName);
            int pathLen = (int)strlen(path);
            MakeUserPart(path + pathLen, 2 * MAX_PATH - pathLen);
            CFTPServerPathType pathType = ControlConnection->GetFTPServerPathType(Path);
            oper->SetOperationDelete(path, FTPGetPathDelimiter(pathType), TRUE, selectedDirs > 0,
                                     Config.OperationsNonemptyDirDel, Config.OperationsHiddenFileDel,
                                     Config.OperationsHiddenDirDel);
            int operUID;
            if (FTPOperationsList.AddOperation(oper, &operUID))
            {
                BOOL ok = TRUE;

                // sestavime frontu polozek operace
                CFTPQueue* queue = new CFTPQueue;
                if (queue != NULL)
                {
                    if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
                        dataIface = NULL; // zajima nas jen data iface typu CFTPListingPluginDataInterface
                    int rightsCol = -1;   // index sloupce s pravy (pouziva se pro detekci linku)
                    if (dataIface != NULL)
                        rightsCol = dataIface->FindRightsColumn();
                    const CFileData* f = NULL; // ukazatel na soubor/adresar/link v panelu, ktery se ma zpracovat
                    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
                    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
                    int skippedItems = 0;  // pocet skipnutych polozek vlozenych do fronty
                    int uiNeededItems = 0; // pocet user-input-needed polozek vlozenych do fronty
                    int index = 0;
                    while (1)
                    {
                        // vyzvedneme data o zpracovavanem souboru
                        if (focused)
                            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
                        else
                            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

                        // zpracujeme soubor/adresar/link
                        if (f != NULL)
                        {
                            CFTPQueueItemType type;
                            CFTPQueueItemState state = sqisWaiting;
                            DWORD problemID = ITEMPR_OK;
                            CFTPQueueItem* item = CreateItemForDeleteOperation(f, isDir, rightsCol, dataIface, &type, &ok, TRUE,
                                                                               Config.OperationsHiddenFileDel,
                                                                               Config.OperationsHiddenDirDel,
                                                                               &state, &problemID, &skippedItems, &uiNeededItems);
                            if (item != NULL)
                            {
                                if (ok)
                                    item->SetItem(-1, type, state, problemID, Path, f->Name);
                                if (!ok || !queue->AddItem(item)) // pridani operace do fronty
                                {
                                    ok = FALSE;
                                    delete item;
                                }
                            }
                            else
                            {
                                TRACE_E(LOW_MEMORY);
                                ok = FALSE;
                            }
                        }
                        // zjistime jestli ma cenu pokracovat (pokud neni chyba a existuje jeste nejaka dalsi oznacena polozka)
                        if (!ok || focused || f == NULL)
                            break;
                    }
                    if (ok)
                        oper->SetChildItems(queue->GetCount(), skippedItems, 0, uiNeededItems);
                    else
                    {
                        delete queue;
                        queue = NULL;
                    }
                }
                else
                {
                    TRACE_E(LOW_MEMORY);
                    ok = FALSE;
                }

                if (ok) // mame naplnenou frontu s polozkama operace
                {
                    oper->SetQueue(queue); // nastavime operaci frontu jejich polozek
                    queue = NULL;
                    if (Config.DeleteAddToQueue)
                        cancelOrError = FALSE; // provest operaci pozdeji -> prozatim jde o uspech operace
                    else                       // provest operaci v aktivni "control connection"
                    {
                        // otevreme okno s progresem operace a spustime operaci
                        if (RunOperation(SalamanderGeneral->GetMsgBoxParent(), operUID, oper, NULL))
                            cancelOrError = FALSE; // uspech operace
                        else
                            ok = FALSE;
                    }
                }
                if (!ok)
                    FTPOperationsList.DeleteOperation(operUID, TRUE);
                oper = NULL; // operace uz je pridana v poli, nebudeme ji uvolnovat pres 'delete' (viz nize)
            }
        }
        if (oper != NULL)
            delete oper;
    }
    else
        TRACE_E(LOW_MEMORY);
    return TRUE;
}

CFTPQueueItem* CreateItemForCopyOrMoveOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                                CFTPListingPluginDataInterface* dataIface,
                                                CFTPQueueItemType* type, int transferMode,
                                                CFTPOperation* oper, BOOL copy, const char* targetPath,
                                                const char* targetName, CQuadWord* size,
                                                BOOL* sizeInBytes, CQuadWord* totalSize)
{
    CFTPQueueItem* item = NULL;
    *type = fqitNone;

    char *name, *ext;               // pomocne promenne pro auto-detect-transfer-mode
    BOOL asciiTransferMode = FALSE; // pomocna promenna pro auto-detect-transfer-mode
    char buffer[MAX_PATH];          // pomocna promenna pro auto-detect-transfer-mode
    BOOL isLink = rightsCol != -1 && IsUNIXLink(dataIface->GetStringFromColumn(*f, rightsCol));
    if (isLink || !isDir) // pokud se pouziva 'asciiTransferMode', napocteme ho
    {
        if (transferMode == trmAutodetect)
        {
            if (dataIface != NULL) // na VMS si musime nechat jmeno "oriznout" na zaklad (cislo verze pri porovnani s maskami vadi)
                dataIface->GetBasicName(*f, &name, &ext, buffer);
            else
            {
                name = f->Name;
                ext = f->Ext;
            }
            asciiTransferMode = oper->IsASCIIFile(name, ext);
        }
        else
            asciiTransferMode = transferMode == trmASCII;
    }
    if (isLink)
    { // link
        *type = copy ? fqitCopyResolveLink : fqitMoveResolveLink;

        BOOL dateAndTimeValid = FALSE;
        CFTPDate date;
        CFTPTime time;
        if (dataIface != NULL)
            dataIface->GetLastWriteDateAndTime(*f, &dateAndTimeValid, &date, &time);
        if (!dateAndTimeValid)
        {
            memset(&date, 0, sizeof(date));
            memset(&time, 0, sizeof(time));
        }

        item = new CFTPQueueItemCopyOrMove;
        if (item != NULL)
        {
            ((CFTPQueueItemCopyOrMove*)item)->SetItemCopyOrMove(targetPath, targetName, CQuadWord(-1, -1) /* neznama velikost */, asciiTransferMode, TRUE, TGTFILESTATE_UNKNOWN, dateAndTimeValid, date, time);
        }
    }
    else
    {
        if (isDir) // adresar
        {
            *type = copy ? fqitCopyExploreDir : fqitMoveExploreDir;
            item = new CFTPQueueItemCopyMoveExplore;
            if (item != NULL)
            {
                ((CFTPQueueItemCopyMoveExplore*)item)->SetItemCopyMoveExplore(targetPath, targetName, TGTDIRSTATE_UNKNOWN);
            }
        }
        else // soubor
        {
            *type = copy ? fqitCopyFileOrFileLink : fqitMoveFileOrFileLink;
            item = new CFTPQueueItemCopyOrMove;
            if (dataIface != NULL && dataIface->GetSize(*f, *size, *sizeInBytes))
                *totalSize += *size;
            else
                size->Set(-1, -1); // nezname velikost souboru

            BOOL dateAndTimeValid = FALSE;
            CFTPDate date;
            CFTPTime time;
            if (dataIface != NULL)
                dataIface->GetLastWriteDateAndTime(*f, &dateAndTimeValid, &date, &time);
            if (!dateAndTimeValid)
            {
                memset(&date, 0, sizeof(date));
                memset(&time, 0, sizeof(time));
            }

            if (item != NULL)
            {
                ((CFTPQueueItemCopyOrMove*)item)->SetItemCopyOrMove(targetPath, targetName, *size, asciiTransferMode, *sizeInBytes, TGTFILESTATE_UNKNOWN, dateAndTimeValid, date, time);
            }
        }
    }
    return item;
}

BOOL CPluginFSInterface::CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                          int panel, int selectedFiles, int selectedDirs,
                                          char* targetPath, BOOL& operationMask,
                                          BOOL& cancelOrHandlePath, HWND dropTarget)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::CopyOrMoveFromFS(%d, %d, %s, , %d, %d, %d, , , ,)",
                        copy, mode, fsName, panel, selectedFiles, selectedDirs);

    if (ControlConnection == NULL)
    {
        TRACE_E("Unexpected situation in CPluginFSInterface::CopyOrMoveFromFS(): ControlConnection == NULL!");
        cancelOrHandlePath = TRUE;
        return TRUE; // cancel operace
    }

    // otestujeme jestli neni v panelu jen simple listing -> v tom pripade aspon zatim nic neumime
    CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)SalamanderGeneral->GetPanelPluginData(panel);
    if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_NEEDPARSEDLISTING),
                                         LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
        cancelOrHandlePath = TRUE; // cancel operace
        return TRUE;
    }

    // sestaveni nadpisu editline s cilem kopirovani/presouvani
    char subjectSrc[MAX_PATH + 100];
    SalamanderGeneral->GetCommonFSOperSourceDescr(subjectSrc, MAX_PATH + 100, panel,
                                                  selectedFiles, selectedDirs, NULL, FALSE, FALSE);
    char dlgSubjectSrc[MAX_PATH + 100];
    SalamanderGeneral->GetCommonFSOperSourceDescr(dlgSubjectSrc, MAX_PATH + 100, panel,
                                                  selectedFiles, selectedDirs, NULL, FALSE, TRUE);
    char subject[MAX_PATH + 200];
    sprintf(subject, LoadStr(copy ? IDS_COPYFROMFTP : IDS_MOVEFROMFTP), subjectSrc);

    if (mode == 1 && targetPath[0] != 0) // jen pri prvnim otevirani dialogu + je-li vybrana cilova cesta
    {
        SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
        SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_TARGET); // default akce = prace s cestou v cilovem panelu
    }
    if (mode != 3 && mode != 5)
    {
        char** history;
        int historyCount;
        SalamanderGeneral->GetStdHistoryValues(SALHIST_COPYMOVETGT, &history, &historyCount);
        CCopyMoveDlg dlg(parent, targetPath, 2 * MAX_PATH, LoadStr(copy ? IDS_COPYTITLE : IDS_MOVETITLE),
                         subject, history, historyCount, copy ? IDD_COPYMOVEDLG : IDH_MOVEDLG);
        INT_PTR res = dlg.Execute();
        UpdateWindow(SalamanderGeneral->GetMainWindowHWND()); // aby na zbytek dialogu user nemusel koukat celou operaci
        if (res == IDOK)
        {
            cancelOrHandlePath = TRUE;
            operationMask = TRUE;
            return FALSE; // nechame cestu rozanalyzovat std. zpusobem
        }
        else
        {
            cancelOrHandlePath = TRUE;
            return TRUE; // cancel operace
        }
    }
    else
    {
        const char* opMask = NULL; // maska operace
        if (mode == 5)             // cil operace byl zadan pres drag&drop
        {
            // pokud jde o diskovou cestu, pak jen nastavime masku operace a pokracujeme (stejne s 'mode'==3);
            // pokud jde o cestu do archivu nebo na FS, vyhodime error "not supported"

            BOOL ok = FALSE;
            opMask = "*.*";
            int type;
            char* secondPart;
            BOOL isDir;
            if (targetPath[0] != 0 && targetPath[1] == ':' ||   // diskova cesta (C:\path)
                targetPath[0] == '\\' && targetPath[1] == '\\') // UNC cesta (\\server\share\path)
            {                                                   // pridame na konec backslash, aby slo k kazdem pripade o cestu (pri 'mode'==5 jde vzdy o cestu)
                SalamanderGeneral->SalPathAddBackslash(targetPath, MAX_PATH);
            }
            lstrcpyn(subject, LoadStr(IDS_FTPERRORTITLE), MAX_PATH + 200);
            if (SalamanderGeneral->SalParsePath(parent, targetPath, type, isDir, secondPart,
                                                subject, NULL, FALSE,
                                                NULL, NULL, NULL, 2 * MAX_PATH))
            {
                switch (type)
                {
                case PATH_TYPE_WINDOWS:
                {
                    if (*secondPart != 0) // asi by vubec nikdy nemelo nastat
                    {
                        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_DRAGDROP_TGTNOTEXIST),
                                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                    else
                        ok = TRUE;
                    break;
                }

                default: // archiv nebo FS, jen ohlasime "not supported"
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_DRAGDROP_TGTARCORFS),
                                                     LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    break;
                }
                }
            }
            if (!ok)
            {
                cancelOrHandlePath = TRUE;
                return TRUE;
            }
        }

        // cesta je rozanalyzovana, zacneme operaci

        // ignorujeme hodnoty "Confirm on" z konfigurace (zakryva je nastaveni reseni problemu "soubor jiz existuje" - viz FILEALREADYEXISTS_XXX)
        // BOOL ConfirmOnFileOverwrite, ConfirmOnDirOverwrite, ConfirmOnSystemHiddenFileOverwrite;
        // SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
        // SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMDIROVER, &ConfirmOnDirOverwrite, 4, NULL);
        // SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);

        // najdeme si masku operace (cilova cesta je v 'targetPath')
        if (opMask == NULL)
        {
            opMask = targetPath;
            while (*opMask != 0)
                opMask++;
            opMask++;
        }

        BOOL success = FALSE; // predpripravime cancel/error operace
        // zalozime objekt operace
        CFTPOperation* oper = new CFTPOperation;
        if (oper != NULL)
        {
            oper->SetEncryptControlConnection(ControlConnection->GetEncryptControlConnection());
            oper->SetEncryptDataConnection(ControlConnection->GetEncryptDataConnection());
            CCertificate* cert = ControlConnection->GetCertificate();
            oper->SetCertificate(cert);
            if (cert)
                cert->Release();
            oper->SetCompressData(ControlConnection->GetCompressData());
            if (ControlConnection->InitOperation(oper)) // inicializace spojeni se serverem podle "control connection"
            {
                oper->SetBasicData(dlgSubjectSrc, (AutodetectSrvType ? NULL : LastServerType));
                char path[2 * MAX_PATH];
                sprintf(path, "%s:", fsName);
                int pathLen = (int)strlen(path);
                MakeUserPart(path + pathLen, 2 * MAX_PATH - pathLen);
                char asciiFileMasks[MAX_GROUPMASK];
                Config.ASCIIFileMasks->GetMasksString(asciiFileMasks);
                CFTPServerPathType pathType = ControlConnection->GetFTPServerPathType(Path);
                BOOL is_AS_400_QSYS_LIB_Path = pathType == ftpsptAS400 &&
                                               FTPIsPrefixOfServerPath(ftpsptAS400, "/QSYS.LIB", Path);
                if (oper->SetOperationCopyMoveDownload(copy, path, FTPGetPathDelimiter(pathType),
                                                       !copy, copy ? FALSE : (selectedDirs > 0),
                                                       targetPath, '\\', TRUE, selectedDirs > 0, asciiFileMasks,
                                                       TransferMode == trmAutodetect, TransferMode == trmASCII,
                                                       Config.OperationsCannotCreateFile,
                                                       Config.OperationsCannotCreateDir,
                                                       Config.OperationsFileAlreadyExists,
                                                       Config.OperationsDirAlreadyExists,
                                                       Config.OperationsRetryOnCreatedFile,
                                                       Config.OperationsRetryOnResumedFile,
                                                       Config.OperationsAsciiTrModeButBinFile))
                {
                    int operUID;
                    if (FTPOperationsList.AddOperation(oper, &operUID))
                    {
                        BOOL ok = TRUE;

                        // sestavime frontu polozek operace
                        CFTPQueue* queue = new CFTPQueue;
                        if (queue != NULL)
                        {
                            if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
                                dataIface = NULL; // zajima nas jen data iface typu CFTPListingPluginDataInterface
                            int rightsCol = -1;   // index sloupce s pravy (pouziva se pro detekci linku)
                            if (dataIface != NULL)
                                rightsCol = dataIface->FindRightsColumn();
                            CQuadWord totalSize(0, 0); // celkova velikost (v bytech nebo blocich)
                            CQuadWord size(-1, -1);    // promenna pro velikost aktualniho souboru
                            BOOL sizeInBytes = TRUE;   // TRUE/FALSE = velikosti v bytech/blocich (na jednom listingu se nemuze stridat - viz CFTPListingPluginDataInterface::GetSize())
                            const CFileData* f = NULL; // ukazatel na soubor/adresar/link v panelu, ktery se ma zpracovat
                            BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
                            BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
                            BOOL donotUseOpMask = strcmp(opMask, "*.*") == 0 || strcmp(opMask, "*") == 0;
                            int index = 0;
                            while (1)
                            {
                                // vyzvedneme data o zpracovavanem souboru
                                if (focused)
                                    f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
                                else
                                    f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

                                // zpracujeme soubor/adresar/link
                                if (f != NULL)
                                {
                                    // vytvorime cilove jmeno podle operacni masky
                                    char targetName[2 * MAX_PATH];
                                    if (!is_AS_400_QSYS_LIB_Path)
                                    {
                                        if (donotUseOpMask)
                                            lstrcpyn(targetName, f->Name, 2 * MAX_PATH); // masky orezavaji '.' z koncu jmen, coz neni vzdy OK (slouci se napr. adresare "a.b" a "a.b.") - neni asi casty problem, proto aspon zatim resime jen takto provizorne
                                        else
                                            SalamanderGeneral->MaskName(targetName, 2 * MAX_PATH, f->Name, opMask);
                                    }
                                    else
                                    {
                                        char mbrName[MAX_PATH];
                                        FTPAS400CutFileNamePart(mbrName, f->Name);
                                        if (donotUseOpMask)
                                            lstrcpyn(targetName, mbrName, 2 * MAX_PATH); // masky orezavaji '.' z koncu jmen, coz neni vzdy OK (slouci se napr. adresare "a.b" a "a.b.") - neni asi casty problem, proto aspon zatim resime jen takto provizorne
                                        else
                                            SalamanderGeneral->MaskName(targetName, 2 * MAX_PATH, mbrName, opMask);
                                    }

                                    CFTPQueueItemType type;
                                    CFTPQueueItem* item = CreateItemForCopyOrMoveOperation(f, isDir, rightsCol,
                                                                                           dataIface, &type,
                                                                                           TransferMode, oper,
                                                                                           copy, targetPath,
                                                                                           targetName, &size,
                                                                                           &sizeInBytes, &totalSize);
                                    if (item != NULL)
                                    {
                                        if (ok)
                                            item->SetItem(-1, type, sqisWaiting, ITEMPR_OK, Path, f->Name);
                                        if (!ok || !queue->AddItem(item)) // pridani operace do fronty
                                        {
                                            ok = FALSE;
                                            delete item;
                                        }
                                    }
                                    else
                                    {
                                        TRACE_E(LOW_MEMORY);
                                        ok = FALSE;
                                    }
                                }
                                // zjistime jestli ma cenu pokracovat (pokud neni chyba a existuje jeste nejaka dalsi oznacena polozka)
                                if (!ok || focused || f == NULL)
                                    break;
                            }
                            if (ok)
                            {
                                oper->SetChildItems(queue->GetCount(), 0, 0, 0);
                                oper->AddToTotalSize(totalSize, sizeInBytes);
                            }
                            else
                            {
                                delete queue;
                                queue = NULL;
                            }
                        }
                        else
                        {
                            TRACE_E(LOW_MEMORY);
                            ok = FALSE;
                        }

                        if (ok) // mame naplnenou frontu s polozkama operace
                        {
                            oper->SetQueue(queue); // nastavime operaci frontu jejich polozek
                            queue = NULL;
                            if (Config.DownloadAddToQueue)
                                success = TRUE; // provest operaci pozdeji -> prozatim jde o uspech operace
                            else                // provest operaci v aktivni "control connection"
                            {
                                // otevreme okno s progresem operace a spustime operaci
                                if (RunOperation(SalamanderGeneral->GetMsgBoxParent(), operUID, oper, dropTarget))
                                    success = TRUE; // uspech operace
                                else
                                    ok = FALSE;
                            }
                        }
                        if (!ok)
                            FTPOperationsList.DeleteOperation(operUID, TRUE);
                        oper = NULL; // operace uz je pridana v poli, nebudeme ji uvolnovat pres 'delete' (viz nize)
                    }
                }
            }
            if (oper != NULL)
                delete oper;
        }
        else
            TRACE_E(LOW_MEMORY);

        if (success)
        {
            cancelOrHandlePath = FALSE; // uspech operace
            targetPath[0] = 0;          // zadne jmeno se nema fokusit (to by muselo byt copy uvnitr FTP serveru, coz zatim neumime)
        }
        else
            cancelOrHandlePath = TRUE; // cancel operace
        return TRUE;
    }
}

CFTPQueueItem* CreateItemForChangeAttrsOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                                 CFTPListingPluginDataInterface* dataIface,
                                                 CFTPQueueItemType* type, BOOL* ok,
                                                 CFTPQueueItemState* state, DWORD* problemID,
                                                 int* skippedItems, int* uiNeededItems,
                                                 BOOL* skip, BOOL selFiles,
                                                 BOOL selDirs, BOOL includeSubdirs,
                                                 DWORD attrAndMask, DWORD attrOrMask,
                                                 int operationsUnknownAttrs)
{
    CFTPQueueItem* item = NULL;
    *type = fqitNone;
    *state = sqisWaiting;
    *problemID = ITEMPR_OK;
    *skip = FALSE; // TRUE pokud se vubec nema soubor/adresar/link zpracovavat (nema vztah ke skippedItems)
    char* rights = NULL;
    if (rightsCol != -1 && IsUNIXLink((rights = dataIface->GetStringFromColumn(*f, rightsCol))))
    {                       // link
        if (includeSubdirs) // zkusime jestli nejde o link na adresar (jinak neni co delat)
        {
            *type = fqitChAttrsResolveLink;
            item = new CFTPQueueItem;
        }
        else
            *skip = TRUE; // linku atributy menit nejdou, takze neni co delat
    }
    else
    {
        // vypocteme nova prava pro soubor/adresar
        DWORD actAttr;
        DWORD attrDiff = 0;
        BOOL attrErr = FALSE;
        if (rightsCol != -1 && GetAttrsFromUNIXRights(&actAttr, &attrDiff, rights))
        {
            DWORD changeMask = (~attrAndMask | attrOrMask) & 0777;
            if ((!includeSubdirs || !isDir) &&                                                   // takto nelze optimalizovat "explore dir"
                (attrDiff & changeMask) == 0 &&                                                  // nemame zmenit zadny neznamy atribut
                (actAttr & changeMask) == (((actAttr & attrAndMask) | attrOrMask) & changeMask)) // nemame zmenit zadny znamy atribut
            {                                                                                    // neni co delat (zadna zmena atributu)
                *skip = TRUE;
            }
            else
            {
                if (((attrDiff & attrAndMask) & attrOrMask) != (attrDiff & attrAndMask))
                {                        // prusvih, neznamy atribut ma byt zachovan, coz neumime
                    actAttr |= attrDiff; // dame tam aspon 'x', kdyz uz neumime 's' nebo 't' nebo co to tam ted vlastne je (viz UNIXova prava)
                    attrErr = TRUE;
                }
                actAttr = (actAttr & attrAndMask) | attrOrMask;
            }
        }
        else // nezname prava
        {
            actAttr = attrOrMask; // predpokladame zadna prava (actAttr==0)
            if (((~attrAndMask | attrOrMask) & 0777) != 0777)
            { // prusvih, nezname prava a nejaky atribut ma byt zachovan (nezname jeho hodnotu -> neumime ho zachovat)
                attrErr = TRUE;
            }
        }

        if (!*skip)
        {
            if (isDir) // adresar
            {
                if (includeSubdirs) // zmena atributu se provede i na obsahu adresare
                {                   // pripadna chyba prav se ohlasi az do vlozene polozky fqitChAttrsDir
                    *type = fqitChAttrsExploreDir;
                    item = new CFTPQueueItemChAttrExplore;
                    if (item != NULL)
                        ((CFTPQueueItemChAttrExplore*)item)->SetItemChAttrExplore(rights);
                }
                else
                {
                    if (selDirs)
                    { // bez podadresaru se uloha redukuje na nastaveni atributu vybraneho adresare
                        if (attrErr)
                        {
                            switch (operationsUnknownAttrs)
                            {
                            case UNKNOWNATTRS_IGNORE:
                                attrErr = FALSE;
                                break;

                            case UNKNOWNATTRS_SKIP:
                            {
                                *state = sqisSkipped;
                                *problemID = ITEMPR_UNKNOWNATTRS;
                                (*skippedItems)++;
                                break;
                            }

                            default: // UNKNOWNATTRS_USERPROMPT
                            {
                                *state = sqisUserInputNeeded;
                                *problemID = ITEMPR_UNKNOWNATTRS;
                                (*uiNeededItems)++;
                                break;
                            }
                            }
                        }
                        if (!attrErr)
                            rights = NULL; // je-li vse OK, neni duvod si pamatovat puvodni prava
                        *type = fqitChAttrsDir;
                        item = new CFTPQueueItemChAttrDir;
                        if (item != NULL)
                        {
                            if (!((CFTPQueueItemChAttrDir*)item)->SetItemDir(0, 0, 0, 0))
                                *ok = FALSE;
                            else
                                ((CFTPQueueItemChAttrDir*)item)->SetItemChAttrDir((WORD)actAttr, rights, attrErr);
                        }
                    }
                    else
                        *skip = TRUE; // na adresare pry nemame sahat
                }
            }
            else // soubor
            {
                if (selFiles)
                {
                    if (attrErr)
                    {
                        switch (operationsUnknownAttrs)
                        {
                        case UNKNOWNATTRS_IGNORE:
                            attrErr = FALSE;
                            break;

                        case UNKNOWNATTRS_SKIP:
                        {
                            *state = sqisSkipped;
                            *problemID = ITEMPR_UNKNOWNATTRS;
                            (*skippedItems)++;
                            break;
                        }

                        default: // UNKNOWNATTRS_USERPROMPT
                        {
                            *state = sqisUserInputNeeded;
                            *problemID = ITEMPR_UNKNOWNATTRS;
                            (*uiNeededItems)++;
                            break;
                        }
                        }
                    }
                    if (!attrErr)
                        rights = NULL; // je-li vse OK, neni duvod si pamatovat puvodni prava
                    *type = fqitChAttrsFile;
                    item = new CFTPQueueItemChAttr;
                    if (item != NULL)
                        ((CFTPQueueItemChAttr*)item)->SetItemChAttr((WORD)actAttr, rights, attrErr);
                }
                else
                    *skip = TRUE; // na soubory pry nemame sahat
            }
        }
    }
    return item;
}
