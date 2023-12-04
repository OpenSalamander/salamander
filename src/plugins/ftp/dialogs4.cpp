// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CEditSrvTypeColumnDlg
//

CEditSrvTypeColumnDlg::CEditSrvTypeColumnDlg(HWND parent,
                                             TIndirectArray<CSrvTypeColumn>* columnsData,
                                             int* editedColumn, BOOL edit)
    : CCenteredDialog(HLanguage, IDD_EDITSRVTYCOLUMN, IDD_EDITSRVTYCOLUMN, parent)
{
    ColumnsData = columnsData;
    EditedColumn = editedColumn;
    Edit = edit;
    LastUsedIndexForName = -1;
    LastUsedIndexForDescr = -1;
    FirstSelNotifyAfterTransfer = FALSE;
}

BOOL IsValidIdentifier(const char* s, int* errResID)
{
    if (s == NULL || *s == 0 || _stricmp(s, "is_dir") == 0 || _stricmp(s, "is_hidden") == 0 ||
        _stricmp(s, "is_link") == 0)
    {
        if (errResID != NULL)
            *errResID = (s == NULL || *s == 0) ? IDS_STC_ERR_IDEMPTY : IDS_STC_ERR_IDRESERVED;
        return FALSE;
    }
    if (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z' || *s == '_')
    {
        s++;
        while (*s != 0 && (*s >= 'a' && *s <= 'z' || *s >= 'A' && *s <= 'Z' ||
                           *s >= '0' && *s <= '9' || *s == '_'))
            s++;
    }
    if (*s != 0)
    {
        if (errResID != NULL)
            *errResID = IDS_STC_ERR_IDINVALID;
        return FALSE;
    }
    return TRUE;
}

void CEditSrvTypeColumnDlg::Validate(CTransferInfo& ti)
{
    // kontrola syntaxe, nepouziti vyhrazenych ID (is_dir+is_hidden+is_link), unikatnosti a neprazdnosti ID
    char id[STC_ID_MAX_SIZE];
    BOOL ok = TRUE;
    ti.EditLine(IDE_COL_ID, id, STC_ID_MAX_SIZE);
    int errResID = 0;
    if (IsValidIdentifier(id, &errResID))
    {
        int j;
        for (j = 0; j < ColumnsData->Count; j++)
        {
            if ((!Edit || *EditedColumn != j) &&
                SalamanderGeneral->StrICmp(id, ColumnsData->At(j)->ID) == 0)
            {
                ok = FALSE;
                errResID = IDS_STC_ERR_IDNOTUNIQUE;
                break;
            }
        }
    }
    else
        ok = FALSE;
    if (!ok)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(errResID), LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_COL_ID);
        return;
    }

    // kontrola syntaxe "empty value"
    CSrvTypeColumnTypes type = stctNone;
    HWND combo = GetDlgItem(HWindow, IDC_COL_TYPE);
    int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (i != CB_ERR)
    {
        // x64 - ITEMDATA nedrzi ukazatel, pretypovani na (int) je bezpecne
        int t = (int)SendMessage(combo, CB_GETITEMDATA, i, 0); // ziskame vybrany typ sloupce
        if (t > stctNone && t < stctLastItem)
            type = (CSrvTypeColumnTypes)t;
    }
    char emptyVal[STC_EMPTYVAL_MAX_SIZE];
    ti.EditLine(IDE_COL_EMPTY, emptyVal, STC_EMPTYVAL_MAX_SIZE);
    if (!GetColumnEmptyValue(emptyVal[0] != 0 ? emptyVal : NULL, type, NULL, NULL, NULL, FALSE))
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_STC_ERR_INVALEMPTY),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_COL_EMPTY);
        return;
    }

    // kontrola neprazdnosti jmena a popisu sloupce
    BOOL errOnName = LastUsedIndexForName == -1 && GetWindowTextLength(GetDlgItem(HWindow, IDC_COL_NAME)) == 0;
    BOOL errOnDescr = LastUsedIndexForDescr == -1 && GetWindowTextLength(GetDlgItem(HWindow, IDC_COL_DESCR)) == 0;
    if (errOnName || errOnDescr)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(errOnName ? IDS_STC_ERR_EMPTYNAME : IDS_STC_ERR_EMPTYDESCR),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(errOnName ? IDC_COL_NAME : IDC_COL_DESCR);
        return;
    }
}

void CEditSrvTypeColumnDlg::Transfer(CTransferInfo& ti)
{
    char id[STC_ID_MAX_SIZE];
    id[0] = 0;
    char emptyVal[STC_EMPTYVAL_MAX_SIZE];
    emptyVal[0] = 0;
    if (ti.Type == ttDataToWindow)
    {
        char emptyBuff[] = "";
        ti.EditLine(IDE_COL_ID, Edit ? ColumnsData->At(*EditedColumn)->ID : emptyBuff, STC_ID_MAX_SIZE); // ID nemuze byt NULL
        ti.EditLine(IDE_COL_EMPTY, Edit ? HandleNULLStr(ColumnsData->At(*EditedColumn)->EmptyValue) : emptyBuff,
                    STC_EMPTYVAL_MAX_SIZE);
    }
    else
    {
        ti.EditLine(IDE_COL_ID, id, STC_ID_MAX_SIZE);
        ti.EditLine(IDE_COL_EMPTY, emptyVal, STC_EMPTYVAL_MAX_SIZE);
    }

    char bufName[STC_NAME_MAX_SIZE];
    bufName[0] = 0;
    char bufDescr[STC_DESCR_MAX_SIZE];
    bufDescr[0] = 0;
    HWND comboName, comboDescr;
    if (ti.GetControl(comboName, IDC_COL_NAME) && ti.GetControl(comboDescr, IDC_COL_DESCR))
    {
        if (ti.Type == ttDataToWindow)
        {
            // napridavame std. stringy
            SendMessage(comboName, CB_RESETCONTENT, 0, 0);
            SendMessage(comboDescr, CB_RESETCONTENT, 0, 0);
            int i;
            for (i = 0; i < STC_STD_NAMES_COUNT; i++)
            {
                LoadStdColumnStrName(bufName, STC_NAME_MAX_SIZE, i);
                LoadStdColumnStrDescr(bufDescr, STC_DESCR_MAX_SIZE, i);
                SendMessage(comboName, CB_ADDSTRING, 0, (LPARAM)bufName);
                SendMessage(comboDescr, CB_ADDSTRING, 0, (LPARAM)bufDescr);
            }
            // nastavime limity na texty
            SendMessage(comboName, CB_LIMITTEXT, STC_NAME_MAX_SIZE - 1, 0);
            SendMessage(comboDescr, CB_LIMITTEXT, STC_DESCR_MAX_SIZE - 1, 0);
            // pokud jde o editaci, vlozime do editline i aktualni texty
            if (Edit)
            {
                CSrvTypeColumn* col = ColumnsData->At(*EditedColumn);
                if (col->NameID != -1)
                {
                    if (!LoadStdColumnStrName(bufName, STC_NAME_MAX_SIZE, col->NameID))
                        bufName[0] = 0;
                    LastUsedIndexForName = col->NameID;
                }
                else
                    lstrcpyn(bufName, HandleNULLStr(col->NameStr), STC_NAME_MAX_SIZE);
                if (col->DescrID != -1)
                {
                    if (!LoadStdColumnStrDescr(bufDescr, STC_DESCR_MAX_SIZE, col->DescrID))
                        bufDescr[0] = 0;
                    LastUsedIndexForDescr = col->DescrID;
                }
                else
                    lstrcpyn(bufDescr, HandleNULLStr(col->DescrStr), STC_DESCR_MAX_SIZE);
                SetWindowText(comboName, bufName);
                SetWindowText(comboDescr, bufDescr);
            }
        }
        else
        {
            // pokud ma user vlastni text, vytahneme ho, jinak pouzijeme indexy z posledniho vyberu
            // v combu (zjisteni indexu dohledanim stringu v seznamu neni mozne, hrozi duplitita stringu)
            if (LastUsedIndexForName == -1)
                GetWindowText(comboName, bufName, STC_NAME_MAX_SIZE);
            if (LastUsedIndexForDescr == -1)
                GetWindowText(comboDescr, bufDescr, STC_DESCR_MAX_SIZE);
        }
    }

    HWND combo;
    CSrvTypeColumnTypes colType = stctNone;
    if (ti.GetControl(combo, IDC_COL_TYPE))
    {
        if (ti.Type == ttDataToWindow)
        {
            char buf[100];
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            int count = 0;
            int focus = 0;
            int i;
            for (i = stctName; i < stctLastItem; i++)
            {
                BOOL add = TRUE;
                if (i == stctExt && Edit && *EditedColumn != 1)
                    add = FALSE; // pripona muze byt jen druhy sloupec
                if (add && i < stctFirstGeneral)
                {
                    int j;
                    for (j = 0; j < ColumnsData->Count; j++)
                    {
                        if ((!Edit || *EditedColumn != j) && // pri editaci pridame prirozene i nas soucasny typ
                            i == (int)ColumnsData->At(j)->Type)
                        {
                            add = FALSE;
                            break;
                        }
                    }
                }
                if (add && GetColumnTypeName(buf, 100, (CSrvTypeColumnTypes)i))
                {
                    SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)buf); // pridame string a
                    SendMessage(combo, CB_SETITEMDATA, count++, i);   // k nemu o jaky typ sloupce jde
                    if (Edit && (int)(ColumnsData->At(*EditedColumn)->Type) == i)
                    {
                        focus = count - 1; // pri editace focusime nas typ
                        if (i == stctName)
                            break; // typ prvniho sloupce se zmenit neda
                    }
                }
            }
            SendMessage(combo, CB_SETCURSEL, focus, 0);
            PostMessage(HWindow, WM_COMMAND, MAKELONG(IDC_COL_TYPE, CBN_SELCHANGE), 0);
            FirstSelNotifyAfterTransfer = TRUE;
        }
        else
        {
            int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR)
            {
                // x64 - ITEMDATA nedrzi ukazatel, pretypovani na (int) je bezpecne
                int type = (int)SendMessage(combo, CB_GETITEMDATA, i, 0); // ziskame vybrany typ sloupce
                if (type > stctNone && type < stctLastItem)
                    colType = (CSrvTypeColumnTypes)type;
                else
                    TRACE_E("Unexpected situation in CEditSrvTypeColumnDlg::Transfer(): unknown type of column!");
            }
        }
    }

    BOOL leftAlignment = TRUE;
    if (ti.GetControl(combo, IDC_COL_ALIGNMENT))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SRVTYPECOL_ALIGNLEFT));  // pridame string "left"
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SRVTYPECOL_ALIGNRIGHT)); // pridame string "right"
            SendMessage(combo, CB_SETCURSEL, ColumnsData->At(*EditedColumn)->Type >= stctFirstGeneral ? (ColumnsData->At(*EditedColumn)->LeftAlignment ? 0 : 1) : -1, 0);
        }
        else
        {
            int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR)
                leftAlignment = i == 0;
        }
    }

    // zpracujeme ziskana data (id, emptyVal, LastUsedIndexForName, bufName, LastUsedIndexForDescr,
    // bufDescr, colType, leftAlignment)
    if (ti.Type == ttDataFromWindow)
    {
        // u typu Name, Ext a Type nemaji prazdne hodnoty smysl, vycistime je
        if (colType == stctName || colType == stctExt || colType == stctType)
            emptyVal[0] = 0;

        if (Edit) // editace sloupce
        {
            CSrvTypeColumn* col = ColumnsData->At(*EditedColumn);
            UpdateStr(col->ID, id); // pri chybe zustaneme u puvodniho ID
            if (LastUsedIndexForName == -1)
            {
                BOOL err = FALSE;
                UpdateStr(col->NameStr, bufName, &err);
                if (!err)
                    col->NameID = -1; // pri chybe nechame puvodni jmeno sloupce
            }
            else
            {
                if (col->NameStr != NULL)
                {
                    free(col->NameStr);
                    col->NameStr = NULL;
                }
                col->NameID = LastUsedIndexForName;
            }
            if (LastUsedIndexForDescr == -1)
            {
                BOOL err = FALSE;
                UpdateStr(col->DescrStr, bufDescr, &err);
                if (!err)
                    col->DescrID = -1; // pri chybe nechame puvodni popis sloupce
            }
            else
            {
                if (col->DescrStr != NULL)
                {
                    free(col->DescrStr);
                    col->DescrStr = NULL;
                }
                col->DescrID = LastUsedIndexForDescr;
            }
            // pri editaci na stctExt musi dojit k Visible = TRUE
            if (colType == stctExt)
                col->Visible = TRUE;
            col->Type = colType;
            col->LeftAlignment = leftAlignment;
            BOOL err = FALSE;
            UpdateStr(col->EmptyValue, emptyVal[0] == 0 ? NULL : emptyVal, &err);
            if (err && col->EmptyValue != NULL)
            {
                free(col->EmptyValue);
                col->EmptyValue = NULL;
            }
        }
        else // novy sloupec
        {
            CSrvTypeColumn* col = new CSrvTypeColumn;
            if (col != NULL && col->IsGood())
            {
                col->Set(TRUE, id, LastUsedIndexForName, bufName[0] == 0 ? NULL : bufName,
                         LastUsedIndexForDescr, bufDescr[0] == 0 ? NULL : bufDescr, colType,
                         emptyVal[0] == 0 ? NULL : emptyVal, leftAlignment, 0, 0);
                // pridani typu stctExt musi vest ke vlozeni za "Name" (tedy na index 1 v poli)
                if (colType == stctExt)
                {
                    ColumnsData->Insert(1, col);
                    *EditedColumn = 1; // fokus vlozeneho sloupce
                }
                else
                {
                    ColumnsData->Add(col);
                    *EditedColumn = ColumnsData->Count - 1; // fokus pridaneho sloupce
                }
                if (ColumnsData->IsGood())
                    col = NULL;
                else
                    ColumnsData->ResetState();
            }
            if (col != NULL)
                delete col;
        }
    }
}

INT_PTR
CEditSrvTypeColumnDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CEditSrvTypeColumnDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (!Edit)
            SetWindowText(HWindow, LoadStr(IDS_SRVTYPECOL_NEWTITLE));
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_COL_NAME:
        case IDC_COL_DESCR:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE) // ulozime posledni zvoleny index z comboboxu
            {
                HWND combo = GetDlgItem(HWindow, LOWORD(wParam));
                int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                if (i != CB_ERR)
                {
                    if (LOWORD(wParam) == IDC_COL_NAME)
                        LastUsedIndexForName = i;
                    else
                        LastUsedIndexForDescr = i;
                }
            }
            if (HIWORD(wParam) == CBN_EDITCHANGE) // zneplatnime posledni zvoleny index z comboboxu
            {
                if (LOWORD(wParam) == IDC_COL_NAME)
                    LastUsedIndexForName = -1;
                else
                    LastUsedIndexForDescr = -1;
            }
            break;
        }

        case IDC_COL_TYPE:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE) // nastavime napovedu pro format "Empty Value" + enablujeme a nastavime combo Alignment
            {
                HWND combo = GetDlgItem(HWindow, IDC_COL_TYPE);
                int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                if (i != CB_ERR)
                {
                    // x64 - ITEMDATA nedrzi ukazatel, pretypovani na (int) je bezpecne
                    int type = (int)SendMessage(combo, CB_GETITEMDATA, i, 0); // ziskame vybrany typ sloupce
                    if (type > stctNone && type < stctLastItem)
                    {
                        BOOL leftAlignment = TRUE;
                        int resID = IDS_SRVTYPECOL_FORMSTR;
                        switch (type)
                        {
                        case stctSize:
                        case stctGeneralNumber:
                            resID = IDS_SRVTYPECOL_FORMNUM;
                            leftAlignment = FALSE;
                            break;

                        case stctDate:
                        case stctGeneralDate:
                            resID = IDS_SRVTYPECOL_FORMDATE;
                            leftAlignment = FALSE;
                            break;

                        case stctTime:
                        case stctGeneralTime:
                            resID = IDS_SRVTYPECOL_FORMTIME;
                            leftAlignment = FALSE;
                            break;
                        }
                        SetDlgItemText(HWindow, IDT_COL_FORMHELP, LoadStr(resID));

                        // enablujeme editline Empty Value
                        BOOL enable = (type != stctName && type != stctExt && type != stctType);
                        HWND focus = GetFocus();
                        if (!enable && focus == GetDlgItem(HWindow, IDE_COL_EMPTY))
                            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDC_COL_TYPE), TRUE);
                        EnableWindow(GetDlgItem(HWindow, IDE_COL_EMPTY), enable);

                        // enablujeme a nastavujeme combo Alignment
                        enable = type >= stctFirstGeneral;
                        if (!FirstSelNotifyAfterTransfer)
                        {
                            SendMessage(GetDlgItem(HWindow, IDC_COL_ALIGNMENT), CB_SETCURSEL,
                                        enable ? (leftAlignment ? 0 : 1) : -1, 0);
                        }
                        focus = GetFocus();
                        if (!enable && focus == GetDlgItem(HWindow, IDC_COL_ALIGNMENT))
                            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDC_COL_TYPE), TRUE);
                        EnableWindow(GetDlgItem(HWindow, IDC_COL_ALIGNMENT), enable);
                    }
                }
                FirstSelNotifyAfterTransfer = FALSE;
            }
            break;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSrvTypeTestParserDlg
//

CSrvTypeTestParserDlg::CSrvTypeTestParserDlg(HWND parent, CFTPParser* parser,
                                             TIndirectArray<CSrvTypeColumn>* columns,
                                             char** rawListing, BOOL* rawListIncomplete)
    : CCenteredDialog(HLanguage, IDD_SRVTYPETESTPARSER, IDD_SRVTYPETESTPARSER, parent), Offsets(500, 500)
{
    HListView = NULL;
    Parser = parser;
    Columns = columns;
    RawListing = rawListing;
    RawListIncomplete = rawListIncomplete;
    AllocatedSizeOfRawListing = *RawListing == NULL ? 0 : (int)strlen(*RawListing) + 1;
    LastSelectedOffset = -1;

    SymbolsImageList = ImageList_Create(16, 16, ILC_MASK | SalamanderGeneral->GetImageListColorFlags(), 3, 0);
    if (SymbolsImageList != NULL)
    {
        ImageList_SetImageCount(SymbolsImageList, 3); // inicializace

        HINSTANCE iconsDLL;
        if (WindowsVistaAndLater)
            iconsDLL = HANDLES(LoadLibraryEx("imageres.dll", NULL, LOAD_LIBRARY_AS_DATAFILE));
        else
        {
            const char* Shell32DLLName = "shell32.dll";
            iconsDLL = HANDLES(LoadLibraryEx(Shell32DLLName, NULL, LOAD_LIBRARY_AS_DATAFILE));
        }

        BOOL err = FALSE;
        if (iconsDLL != NULL)
        {
            HICON shortcutOverlay = (HICON)HANDLES(LoadImage(iconsDLL, MAKEINTRESOURCE(WindowsVistaAndLater ? 163 : 30),
                                                             IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags()));
            if (shortcutOverlay != NULL)
            {
                ImageList_ReplaceIcon(SymbolsImageList, 2, shortcutOverlay);
                ImageList_SetOverlayImage(SymbolsImageList, 2, 1);
                HANDLES(DestroyIcon(shortcutOverlay));
                int i;
                for (i = 0; i < 2; i++)
                {
                    int resID = (i == 0 ? 4 /* directory */ : 1 /* non-assoc. file */);
                    int vistaResID = (i == 0 ? 4 /* directory */ : 2 /* non-assoc. file */);
                    HICON hIcon = (HICON)HANDLES(LoadImage(iconsDLL, MAKEINTRESOURCE(WindowsVistaAndLater ? vistaResID : resID),
                                                           IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags()));
                    if (hIcon != NULL)
                    {
                        ImageList_ReplaceIcon(SymbolsImageList, i, hIcon);
                        HANDLES(DestroyIcon(hIcon));
                    }
                    else
                        err = TRUE;
                }
            }
            else
                err = TRUE;
            HANDLES(FreeLibrary(iconsDLL));
        }
        else
            err = TRUE;
        if (err)
        {
            ImageList_Destroy(SymbolsImageList);
            SymbolsImageList = NULL;
        }
    }

    MinDlgHeight = 0;
    MinDlgWidth = 0;
    ListingHeight = 0;
    ListingSpacing = 0;
    ButtonsY = 0;
    ParseBorderX = 0;
    ReadBorderX = 0;
    CloseBorderX = 0;
    HelpBorderX = 0;
    CloseBorderY = 0;
    ResultsSpacingX = 0;
    ResultsSpacingY = 0;
    SizeBoxWidth = 0;
    SizeBoxHeight = 0;

    SizeBox = NULL;
}

CSrvTypeTestParserDlg::~CSrvTypeTestParserDlg()
{
    if (SymbolsImageList != NULL)
        ImageList_Destroy(SymbolsImageList);
}

void CSrvTypeTestParserDlg::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_PARSER_LISTINCOMPL, *RawListIncomplete);
    if (ti.Type == ttDataToWindow)
    {
        SendDlgItemMessage(HWindow, IDE_PARSER_RAWLIST, EM_LIMITTEXT, 0, 0);
        if (*RawListing != NULL)
            SetDlgItemText(HWindow, IDE_PARSER_RAWLIST, *RawListing);
    }
    else
    {
        HWND edit = GetDlgItem(HWindow, IDE_PARSER_RAWLIST);
        int len = GetWindowTextLength(edit);
        if (len >= 0)
        {
            if (AllocatedSizeOfRawListing != len + 1) // realokujeme buffer na presnou velikost textu
            {
                AllocatedSizeOfRawListing = len + 1;
                char* n = (char*)realloc(*RawListing, AllocatedSizeOfRawListing);
                if (n == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    AllocatedSizeOfRawListing = 0;
                    if (*RawListing != NULL)
                        free(*RawListing);
                }
                *RawListing = n;
            }
            if (*RawListing != NULL)
                GetWindowText(edit, *RawListing, len + 1);
        }
        else
        {
            if (*RawListing != NULL)
            {
                free(*RawListing);
                *RawListing = NULL;
            }
        }

        // uvolnime image-list z list-view, chceme image-list uvolnit sami
        ListView_SetImageList(HListView, NULL, LVSIL_SMALL);
        if (SymbolsImageList != NULL)
        {
            ImageList_Destroy(SymbolsImageList);
            SymbolsImageList = NULL; // simulujeme stav, kdy se imagelist nepodarilo vytvorit
        }
        ListView_DeleteAllItems(HListView); // promazeme listview dokud je dialog videt - celkove lepsi chovani
    }
}

void CSrvTypeTestParserDlg::InitColumns()
{
    LV_COLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < Columns->Count; i++) // vytvorim sloupce
    {
        CSrvTypeColumn* col = Columns->At(i);
        char bufName[STC_NAME_MAX_SIZE];
        if (col->NameID != -1)
        {
            LoadStdColumnStrName(bufName, STC_NAME_MAX_SIZE, col->NameID);
            lvc.pszText = bufName;
        }
        else
            lvc.pszText = HandleNULLStr(col->NameStr);
        lvc.iSubItem = i;
        ListView_InsertColumn(HListView, i, &lvc);
        //    ListView_SetColumnWidth(HListView, i, LVSCW_AUTOSIZE_USEHEADER); // sirky nastavime az v SetColumnWidths()
    }
    if (SymbolsImageList != NULL)
        ListView_SetImageList(HListView, SymbolsImageList, LVSIL_SMALL);
}

void CSrvTypeTestParserDlg::SetColumnWidths()
{
    if (Columns->Count == 0)
        return;

    int i;
    for (i = 0; i < Columns->Count; i++)
        ListView_SetColumnWidth(HListView, i, LVSCW_AUTOSIZE_USEHEADER);
}

void CSrvTypeTestParserDlg::ParseListingToListView()
{
    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    // LockWindowUpdate(HListView);  // nepouzivat - blika cely Windows
    //  SendMessage(HListView, WM_SETREDRAW, FALSE, 0);
    // pri volani SetColumnWidths() dochazi pru pouziti samotneho WM_SETREDRAW ke
    // smazani obsahu listview a tedy zbytecnemu blikani. Proto okno po dobu plneni zhasneme.
    SetWindowPos(HListView, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER);

    int selIndex = 0;
    int topIndex = 0;
    if (ListView_GetItemCount(HListView) > 0)
    {
        topIndex = ListView_GetTopIndex(HListView);
        selIndex = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
    }

    ListView_DeleteAllItems(HListView);
    Offsets.DestroyMembers();
    LastSelectedOffset = -1;

    char strOnlyInPanel[100];
    lstrcpyn(strOnlyInPanel, LoadStr(IDS_SRVTYPE_ONLYINPANEL), 100);
    char strDIR[100];
    lstrcpyn(strDIR, LoadStr(IDS_SRVTYPE_SIZEISDIR), 100);

    CFileData file;
    CFTPListingPluginDataInterface dataIface(Columns, FALSE, 0 /* nepouziva se zde */, FALSE /* nepouziva se zde */);
    char buf[100];
    CQuadWord qwVal(0, 0);
    __int64 int64Val = 0;
    SYSTEMTIME stDateVal;
    GetLocalTime(&stDateVal);         // inicializujeme na nejake platne hodnoty
    SYSTEMTIME stTimeVal = stDateVal; // inicializujeme na nejake platne hodnoty
    SYSTEMTIME st;                    // pomocne
    FILETIME ft;                      // pomocne
    const char* listingStart = HandleNULLStr(*RawListing);
    const char* listing = listingStart;
    const char* listingEnd = listing + strlen(listing);
    if (*RawListIncomplete) // zkratime listing tak, aby obsahoval jen cele radky
    {
        const char* s = listingEnd;
        while (s > listingStart && *(s - 1) != '\r' && *(s - 1) != '\n')
            s--;
        listingEnd = s;
    }
    BOOL lowMem = FALSE;
    DWORD* emptyCol = new DWORD[Columns->Count]; // pomocne predalokovane pole pro GetNextItemFromListing
    if (dataIface.IsGood() && emptyCol != NULL)
    {
        const char* itemStart = NULL;
        BOOL isDir = FALSE;
        int i = 0;
        Parser->BeforeParsing(listingStart, listingEnd, stDateVal.wYear, stDateVal.wMonth,
                              stDateVal.wDay, *RawListIncomplete); // init parseru
        while (Parser->GetNextItemFromListing(&file, &isDir, &dataIface, Columns, &listing,
                                              listingEnd, &itemStart, &lowMem, emptyCol))
        {
            Offsets.Add((DWORD)(itemStart - listingStart));
            Offsets.Add((DWORD)(listing - listingStart));

            // prvni sloupec je vzdy Name
            LVITEM lvi;
            lvi.mask = LVIF_STATE | LVIF_TEXT | (SymbolsImageList != NULL ? LVIF_IMAGE : 0);
            lvi.iImage = (isDir ? 0 : 1);
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.state = (file.Hidden ? LVIS_CUT : 0) | (file.IsLink ? INDEXTOOVERLAYMASK(1) : 0);
            lvi.pszText = file.Name;
            ListView_InsertItem(HListView, &lvi);

            // vlozime data pro dalsi sloupce
            int j;
            for (j = 1; j < Columns->Count; j++)
            {
                static char emptyBuff[] = "";
                char* value = emptyBuff;
                CSrvTypeColumn* col = Columns->At(j);
                switch (col->Type)
                {
                // case stctName:   // Name muze byt jen v prvnim sloupci
                case stctGeneralText:
                    value = HandleNULLStr(dataIface.GetStringFromColumn(file, j));
                    break;

                case stctExt:
                case stctType:
                    value = strOnlyInPanel;
                    break;

                case stctSize:
                {
                    if (!isDir)
                    {
                        SalamanderGeneral->NumberToStr(buf, file.Size);
                        value = buf;
                    }
                    else
                        value = strDIR;
                    break;
                }

                case stctGeneralNumber:
                {
                    int64Val = dataIface.GetNumberFromColumn(file, j);
                    if (int64Val >= 0)
                    {
                        SalamanderGeneral->NumberToStr(buf, qwVal.SetUI64((unsigned __int64)int64Val));
                        value = buf;
                    }
                    else
                    {
                        if (int64Val != INT64_EMPTYNUMBER) // nema se zobrazit ""
                        {
                            buf[0] = '-';
                            SalamanderGeneral->NumberToStr(buf + 1, qwVal.SetUI64((unsigned __int64)(-int64Val)));
                            value = buf;
                        }
                    }
                    break;
                }

                case stctDate:
                {
                    FileTimeToLocalFileTime(&file.LastWrite, &ft);
                    FileTimeToSystemTime(&ft, &st);
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buf, 100) == 0)
                        sprintf(buf, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                    value = buf;
                    break;
                }

                case stctGeneralDate:
                {
                    dataIface.GetDateFromColumn(file, j, &stDateVal);
                    if (stDateVal.wDay != 0) // nema se zobrazit ""
                    {
                        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &stDateVal, NULL, buf, 100) == 0)
                            sprintf(buf, "%u.%u.%u", stDateVal.wDay, stDateVal.wMonth, stDateVal.wYear);
                        value = buf;
                    }
                    break;
                }

                case stctTime:
                {
                    FileTimeToLocalFileTime(&file.LastWrite, &ft);
                    FileTimeToSystemTime(&ft, &st);
                    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buf, 100) == 0)
                        sprintf(buf, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
                    value = buf;
                    break;
                }

                case stctGeneralTime:
                {
                    dataIface.GetTimeFromColumn(file, j, &stTimeVal);
                    if (stTimeVal.wHour != 24) // nema se zobrazit ""
                    {
                        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &stTimeVal, NULL, buf, 100) == 0)
                            sprintf(buf, "%u:%02u:%02u", stTimeVal.wHour, stTimeVal.wMinute, stTimeVal.wSecond);
                        value = buf;
                    }
                    break;
                }
                }
                ListView_SetItemText(HListView, i, j, value);
            }
            i++;
            // uvolnime data souboru nebo adresare
            dataIface.ReleasePluginData(file, isDir);
            free(file.Name);
        }
    }
    else
    {
        if (emptyCol == NULL)
            TRACE_E(LOW_MEMORY);
        lowMem = TRUE;
    }
    if (emptyCol != NULL)
        delete[] emptyCol;

    int count = ListView_GetItemCount(HListView);
    if (count > 0)
    {
        if (topIndex >= count)
            topIndex = count - 1;
        if (topIndex < 0)
            topIndex = 0;
        if (selIndex >= count)
            selIndex = count - 1;
        if (selIndex < 0)
            selIndex = 0;
        // nahrazka za SetTopIndex u list-view
        ListView_EnsureVisible(HListView, count - 1, FALSE);
        ListView_EnsureVisible(HListView, topIndex, FALSE);
        // normalni focus
        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItemState(HListView, selIndex, state, state);
        ListView_EnsureVisible(HListView, selIndex, FALSE);
    }
    SetColumnWidths();

    //  LockWindowUpdate(NULL);   // nepouzivat - pri delsim parsovani blika cely Windows
    //  SendMessage(HListView, WM_SETREDRAW, TRUE, 0);
    SetWindowPos(HListView, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW /*| SWP_NOREDRAW */ | SWP_NOSENDCHANGING | SWP_NOZORDER);

    SetCursor(oldCur);

    // pokud nedoslo k uspesnemu rozparsovani celeho listingu, ohlasime chybu
    if (!lowMem && listing < listingEnd) // nedostatek pameti nehlasime (fatal error)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_SRVTYPE_PARSEERROR),
                                         LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        // oznacime misto chyby v textu pravidel pro parsovani
        DWORD errorPos = (DWORD)(listing - listingStart);
        SendDlgItemMessage(HWindow, IDE_PARSER_RAWLIST, EM_SETSEL, (WPARAM)errorPos,
                           (LPARAM)errorPos);
        SendDlgItemMessage(HWindow, IDE_PARSER_RAWLIST, EM_SCROLLCARET, 0, 0); // scroll caret into view
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_PARSER_RAWLIST), TRUE);
    }
}

void CSrvTypeTestParserDlg::LoadTextFromFile()
{
    static char initDir[MAX_PATH] = "";
    if (initDir[0] == 0)
        GetMyDocumentsPath(initDir);
    char fileName[MAX_PATH];
    fileName[0] = 0;
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = HWindow;
    char* s = LoadStr(IDS_RAWLISTINGFILTER);
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = initDir;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    char buf[300 + MAX_PATH];
    if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
    {
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

        s = strrchr(fileName, '\\');
        if (s != NULL)
        {
            memcpy(initDir, fileName, s - fileName);
            initDir[s - fileName] = 0;
        }

        HANDLE file = HANDLES_Q(CreateFile(fileName, GENERIC_READ,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                           OPEN_EXISTING,
                                           FILE_FLAG_SEQUENTIAL_SCAN,
                                           NULL));
        CQuadWord size;
        DWORD err = NO_ERROR;
        if (file != INVALID_HANDLE_VALUE &&
            SalamanderGeneral->SalGetFileSize(file, size, err))
        {
            int len;
            if (size > CQuadWord(1024 * 1024, 0))
            {
                SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_SRVTYPE_READRAWLISTLIMIT),
                                                 LoadStr(IDS_FTPPLUGINTITLE),
                                                 MB_OK | MB_ICONWARNING);
                len = 1024 * 1024;
            }
            else
                len = (DWORD)size.Value;

            if (AllocatedSizeOfRawListing < len + 1)
            {
                AllocatedSizeOfRawListing = len + 1;
                char* n = (char*)realloc(*RawListing, AllocatedSizeOfRawListing);
                if (n == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    AllocatedSizeOfRawListing = 0;
                    if (*RawListing != NULL)
                        free(*RawListing);
                }
                *RawListing = n;
            }
            if (*RawListing != NULL)
            {
                DWORD read;
                if (ReadFile(file, *RawListing, len, &read, NULL) && read == (DWORD)len)
                {
                    (*RawListing)[len] = 0;
                    SetDlgItemText(HWindow, IDE_PARSER_RAWLIST, *RawListing);
                }
                else
                {
                    err = GetLastError();
                    (*RawListing)[0] = 0;
                }
            }

            HANDLES(CloseHandle(file));
            SetCursor(oldCur);
            if (err != NO_ERROR) // vypis chyby
            {
                sprintf(buf, LoadStr(IDS_SRVTYPE_READRAWLISTERR), SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
        }
        else
        {
            if (err == NO_ERROR)
                err = GetLastError();
            if (file != INVALID_HANDLE_VALUE)
                HANDLES(CloseHandle(file));
            SetCursor(oldCur);
            sprintf(buf, LoadStr(IDS_SRVTYPE_READRAWLISTERR), SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }
    }
}

void CSrvTypeTestParserDlg::OnWMSize(int width, int height, BOOL notInitDlg, WPARAM wParam)
{
    HWND listing = GetDlgItem(HWindow, IDE_PARSER_RAWLIST);
    HWND buttonParse = GetDlgItem(HWindow, IDB_PARSER_PARSELIST);
    HWND buttonRead = GetDlgItem(HWindow, IDB_PARSER_LOADLIST);
    HWND buttonClose = GetDlgItem(HWindow, IDOK);
    HWND buttonHelp = GetDlgItem(HWindow, IDHELP);
    HWND results = GetDlgItem(HWindow, IDL_PARSER_COLUMNS);

    HDWP hdwp = HANDLES(BeginDeferWindowPos(8));
    if (hdwp != NULL)
    {
        hdwp = HANDLES(DeferWindowPos(hdwp, listing, NULL, 0, 0, width - ListingSpacing, ListingHeight,
                                      SWP_NOZORDER | SWP_NOMOVE));

        hdwp = HANDLES(DeferWindowPos(hdwp, buttonParse, NULL, width - ParseBorderX, ButtonsY, 0, 0,
                                      SWP_NOZORDER | SWP_NOSIZE));

        hdwp = HANDLES(DeferWindowPos(hdwp, buttonRead, NULL, width - ReadBorderX, ButtonsY, 0, 0,
                                      SWP_NOZORDER | SWP_NOSIZE));

        hdwp = HANDLES(DeferWindowPos(hdwp, results, NULL, 0, 0, width - ResultsSpacingX, height - ResultsSpacingY,
                                      SWP_NOZORDER | SWP_NOMOVE));

        hdwp = HANDLES(DeferWindowPos(hdwp, buttonClose, NULL, width - CloseBorderX, height - CloseBorderY, 0, 0,
                                      SWP_NOZORDER | SWP_NOSIZE));

        hdwp = HANDLES(DeferWindowPos(hdwp, buttonHelp, NULL, width - HelpBorderX, height - CloseBorderY, 0, 0,
                                      SWP_NOZORDER | SWP_NOSIZE));

        if (SizeBox != NULL)
        {
            hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, width - SizeBoxWidth,
                                          height - SizeBoxHeight, SizeBoxWidth, SizeBoxHeight, SWP_NOZORDER));
            if (notInitDlg)
            {
                // pry nejde show/hide kombinovat se zmenou velikosti a posunem
                hdwp = HANDLES(DeferWindowPos(hdwp, SizeBox, NULL, 0, 0, 0, 0,
                                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | (wParam == SIZE_RESTORED ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)));
            }
        }

        HANDLES(EndDeferWindowPos(hdwp));
    }
}

INT_PTR
CSrvTypeTestParserDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSrvTypeTestParserDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // zakazeme select-all pri fokusu + nastavime fixed-font pro edit s listingem
        CSimpleDlgControlWindow* wnd = new CSimpleDlgControlWindow(HWindow, IDE_PARSER_RAWLIST, FALSE);
        if (wnd != NULL && wnd->HWindow == NULL)
            delete wnd; // nepodarilo se attachnout - nedealokuje se samo
        if (FixedFont != NULL)
            SendDlgItemMessage(HWindow, IDE_PARSER_RAWLIST, WM_SETFONT, (WPARAM)FixedFont, TRUE);

        RECT r1, r2, r3, r4, r5, r6;
        GetWindowRect(HWindow, &r1);
        GetClientRect(GetDlgItem(HWindow, IDE_PARSER_RAWLIST), &r2);
        GetClientRect(GetDlgItem(HWindow, IDL_PARSER_COLUMNS), &r3);
        MinDlgHeight = r1.bottom - r1.top - r3.bottom + 50; // edit nechame zmensit na 50 bodu client area "results of parsing"
        GetWindowRect(GetDlgItem(HWindow, IDC_PARSER_LISTINCOMPL), &r4);
        GetWindowRect(GetDlgItem(HWindow, IDB_PARSER_PARSELIST), &r5);
        GetWindowRect(GetDlgItem(HWindow, IDB_PARSER_LOADLIST), &r6);
        MinDlgWidth = r1.right - r1.left - r2.right + r4.right - r4.left + r6.right - r5.left; // do client area editu nechame vejit aspon checkbox a dva butony vcetne mezery mezi nimi

        RECT r7, r8, r9, r10;
        GetWindowRect(GetDlgItem(HWindow, IDE_PARSER_RAWLIST), &r7);
        ListingHeight = r7.bottom - r7.top;
        GetClientRect(HWindow, &r8);
        ListingSpacing = r8.right - (r7.right - r7.left);
        POINT p;
        p.x = r5.left;
        p.y = r5.top;
        ScreenToClient(HWindow, &p);
        ParseBorderX = r8.right - p.x;
        ButtonsY = p.y;
        p.x = r6.left;
        p.y = r6.top;
        ScreenToClient(HWindow, &p);
        ReadBorderX = r8.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDOK), &r9);
        p.x = r9.left;
        p.y = r9.top;
        ScreenToClient(HWindow, &p);
        CloseBorderX = r8.right - p.x;
        CloseBorderY = r8.bottom - p.y;
        GetWindowRect(GetDlgItem(HWindow, IDHELP), &r9);
        p.x = r9.left;
        p.y = r9.top;
        ScreenToClient(HWindow, &p);
        HelpBorderX = r8.right - p.x;
        GetWindowRect(GetDlgItem(HWindow, IDL_PARSER_COLUMNS), &r10);
        ResultsSpacingX = r8.right - (r10.right - r10.left);
        ResultsSpacingY = r8.bottom - (r10.bottom - r10.top);

        // do praveho spodniho ruzku vlozime znacek pro resize okna
        SizeBox = CreateWindowEx(0,
                                 "scrollbar",
                                 "",
                                 WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE |
                                     WS_GROUP | SBS_SIZEBOX | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN,
                                 0, 0, r8.right, r8.bottom,
                                 HWindow,
                                 NULL,
                                 DLLInstance,
                                 NULL);
        RECT r11;
        GetClientRect(SizeBox, &r11);
        SizeBoxWidth = r11.right;
        SizeBoxHeight = r11.bottom;

        // nastavime pozadovane pocatecni rozmery dialogu
        if (Config.TestParserDlgWidth > 0 && Config.TestParserDlgHeight > 0)
            MoveWindow(HWindow, r1.left, r1.top, Config.TestParserDlgWidth, Config.TestParserDlgHeight, TRUE);

        // provedeme layoutovani dialogu
        RECT clientRect;
        GetClientRect(HWindow, &clientRect);
        OnWMSize(clientRect.right, clientRect.bottom, FALSE, 0);

        // provedeme transfer dat do okna
        INT_PTR ret = CCenteredDialog::DialogProc(uMsg, wParam, lParam);

        // listview setup
        HListView = GetDlgItem(HWindow, IDL_PARSER_COLUMNS);
        DWORD exFlags = LVS_EX_FULLROWSELECT;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        // vlozime sloupce
        InitColumns();

        // nastavime sirky sloupcu
        SetColumnWidths();

        // hned po otevreni dialogu spustime parsovani listingu
        PostMessage(HWindow, WM_COMMAND, IDB_PARSER_PARSELIST, 0);

        if (Config.TestParserDlgWidth == -2) // show maximized
        {
            ShowWindow(HWindow, SW_MAXIMIZE);
            UpdateWindow(HWindow);
        }

        return ret;
    }

    case WM_SIZE:
    {
        RECT clientRect;
        GetClientRect(HWindow, &clientRect);
        OnWMSize(clientRect.right, clientRect.bottom, TRUE, wParam);
        break;
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinDlgWidth;
        lpmmi->ptMinTrackSize.y = MinDlgHeight;
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            wParam = IDOK;
            break; // v tomto dialogu Cancel = OK (vzdy je treba transferit data)

        case IDE_PARSER_RAWLIST:
        {
            if (HIWORD(wParam) == EN_CHANGE)
                Offsets.DestroyMembers(); // zmena textu -> offsety prestavaji byt platne
            break;
        }

        case IDB_PARSER_PARSELIST:
        {
            *RawListIncomplete = IsDlgButtonChecked(HWindow, IDC_PARSER_LISTINCOMPL) == BST_CHECKED;
            HWND edit = GetDlgItem(HWindow, IDE_PARSER_RAWLIST);
            int len = GetWindowTextLength(edit);
            if (len >= 0)
            {
                if (AllocatedSizeOfRawListing < len + 1)
                {
                    AllocatedSizeOfRawListing = len + 50;
                    char* n = (char*)realloc(*RawListing, AllocatedSizeOfRawListing); // udelame trochu rezervu
                    if (n == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        AllocatedSizeOfRawListing = 0;
                        if (*RawListing != NULL)
                            free(*RawListing);
                    }
                    *RawListing = n;
                }
                if (*RawListing != NULL)
                {
                    // nacteme novy text listingu z editboxu
                    GetWindowText(edit, *RawListing, len + 1);
                    // spustime parsovani, vysledky primo do listview
                    ParseListingToListView();
                }
            }
            return TRUE; // dale nepokracovat
        }

        case IDB_PARSER_LOADLIST:
        {
            LoadTextFromFile();
            return TRUE; // dale nepokracovat
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDL_PARSER_COLUMNS)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            if (nmh->code == LVN_DELETEALLITEMS)
            {
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE); // potlaceni posilani LVN_DELETEITEM pro kazdou polozku
                return TRUE;
            }
            if (nmh->code == LVN_ITEMCHANGED)
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if ((nmhi->uOldState & LVIS_SELECTED) != (nmhi->uNewState & LVIS_SELECTED))
                {
                    int i = 2 * nmhi->iItem;
                    if (LastSelectedOffset != i && i >= 0 && i + 1 < Offsets.Count)
                    {
                        LastSelectedOffset = i;
                        SendDlgItemMessage(HWindow, IDE_PARSER_RAWLIST, EM_SETSEL, Offsets[i + 1], Offsets[i]);
                        SendDlgItemMessage(HWindow, IDE_PARSER_RAWLIST, EM_SCROLLCARET, 0, 0);
                    }
                }
            }
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(HListView, GetSysColor(COLOR_WINDOW));
        break;
    }

    case WM_DESTROY:
    {
        if (IsZoomed(HWindow))
            Config.TestParserDlgWidth = -2;
        else
        {
            if (!IsIconic(HWindow)) // "always false"
            {
                RECT r;
                GetWindowRect(HWindow, &r);
                Config.TestParserDlgWidth = r.right - r.left;
                Config.TestParserDlgHeight = r.bottom - r.top;
            }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCopyMoveDlg
//

CCopyMoveDlg::CCopyMoveDlg(HWND parent, char* path, int pathBufSize, const char* title,
                           const char* subject, char* history[], int historyCount, int helpID)
    : CCenteredDialog(HLanguage, IDD_COPYMOVEDLG, helpID, parent)
{
    Title = title;
    Subject = subject;
    Path = path;
    PathBufSize = pathBufSize;
    History = history;
    HistoryCount = historyCount;
}

void CCopyMoveDlg::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCopyMoveDlg::Transfer()");
    if (History != NULL)
    {
        HWND hWnd;
        if (ti.GetControl(hWnd, IDC_TGTPATH))
        {
            if (ti.Type == ttDataToWindow)
            {
                SalamanderGeneral->LoadComboFromStdHistoryValues(hWnd, History, HistoryCount);
                SendMessage(hWnd, CB_LIMITTEXT, PathBufSize - 1, 0);
                SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Path);
            }
            else
            {
                SendMessage(hWnd, WM_GETTEXT, PathBufSize, (LPARAM)Path);
                SalamanderGeneral->AddValueToStdHistoryValues(History, HistoryCount, Path, FALSE);
            }
        }
    }
    ti.CheckBox(IDC_ADDTOQUEUE, Config.DownloadAddToQueue);
}

INT_PTR
CCopyMoveDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCopyMoveDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGeneral->InstallWordBreakProc(GetDlgItem(HWindow, IDC_TGTPATH)); // instalujeme WordBreakProc do comboboxu
        SetWindowText(HWindow, Title);
        SetDlgItemText(HWindow, IDT_TGTPATHSUBJECT, Subject);
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConfirmDeleteDlg
//

CConfirmDeleteDlg::CConfirmDeleteDlg(HWND parent, const char* subject, HICON icon)
    : CCenteredDialog(HLanguage, IDD_CONFIRMDELETEDLG, parent)
{
    Subject = subject;
    Icon = icon;
}

void CConfirmDeleteDlg::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_DELADDTOQUEUE, Config.DeleteAddToQueue);
}

INT_PTR
CConfirmDeleteDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfirmDeleteDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetDlgItemText(HWindow, IDT_DELSUBJECT, Subject);
        SendDlgItemMessage(HWindow, IDC_DELICON, STM_SETICON,
                           (WPARAM)(Icon == NULL ? HANDLES(LoadIcon(NULL, IDI_QUESTION)) : Icon), 0);
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CChangeAttrsDlg
//

CChangeAttrsDlg::CChangeAttrsDlg(HWND parent, const char* subject, DWORD attr, DWORD attrDiff,
                                 BOOL selDirs)
    : CCenteredDialog(HLanguage, IDD_CHANGEATTRSDLG, IDD_CHANGEATTRSDLG, parent)
{
    Subject = subject;
    Attr = attr;
    AttrDiff = attrDiff;
    SelFiles = TRUE;
    SelDirs = selDirs;
    IncludeSubdirs = FALSE;
    AttrAndMask = 0777;
    AttrOrMask = 0;
    EnableNotification = TRUE;
}

void CChangeAttrsDlg::RefreshNumValue()
{
    UINT userRead = IsDlgButtonChecked(HWindow, IDC_READOWNER);
    UINT groupRead = IsDlgButtonChecked(HWindow, IDC_READGROUP);
    UINT othersRead = IsDlgButtonChecked(HWindow, IDC_READOTHERS);
    UINT userWrite = IsDlgButtonChecked(HWindow, IDC_WRITEOWNER);
    UINT groupWrite = IsDlgButtonChecked(HWindow, IDC_WRITEGROUP);
    UINT othersWrite = IsDlgButtonChecked(HWindow, IDC_WRITEOTHERS);
    UINT userExec = IsDlgButtonChecked(HWindow, IDC_EXECUTEOWNER);
    UINT groupExec = IsDlgButtonChecked(HWindow, IDC_EXECUTEGROUP);
    UINT othersExec = IsDlgButtonChecked(HWindow, IDC_EXECUTEOTHERS);

    char text[4];
    text[3] = 0;
    if (userRead == 2 || userWrite == 2 || userExec == 2)
        text[0] = '-';
    else
        text[0] = '0' + (userRead << 2) + (userWrite << 1) + userExec;
    if (groupRead == 2 || groupWrite == 2 || groupExec == 2)
        text[1] = '-';
    else
        text[1] = '0' + (groupRead << 2) + (groupWrite << 1) + groupExec;
    if (othersRead == 2 || othersWrite == 2 || othersExec == 2)
        text[2] = '-';
    else
        text[2] = '0' + (othersRead << 2) + (othersWrite << 1) + othersExec;
    EnableNotification = FALSE;
    HWND edit = GetDlgItem(HWindow, IDE_NUMATTRVALUE);
    DWORD start = 0;
    DWORD end = 0;
    SendMessage(edit, EM_GETSEL, (WPARAM)(&start), (LPARAM)(&end));
    SetWindowText(edit, text);
    SendMessage(edit, EM_SETSEL, start, end);
    EnableNotification = TRUE;
}

void CChangeAttrsDlg::Validate(CTransferInfo& ti)
{
    if (IsDlgButtonChecked(HWindow, IDC_READOWNER) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_READGROUP) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_READOTHERS) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_WRITEOWNER) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_WRITEGROUP) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_WRITEOTHERS) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_EXECUTEOWNER) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_EXECUTEGROUP) == 2 &&
        IsDlgButtonChecked(HWindow, IDC_EXECUTEOTHERS) == 2)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CHATTRNOTHINGTODO2), LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_READOWNER);
        return;
    }

    BOOL files, dirs;
    ti.CheckBox(IDC_CHATTRSETFILES, files);
    ti.CheckBox(IDC_CHATTRSETDIRS, dirs);
    if (!files && !dirs)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CHATTRNOTHINGTODO), LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_CHATTRSETFILES);
        return;
    }
    char text[5];
    if (GetDlgItemText(HWindow, IDE_NUMATTRVALUE, text, 5) != 3)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CHATTRNUMVAL3DIGITS), LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_NUMATTRVALUE);
        return;
    }
}

void CChangeAttrsDlg::Transfer(CTransferInfo& ti)
{
    int userRead = (AttrDiff & 0400) ? 2 : ((Attr & 0400) != 0);
    int groupRead = (AttrDiff & 0040) ? 2 : ((Attr & 0040) != 0);
    int othersRead = (AttrDiff & 0004) ? 2 : ((Attr & 0004) != 0);
    int userWrite = (AttrDiff & 0200) ? 2 : ((Attr & 0200) != 0);
    int groupWrite = (AttrDiff & 0020) ? 2 : ((Attr & 0020) != 0);
    int othersWrite = (AttrDiff & 0002) ? 2 : ((Attr & 0002) != 0);
    int userExec = (AttrDiff & 0100) ? 2 : ((Attr & 0100) != 0);
    int groupExec = (AttrDiff & 0010) ? 2 : ((Attr & 0010) != 0);
    int othersExec = (AttrDiff & 0001) ? 2 : ((Attr & 0001) != 0);
    ti.CheckBox(IDC_READOWNER, userRead);
    ti.CheckBox(IDC_WRITEOWNER, userWrite);
    ti.CheckBox(IDC_EXECUTEOWNER, userExec);
    ti.CheckBox(IDC_READGROUP, groupRead);
    ti.CheckBox(IDC_WRITEGROUP, groupWrite);
    ti.CheckBox(IDC_EXECUTEGROUP, groupExec);
    ti.CheckBox(IDC_READOTHERS, othersRead);
    ti.CheckBox(IDC_WRITEOTHERS, othersWrite);
    ti.CheckBox(IDC_EXECUTEOTHERS, othersExec);

    if (ti.Type == ttDataFromWindow)
    {
        AttrAndMask = 0777;
        AttrOrMask = 0;
        if (userRead == 0)
            AttrAndMask &= ~0400;
        if (groupRead == 0)
            AttrAndMask &= ~0040;
        if (othersRead == 0)
            AttrAndMask &= ~0004;
        if (userWrite == 0)
            AttrAndMask &= ~0200;
        if (groupWrite == 0)
            AttrAndMask &= ~0020;
        if (othersWrite == 0)
            AttrAndMask &= ~0002;
        if (userExec == 0)
            AttrAndMask &= ~0100;
        if (groupExec == 0)
            AttrAndMask &= ~0010;
        if (othersExec == 0)
            AttrAndMask &= ~0001;

        if (userRead == 1)
            AttrOrMask |= 0400;
        if (groupRead == 1)
            AttrOrMask |= 0040;
        if (othersRead == 1)
            AttrOrMask |= 0004;
        if (userWrite == 1)
            AttrOrMask |= 0200;
        if (groupWrite == 1)
            AttrOrMask |= 0020;
        if (othersWrite == 1)
            AttrOrMask |= 0002;
        if (userExec == 1)
            AttrOrMask |= 0100;
        if (groupExec == 1)
            AttrOrMask |= 0010;
        if (othersExec == 1)
            AttrOrMask |= 0001;
    }
    else
        RefreshNumValue();

    ti.CheckBox(IDC_INCLUDESUBDIRS, IncludeSubdirs);
    ti.CheckBox(IDC_CHATTRSETFILES, SelFiles);
    ti.CheckBox(IDC_CHATTRSETDIRS, SelDirs);
    ti.CheckBox(IDC_CHATTRADDTOQUEUE, Config.ChAttrAddToQueue);
}

void UpdateCheckBox(HWND hWindow, int checkID, int value)
{
    if (IsDlgButtonChecked(hWindow, checkID) != (UINT)value)
        CheckDlgButton(hWindow, checkID, value);
}

INT_PTR
CChangeAttrsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CChangeAttrsDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetDlgItemText(HWindow, IDT_CHATTRSUBJECT, Subject);
        EnableWindow(GetDlgItem(HWindow, IDC_INCLUDESUBDIRS), SelDirs);
        EnableWindow(GetDlgItem(HWindow, IDC_CHATTRSETFILES), SelDirs);
        EnableWindow(GetDlgItem(HWindow, IDC_CHATTRSETDIRS), SelDirs);
        SendDlgItemMessage(HWindow, IDE_NUMATTRVALUE, EM_LIMITTEXT, 3, 0);
        break;
    }

    case WM_COMMAND:
    {
        if (EnableNotification)
        {
            if (HIWORD(wParam) == BN_CLICKED &&
                (LOWORD(wParam) == IDC_READOWNER ||
                 LOWORD(wParam) == IDC_WRITEOWNER ||
                 LOWORD(wParam) == IDC_EXECUTEOWNER ||
                 LOWORD(wParam) == IDC_READGROUP ||
                 LOWORD(wParam) == IDC_WRITEGROUP ||
                 LOWORD(wParam) == IDC_EXECUTEGROUP ||
                 LOWORD(wParam) == IDC_READOTHERS ||
                 LOWORD(wParam) == IDC_WRITEOTHERS ||
                 LOWORD(wParam) == IDC_EXECUTEOTHERS))
            {
                RefreshNumValue();
            }
            else
            {
                if (HIWORD(wParam) == EN_CHANGE)
                {
                    char text[5];
                    if (GetDlgItemText(HWindow, IDE_NUMATTRVALUE, text, 5) == 3)
                    {
                        DWORD attr = 0;
                        DWORD attrDiff = 0;
                        if (text[0] >= '0' && text[0] <= '7')
                            attr |= (text[0] - '0') << 6;
                        else
                            attrDiff |= 0700;
                        if (text[1] >= '0' && text[1] <= '7')
                            attr |= (text[1] - '0') << 3;
                        else
                            attrDiff |= 0070;
                        if (text[2] >= '0' && text[2] <= '7')
                            attr |= text[2] - '0';
                        else
                            attrDiff |= 0007;

                        int userRead = (attrDiff & 0400) ? 2 : ((attr & 0400) != 0);
                        int groupRead = (attrDiff & 0040) ? 2 : ((attr & 0040) != 0);
                        int othersRead = (attrDiff & 0004) ? 2 : ((attr & 0004) != 0);
                        int userWrite = (attrDiff & 0200) ? 2 : ((attr & 0200) != 0);
                        int groupWrite = (attrDiff & 0020) ? 2 : ((attr & 0020) != 0);
                        int othersWrite = (attrDiff & 0002) ? 2 : ((attr & 0002) != 0);
                        int userExec = (attrDiff & 0100) ? 2 : ((attr & 0100) != 0);
                        int groupExec = (attrDiff & 0010) ? 2 : ((attr & 0010) != 0);
                        int othersExec = (attrDiff & 0001) ? 2 : ((attr & 0001) != 0);
                        UpdateCheckBox(HWindow, IDC_READOWNER, userRead);
                        UpdateCheckBox(HWindow, IDC_WRITEOWNER, userWrite);
                        UpdateCheckBox(HWindow, IDC_EXECUTEOWNER, userExec);
                        UpdateCheckBox(HWindow, IDC_READGROUP, groupRead);
                        UpdateCheckBox(HWindow, IDC_WRITEGROUP, groupWrite);
                        UpdateCheckBox(HWindow, IDC_EXECUTEGROUP, groupExec);
                        UpdateCheckBox(HWindow, IDC_READOTHERS, othersRead);
                        UpdateCheckBox(HWindow, IDC_WRITEOTHERS, othersWrite);
                        UpdateCheckBox(HWindow, IDC_EXECUTEOTHERS, othersExec);

                        RefreshNumValue();
                    }
                }
            }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CViewErrAsciiTrForBinFileDlg
//

CViewErrAsciiTrForBinFileDlg::CViewErrAsciiTrForBinFileDlg(HWND parent)
    : CCenteredDialog(HLanguage, IDD_VIEWERRASCIITRFORBINFILE, parent)
{
}

INT_PTR
CViewErrAsciiTrForBinFileDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CViewErrAsciiTrForBinFileDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    if (uMsg == WM_INITDIALOG)
    {
        SendDlgItemMessage(HWindow, IDI_INFOICON, STM_SETICON,
                           (WPARAM)HANDLES(LoadIcon(NULL, IDI_QUESTION)), 0);
    }
    if (uMsg == WM_COMMAND && LOWORD(wParam) == IDIGNORE)
    {
        if (Modal)
            EndDialog(HWindow, LOWORD(wParam));
        else
            DestroyWindow(HWindow);
        return TRUE;
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}
