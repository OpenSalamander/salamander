// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "viewer.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "codetbl.h"
#include "usermenu.h"
#include "execute.h"
#include "gui.h"

const char* CVIEWERWINDOW_CLASSNAME = "Salamander's Viewer Window";

char* ViewerHistory[VIEWER_HISTORY_SIZE];

HACCEL ViewerTable = NULL;
BOOL UseCustomViewerFont = FALSE;
LOGFONT ViewerLogFont;
HMENU ViewerMenu = NULL;
int CharWidth = 1,  // character width (in points), by this value we divide = we will not set it to zero at all
    CharHeight = 1; // character height (in points), by this value we will not divide to zero at all

CRITICAL_SECTION ViewerFontMeasureCS;
BOOL ViewerFontMeasured = FALSE;
BOOL ViewerFontNeedsMapping = FALSE;
char ViewerFontMapping[256];

void GetDefaultViewerLogFont(LOGFONT* lf)
{
    const int VIEWER_FONT_PTS = 10;
    memset(lf, 0, sizeof(*lf));
    lf->lfHeight = -(VIEWER_FONT_PTS * SystemDPI) / 72;
    lf->lfWeight = FW_NORMAL;
    lf->lfCharSet = UserCharset;
    lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf->lfQuality = DEFAULT_QUALITY;
    lf->lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    strcpy(lf->lfFaceName, "Consolas");
}

//
//*****************************************************************************

void HistoryComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, char* Text,
                     int textLen, BOOL hexMode, int historySize, char* history[],
                     BOOL changeOnlyHistory)
{
    CALL_STACK_MESSAGE6("HistoryComboBox(, , %d, , %d, %d, %d, , %d)",
                        ctrlID, textLen, hexMode, historySize, changeOnlyHistory);
    HWND hwnd;
    if (changeOnlyHistory || ti.GetControl(hwnd, ctrlID))
    {
        if (!changeOnlyHistory && ti.Type == ttDataToWindow)
        {
            SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
            SendMessage(hwnd, CB_LIMITTEXT, textLen - 1, 0);
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)Text);
        }
        else
        {
            if (!changeOnlyHistory)
            {
                SendMessage(hwnd, WM_GETTEXT, textLen, (LPARAM)Text);
                SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
                SendMessage(hwnd, CB_LIMITTEXT, textLen - 1, 0);
                SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)Text);
            }

            // Handling hex mode
            if (hexMode)
            {
                char* s = Text;
                BOOL openedQuotes = FALSE;
                char* lastQuotes = NULL;
                while (*s != 0 && (openedQuotes || *s == ' ' || *s >= '0' && *s <= '9' ||
                                   LowerCase[*s] >= 'a' && LowerCase[*s] <= 'f' ||
                                   *s == '"'))
                {
                    if (*s == '"')
                    {
                        openedQuotes = !openedQuotes;
                        lastQuotes = s;
                    }
                    s++;
                }
                if (openedQuotes)
                    s = lastQuotes;
                if (*s != 0) // contains a non-hex character
                {
                    if (!changeOnlyHistory)
                    {
                        SalMessageBox(hWindow, LoadStr(IDS_STRINGISNOTHEX), LoadStr(IDS_ERRORTITLE),
                                      MB_OK | MB_ICONEXCLAMATION);
                        SetFocus(hwnd);
                        SendMessage(hwnd, CB_SETEDITSEL, 0, MAKELPARAM(s - Text, 1 + (s - Text)));
                    }
                    ti.ErrorOn(ctrlID);
                }
            }
            // all right. Let's save it to history
            if (ti.IsGood())
            {
                if (Text[0] != 0)
                {
                    BOOL insert = TRUE;
                    int i;
                    for (i = 0; i < historySize; i++)
                    {
                        if (history[i] != NULL)
                        {
                            if (strcmp(history[i], Text) == 0) // if it is already in history
                            {                                  // will go to position 0
                                if (i > 0)
                                {
                                    char* swap = history[i];
                                    memmove(history + 1, history, i * sizeof(char*));
                                    history[0] = swap;
                                }
                                insert = FALSE;
                                break;
                            }
                        }
                        else
                            break;
                    }

                    if (insert)
                    {
                        char* newText = (char*)malloc(strlen(Text) + 1);
                        if (newText != NULL)
                        {
                            strcpy(newText, Text);
                            if (history[historySize - 1] != NULL)
                                free(history[historySize - 1]);
                            memmove(history + 1, history,
                                    (historySize - 1) * sizeof(char*));
                            history[0] = newText;
                        }
                        else
                            TRACE_E(LOW_MEMORY);
                    }
                }
            }
        }

        if (!changeOnlyHistory)
        {
            int i;
            for (i = 0; i < historySize; i++) // filling the combo box list
                if (history[i] != NULL)
                    SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)history[i]);
                else
                    break;
        }
    }
}

//
//*****************************************************************************

void DoHexValidation(HWND edit, const int textLen)
{
    CALL_STACK_MESSAGE2("DoHexValidation(, %d)", textLen);
    int start, end;
    SendMessage(edit, CB_GETEDITSEL, (WPARAM)&start, (LPARAM)&end);
    char* text = new char[textLen];
    if (text == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }
    SendMessage(edit, WM_GETTEXT, textLen, (LPARAM)text);
    char* s = text;
    while (*s != 0 && *s == ' ')
        s++;
    if (s != text)
    {
        start -= (int)(s - text);
        end -= (int)(s - text);
        if (start < 0)
            start = 0;
        if (end < 0)
            end = 0;
        memmove(text, s, strlen(s) + 1);
    }
    s = text;
    BOOL openedQuotes = FALSE;
    char *st = s, *strEnd = text + strlen(text);
    while (*s != 0)
    {
        if (*s == '"')
        {
            if (!openedQuotes && s > text && *(s - 1) != ' ' && strEnd - text < textLen - 1)
            {
                if (start > s - text)
                    start++;
                if (end > s - text)
                    end++;
                memmove(s + 1, s, (strEnd - s) + 1);
                *s++ = ' ';
                strEnd++;
            }
            else
            {
                if (openedQuotes && s + 1 < strEnd && *(s + 1) != ' ' &&
                    strEnd - text < textLen - 1)
                {
                    if (start >= (s - text) + 1)
                        start++;
                    if (end >= (s - text) + 1)
                        end++;
                    memmove(s + 2, s + 1, strEnd - s);
                    *(s + 1) = ' ';
                    strEnd++;
                }
                if (openedQuotes && s + 1 < strEnd)
                    s++;
                st = s + 1;
            }
            openedQuotes = !openedQuotes;
        }
        else
        {
            if (!openedQuotes)
            {
                if (*s == ' ')
                {
                    if (st == s) // '  ' -> ' '
                    {
                        s--;
                        if (start >= st - text)
                            start--;
                        if (end >= st - text)
                            end--;
                        memmove(s, st, (strEnd - st) + 1);
                        strEnd--;
                    }
                    else
                        st = s + 1;
                }
                else
                {
                    if ((s - st) == 2) // 'ABC' -> 'AB C'
                    {
                        if (strEnd - text < textLen - 1)
                        {
                            if (start >= s - text)
                                start++;
                            if (end >= s - text)
                                end++;
                            memmove(s + 1, s, (strEnd - s) + 1);
                            *s = ' ';
                            st = s + 1;
                            strEnd++;
                        }
                    }
                }
            }
        }
        s++;
    }
    SendMessage(edit, WM_SETTEXT, 0, (LPARAM)text);
    SendMessage(edit, CB_SETEDITSEL, 0, MAKELPARAM(start, end));
    delete[] (text);
}

//
//*****************************************************************************

void ConvertHexToString(char* text, char* hex, int& len)
{
    CALL_STACK_MESSAGE2("ConvertHexToString(%s, ,)", text);
    len = 0;
    char *s = text, *st = text;
    BYTE value = 0;
    BOOL openedQuotes = FALSE;
    while (1)
    {
        if (*s == '"')
        {
            s++;
            openedQuotes = !openedQuotes;
            continue;
        }
        if (openedQuotes)
        {
            if (*s == 0)
                break;
            else
                hex[len++] = *s++;
        }
        else
        {
            if (*s != ' ')
            {
                if (*s == 0)
                    break; // end of string
                else
                {
                    if (*s >= '0' && *s <= '9')
                        value = (BYTE)(*s - '0'); // first digit
                    else
                        value = (BYTE)(10 + (LowerCase[*s] - 'a'));
                    s++;
                    if (*s != ' ' && *s != 0 && *s != '"') // second digit
                    {
                        value <<= 4;
                        if (*s >= '0' && *s <= '9')
                            value |= (BYTE)(*s - '0');
                        else
                            value |= (BYTE)(10 + (LowerCase[*s] - 'a'));
                        s++;
                    }
                    hex[len++] = value;
                }
            }
            else
                s++; // skip space
        }
    }
}

//
//*****************************************************************************
// CFindSetDialog
//

void CFindSetDialog::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_FINDHEX, HexMode);
    ti.CheckBox(IDC_VIEWREGEXP, Regular);
    HistoryComboBox(HWindow, ti, IDC_FINDTEXT, Text, FIND_TEXT_LEN, !Regular && HexMode,
                    VIEWER_HISTORY_SIZE, ViewerHistory);
    if (ti.Type == ttDataToWindow)
    { // initialize the searched text according to the label in the viewer (parent of this dialog)
        CWindowsObject* win = WindowsManager.GetWindowPtr(Parent);
        if (win != NULL && win->Is(otViewerWindow)) // for safety test if it is a viewru window
        {
            CViewerWindow* view = (CViewerWindow*)win;
            char buf[FIND_TEXT_LEN];
            char hexBuf[FIND_TEXT_LEN];
            int len;
            if (view->GetFindText(buf, len))
            {
                if (HexMode)
                {
                    if (len * 3 > FIND_TEXT_LEN)
                        len = (FIND_TEXT_LEN - 1) / 3;
                    int i;
                    for (i = 0; i < len; i++)
                    {
                        sprintf(hexBuf + i * 3, i == len - 1 ? "%02X" : "%02X ", (unsigned)buf[i]);
                    }
                    strcpy(buf, hexBuf);
                }
                SendMessage(GetDlgItem(HWindow, IDC_FINDTEXT), WM_SETTEXT, 0, (LPARAM)buf);
            }
        }
    }
    ti.RadioButton(IDC_SBACKWARD, 0, Forward);
    ti.RadioButton(IDC_SFORWARD, 1, Forward);
    ti.CheckBox(IDC_WHOLEWORDS, WholeWords);
    ti.CheckBox(IDC_CASESENSITIVE, CaseSensitive);
}

INT_PTR
CFindSetDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CFindSetDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CancelHexMode = HexMode;
        CancelRegular = Regular;
        EnableWindow(GetDlgItem(HWindow, IDC_FINDHEX), !Regular);
        if (Regular)
            CheckDlgButton(HWindow, IDC_FINDHEX, BST_UNCHECKED);
        ChangeToArrowButton(HWindow, IDC_REGEXP_BROWSE);

        CComboboxEdit* edit = new CComboboxEdit();
        if (edit != NULL)
        {
            HWND hCombo = GetDlgItem(HWindow, IDC_FINDTEXT);
            edit->AttachToWindow(GetWindow(hCombo, GW_CHILD));
        }

        break;
    }

    case WM_USER_CLEARHISTORY:
    {
        // We need to clear the history
        ClearComboboxListbox(GetDlgItem(HWindow, IDC_FINDTEXT));
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        {
            HexMode = CancelHexMode; // to be Cancel correct
            Regular = CancelRegular;
            break;
        }

        case IDC_REGEXP_BROWSE:
        {
            const CExecuteItem* item = TrackExecuteMenu(HWindow, IDC_REGEXP_BROWSE, IDC_FINDTEXT,
                                                        TRUE, RegularExpressionItems);
            if (item != NULL)
            {
                BOOL regular = (IsDlgButtonChecked(HWindow, IDC_VIEWREGEXP) == BST_CHECKED);
                if (item->Keyword == EXECUTE_HELP)
                {
                    // Open the help page dedicated to regular expressions
                    OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, IDH_REGEXP, FALSE);
                }
                if (item->Keyword != EXECUTE_HELP && !regular)
                {
                    // user selected some expression -> we check the checkbox for searching regulars
                    CheckDlgButton(HWindow, IDC_VIEWREGEXP, BST_CHECKED);
                    PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDC_VIEWREGEXP, BN_CLICKED), 0);
                }
            }
            return 0;
        }

        case IDC_VIEWREGEXP:
        {
            Regular = (IsDlgButtonChecked(HWindow, IDC_VIEWREGEXP) != BST_UNCHECKED);
            EnableWindow(GetDlgItem(HWindow, IDC_FINDHEX), !Regular);
            if (Regular)
                CheckDlgButton(HWindow, IDC_FINDHEX, BST_UNCHECKED);
            break;
        }

        case IDC_FINDHEX:
        {
            if (HIWORD(wParam) == BN_CLICKED)
            {
                HexMode = (IsDlgButtonChecked(HWindow, IDC_FINDHEX) != BST_UNCHECKED);
                if (HexMode)
                    CheckDlgButton(HWindow, IDC_CASESENSITIVE, BST_CHECKED);
                return TRUE;
            }
            break;
        }

        case IDC_FINDTEXT:
        {
            if (!Regular && HexMode && HIWORD(wParam) == CBN_EDITUPDATE)
            {
                DoHexValidation((HWND)lParam, FIND_TEXT_LEN);
                return TRUE;
            }
            break;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
//*****************************************************************************
// CViewerGoToOffsetDialog
//

void CViewerGoToOffsetDialog::Validate(CTransferInfo& ti)
{
    int h;
    ti.CheckBox(IDC_VGTO_HEX, h);
    __int64 dummy;
    ti.EditLine(IDE_VGTO_OFFSET, dummy, TRUE, TRUE, h);
}

void CViewerGoToOffsetDialog::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_VGTO_HEX, Configuration.GoToOffsetIsHex);
    ti.EditLine(IDE_VGTO_OFFSET, *Offset, TRUE, TRUE, Configuration.GoToOffsetIsHex);
}

INT_PTR
CViewerGoToOffsetDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CViewerGoToOffsetDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_VGTO_HEX && HIWORD(wParam) == BN_CLICKED)
        { // switch HEX = changing the offset from decimal to hexadecimal and vice versa
            BOOL h = IsDlgButtonChecked(HWindow, IDC_VGTO_HEX) != BST_UNCHECKED;
            CTransferInfo ti(HWindow, ttDataFromWindow);
            __int64 off;
            ti.EditLine(IDE_VGTO_OFFSET, off, TRUE, TRUE, !h, FALSE, TRUE); // We do not show errors, we just do not convert
            if (ti.IsGood())
            {
                CTransferInfo ti2(HWindow, ttDataToWindow);
                ti2.EditLine(IDE_VGTO_OFFSET, off, FALSE, TRUE, h);
            }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
//*****************************************************************************
// CViewerWindow
//

CViewerWindow::CViewerWindow(const char* fileName, CViewType type, const char* caption,
                             BOOL wholeCaption, CObjectOrigin origin,
                             int enumFileNamesSourceUID, int enumFileNamesLastFileIndex)
    : CWindow(origin), LineOffset(300, 100),
      FindDialog(HLanguage, IDD_FINDSET, IDD_FINDSET)
{
    // GDI variables
    BkgndBrush = NULL;
    BkgndBrushSel = NULL;
    ViewerFont = NULL;

    Width = Height = 0;

    // dummy bitmap -- correct size after WM_SIZE
    if (!Bitmap.CreateBmp(NULL, 1, 1))
        TRACE_E("Unable to create bitmap or memory DC for viewer.");

    CreateViewerBrushs();
    SetViewerFont(); // uses Bitmap (must be allocated at least as 1x1) and Width (must be initialized to at least 0)

    // other variables
    HexOffsetLength = 0;
    CanSwitchToHex = TRUE;
    CanSwitchQuietlyToHex = FALSE;
    FindingSoDonotSwitchToHex = FALSE;
    WaitForViewerRefresh = FALSE;
    LastSeekY = 0;
    LastOriginX = 0;
    RepeatCmdAfterRefresh = -1;
    CurrentDir[0] = 0;
    ExitTextMode = FALSE;
    ForceTextMode = FALSE;
    CodeType = 0;
    CodeTables.Init(MainWindow->HWindow);
    UseCodeTable = FALSE;
    if (fileName == NULL)
        FileName = NULL; // error
    else
    {
        char name[MAX_PATH];
        lstrcpyn(name, fileName, MAX_PATH);
        if (SalGetFullName(name))
        {
            FileName = (char*)malloc(strlen(name) + 1);
            if (FileName != NULL)
                strcpy(FileName, name);
        }
        else
            FileName = NULL;
    }
    Buffer = (unsigned char*)malloc(VIEW_BUFFER_SIZE);
    Seek = 0;
    Loaded = 0;
    DefViewMode = Configuration.DefViewMode;
    Type = type;
    OriginX = SeekY = 0;
    MaxSeekY = -1;
    ViewSize = FileSize = 0;
    LastLineSize = FirstLineSize = 0;
    EnablePaint = TRUE;
    StartSelection = -1; // No selection has been made yet
    EndSelection = -1;   // No selection has been made yet
    TooBigSelAction = 0;
    EndSelectionRow = -1;
    EndSelectionPrefX = -1;
    WrapIsBeforeFirstLine = FALSE;
    MouseDrag = FALSE;
    ChangingSelWithShiftKey = FALSE;
    FindOffset = 0;
    ResetFindOffsetOnNextPaint = TRUE;
    SelectionIsFindResult = FALSE;
    ScrollScaleX = ScrollScaleY = 0;
    EnableSetScroll = TRUE;
    ScrollToSelection = FALSE;
    ToolTipOffset = -1;
    HToolTip = NULL;
    Lock = NULL;
    WrapText = Configuration.WrapText;
    CodePageAutoSelect = Configuration.CodePageAutoSelect;
    strcpy(DefaultConvert, Configuration.DefaultConvert);
    LastFindSeekY = -1;
    LastFindOffset = -1;

    if (caption != NULL)
    {
        Caption = DupStr(caption);
        WholeCaption = wholeCaption;
    }
    else
    {
        Caption = NULL;
        WholeCaption = FALSE;
    }
    EnumFileNamesSourceUID = enumFileNamesSourceUID;
    EnumFileNamesLastFileIndex = enumFileNamesLastFileIndex;
    VScrollWParam = -1;

    ResetMouseWheelAccumulator();
}

CViewerWindow::~CViewerWindow()
{
    if (ViewerFont != NULL)
        HANDLES(DeleteObject(ViewerFont));
    ReleaseViewerBrushs();
    if (Lock != NULL)
    {
        SetEvent(Lock);
        Lock = NULL; // now it's just on the disk cache
    }
    if (Buffer != NULL)
        free(Buffer);
    if (FileName != NULL)
        free(FileName);
    if (Caption != NULL)
        free(Caption);
}

HANDLE
CViewerWindow::GetLockObject()
{
    if (Lock == NULL)
        Lock = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    return Lock;
}

void CViewerWindow::CloseLockObject()
{
    if (Lock != NULL)
    {
        HANDLES(CloseHandle(Lock));
        Lock = NULL;
    }
}

void CViewerWindow::FindNewSeekY(__int64 newSeekY, BOOL& fatalErr)
{
    CALL_STACK_MESSAGE2("CViewerWindow::FindNewSeekY(%g,)", (double)newSeekY);
    fatalErr = FALSE;
    if (newSeekY >= MaxSeekY)
        SeekY = MaxSeekY;
    else
    {
        if (newSeekY == 0)
            SeekY = 0;
        else
        {
            newSeekY = FindBegin(newSeekY, fatalErr);
            if (!fatalErr && !ExitTextMode)
                SeekY = newSeekY;
        }
    }
}

int TranslateU2T(int u, BOOL left, int hexOffsetLength)
{
    int i = u - (62 - 8 + hexOffsetLength);
    return left ? (10 - 8 + hexOffsetLength + i * 3 + (i / 4)) : (9 - 8 + hexOffsetLength + i * 3 + ((i - 1) / 4));
}

int GetHexOffsetMode(unsigned __int64 fileSize, int& hexOffsetLength)
{
    if (fileSize == 0)
    {
        hexOffsetLength = 4;
        return 1; // min. 4 places
    }
    --fileSize;                                              // the largest possible offset in the file is one less than the size of the file
    if (fileSize <= CQuadWord(0x0000FFFF, 0x00000000).Value) // 4 spaces are enough
    {
        hexOffsetLength = 4;
        return 1;
    }
    else
    {
        if (fileSize <= CQuadWord(0xFFFFFFFF, 0x00000000).Value) // 8 characters are enough
        {
            hexOffsetLength = 9;
            return 2;
        }
        else
        {
            if (fileSize <= CQuadWord(0xFFFFFFFF, 0x0000FFFF).Value) // 12 characters are enough
            {
                hexOffsetLength = 14;
                return 3;
            }
            else // required 16 places
            {
                hexOffsetLength = 19;
                return 4;
            }
        }
    }
}

#define LOWORD64(qw) ((WORD)((qw)&0xffff))

void PrintHexOffset(char* s, unsigned __int64 offset, int mode)
{
    switch (mode)
    {
    case 1:
        sprintf(s, "%04X", LOWORD64(offset));
        return; // 4 spaces are enough
    case 2:
        sprintf(s, "%04X %04X", LOWORD64(offset >> 16), LOWORD64(offset));
        return; // 8 characters are enough
    case 3:
        sprintf(s, "%04X %04X %04X", LOWORD64(offset >> 32), LOWORD64(offset >> 16), LOWORD64(offset));
        return; // 12 characters are enough
    case 4:
        sprintf(s, "%04X %04X %04X %04X", LOWORD64(offset >> 48), LOWORD64(offset >> 32),
                LOWORD64(offset >> 16), LOWORD64(offset));
        return; // required 16 places
    }
    TRACE_E("Unexpected situation in PrintHexOffset().");
}

void MyTextOut(HDC hdc, int nXStart, int nYStart, LPCTSTR lpString, int cbString)
{
#ifdef _DEBUG
    if (!ViewerFontMeasured)
        TRACE_E("MyTextOut(): ViewerFontMeasured is FALSE!");
#endif // _DEBUG
    if (ViewerFontNeedsMapping)
    {
        const char* s = lpString;
        if (cbString >= 2001)
        {
            cbString = 2000;
            TRACE_E("MyTextOut(): too long string! Truncating to 2000 characters!");
        }
        const char* end = s + cbString;
        char buf[2001];
        char* d = buf;
        while (s < end)
            *d++ = ViewerFontMapping[(unsigned char)*s++];
        *d = 0;
        TextOut(hdc, nXStart, nYStart, buf, cbString);
    }
    else
        TextOut(hdc, nXStart, nYStart, lpString, cbString);
}

void CViewerWindow::Paint(HDC dc)
{
    CALL_STACK_MESSAGE1("CViewerWindow::Paint()");
    if (EnablePaint && !ExitTextMode && FileName != NULL && Width > 0 && Height > 0)
    {
        //    HCURSOR oldCursor = GetCursor();
        //    SetCursor(LoadCursor(NULL, IDC_WAIT));
        //---
        HFONT oldFont = (HFONT)SelectObject(dc, ViewerFont);
        SetTextColor(dc, GetCOLORREF(ViewerColors[VIEWER_FG_NORMAL]));
        SetBkColor(dc, GetCOLORREF(ViewerColors[VIEWER_BK_NORMAL]));
        //---
        HFONT oldFont2 = (HFONT)SelectObject(Bitmap.HMemDC, ViewerFont);
        SetTextColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_FG_NORMAL]));
        SetBkColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_BK_NORMAL]));
        //---
        int oldMode = SetBkMode(Bitmap.HMemDC, TRANSPARENT);

        EnablePaint = FALSE;
        LineOffset.DestroyMembers();
        RECT r;
        r.left = 0;
        r.right = BORDER_WIDTH;
        r.top = 0;
        r.bottom = Height;
        FillRect(dc, &r, BkgndBrush); // delete columns to the left of the text
        RECT fullLine;
        fullLine.left = 0;
        fullLine.top = 0;
        fullLine.right = Width - BORDER_WIDTH;
        fullLine.bottom = CharHeight /*Height*/; //j.r. W2K slow patch

        r.right = Width;
        ViewSize = 0;
        int lines = Height / CharHeight + 1;
        int columns = (Width - BORDER_WIDTH) / CharWidth;
        char line[2001]; // max. 2000 fully visible characters per line + 1 partially visible character
        char* s;
        BOOL fatalErr = FALSE;
        if (columns <= 2000) // only if this maximum is not exceeded.
        {
            // Determine the range of rows that need to be redrawn
            RECT clipRect;
            int clipRet = GetClipBox(dc, &clipRect);
            int clipFirstRow = 0;
            int clipLastRow = lines;
            if (clipRet == SIMPLEREGION || clipRet == COMPLEXREGION)
            {
                clipFirstRow = clipRect.top / CharHeight;
                clipLastRow = clipRect.bottom / CharHeight + 1;
            }

            BOOL setFindOffset = ResetFindOffsetOnNextPaint;
            switch (Type)
            {
            case vtHex:
            {
                FirstLineSize = LastLineSize = 16;
                int hexOffsetMode = GetHexOffsetMode(FileSize, HexOffsetLength);

                __int64 startSel = min(StartSelection, EndSelection);
                if (startSel == -1)
                    startSel = 0;
                __int64 endSel = max(StartSelection, EndSelection);
                if (endSel == -1)
                    endSel = 0;
                if (startSel == endSel)
                    startSel = endSel = 0;
                __int64 lineOffset = SeekY;
                int i;
                for (i = 0; i < lines; i++)
                {
                    __int64 len = Prepare(NULL, lineOffset, 16, fatalErr);
                    // if (fatalErr) FatalFileErrorOccured(); // is below
                    if (fatalErr)
                        break;
                    if (len == 0 && i + 1 != lines && SeekY != 0)
                    {
                        __int64 size = FileSize;
                        FileChanged(NULL, TRUE, fatalErr, FALSE);
                        // if (fatalErr) FatalFileErrorOccured(); // is below
                        if (fatalErr || ExitTextMode)
                            break;
                        if (size != FileSize)
                        {
                            setFindOffset = FALSE; // we will let it be done at the next round of drawing
                            ViewSize = 0;
                            FindNewSeekY(SeekY, fatalErr);
                            if (fatalErr || ExitTextMode)
                                break;
                            FirstLineSize = LastLineSize = 0;
                            // if an expanding text file was being viewed, where the last line ended
                            // When scrolling down to the end of the file, there was a faulty transition to a new line.
                            // redrawing, I expect it also in HEX view, the following invalidate will ensure complete redraw
                            InvalidateRect(HWindow, NULL, FALSE);
                            break;
                        }
                    }
                    if (i + 1 != lines)
                        ViewSize += len; // only whole visible lines

                    s = line;
                    if (len != 0)
                    {
                        PrintHexOffset(s, lineOffset, hexOffsetMode); // line offset
                        s += HexOffsetLength;
                        *s++ = ':';
                        *s++ = ' ';

                        int j;
                        for (j = 0; j < 16; j++)
                        {
                            if (j < len)
                                if ((j % 4) == 3)
                                    s += sprintf(s, "%02X  ", (unsigned int)Buffer[lineOffset - Seek + j]);
                                else
                                    s += sprintf(s, "%02X ", (unsigned int)Buffer[lineOffset - Seek + j]);
                            else if ((j % 4) == 3)
                                s += sprintf(s, "    ");
                            else
                                s += sprintf(s, "   ");
                        }
                        memmove(s, Buffer + (lineOffset - Seek), (int)len);
                        s += len;
                    }

                    int lineLen = (int)(s - line); // print line
                    if (OriginX < lineLen)
                    {
                        int u1, u2;
                        if (startSel < lineOffset + len && endSel > lineOffset)
                        {
                            u1 = (int)(lineLen - len);
                            if (startSel > lineOffset)
                                u1 += (int)(startSel - lineOffset);
                            if (endSel >= lineOffset + len)
                                u2 = lineLen;
                            else
                                u2 = (int)(lineLen - (lineOffset + len - endSel));
                        }
                        else
                        {
                            u1 = lineLen;
                            u2 = lineLen;
                        }
                        int t1, t2;
                        t1 = TranslateU2T(u1, TRUE, HexOffsetLength);
                        t2 = TranslateU2T(u2, FALSE, HexOffsetLength);
                        if (t2 < t1)
                            t2 = t1;

                        if (i >= clipFirstRow && i <= clipLastRow)
                        {
                            RECT myLine = fullLine; // myLine is the area with text that we will draw slowly using BitBlt
                            myLine.right = min(myLine.right, (lineLen + 1) * CharWidth);
                            FillRect(Bitmap.HMemDC, &myLine, BkgndBrush);
                            if (lineLen > OriginX)
                            {
                                char* text = line + OriginX; // Move the text according to OriginX
                                u1 -= (int)OriginX;
                                if (u1 < 0)
                                    u1 = 0;
                                u2 -= (int)OriginX;
                                if (u2 < 0)
                                    u2 = 0;
                                t1 -= (int)OriginX;
                                if (t1 < 0)
                                    t1 = 0;
                                t2 -= (int)OriginX;
                                if (t2 < 0)
                                    t2 = 0;
                                // Print text backwards - for italics
                                // u2, lineLen - OriginX norm
                                if (u2 < lineLen - OriginX)
                                {
                                    MyTextOut(Bitmap.HMemDC, u2 * CharWidth, 0, text + u2,
                                              (int)(lineLen - OriginX - u2));
                                }
                                // u1, u2 selected
                                if (u1 < u2)
                                {
                                    SetBkColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_BK_SELECTED]));
                                    SetTextColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_FG_SELECTED]));
                                    SetBkMode(Bitmap.HMemDC, OPAQUE);
                                    MyTextOut(Bitmap.HMemDC, u1 * CharWidth, 0, text + u1, u2 - u1);
                                    SetBkMode(Bitmap.HMemDC, TRANSPARENT);
                                    SetTextColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_FG_NORMAL]));
                                    SetBkColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_BK_NORMAL]));
                                }
                                // t2, u1 norm
                                if (t2 < u1)
                                {
                                    MyTextOut(Bitmap.HMemDC, t2 * CharWidth, 0, text + t2, u1 - t2);
                                }
                                // t1, t2 select
                                if (t1 < t2)
                                {
                                    SetBkColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_BK_SELECTED]));
                                    SetTextColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_FG_SELECTED]));
                                    SetBkMode(Bitmap.HMemDC, OPAQUE);
                                    MyTextOut(Bitmap.HMemDC, t1 * CharWidth, 0, text + t1, t2 - t1);
                                    SetBkMode(Bitmap.HMemDC, TRANSPARENT);
                                    SetTextColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_FG_NORMAL]));
                                    SetBkColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_BK_NORMAL]));
                                }
                                // 0, t1 norm
                                if (t1 > 0)
                                    MyTextOut(Bitmap.HMemDC, 0, 0, text, t1);
                            }

                            // bitblt the entire row to the screen
                            BitBlt(dc, BORDER_WIDTH, CharHeight * i, (lineLen + 1) * CharWidth, // For italics, we extend the line by one character
                                   CharHeight, Bitmap.HMemDC, 0, 0, SRCCOPY);
                            // if necessary, we will reclaim space to the right
                            if (myLine.right < fullLine.right)
                            {
                                myLine.top = CharHeight * i;
                                myLine.bottom = myLine.top + CharHeight;
                                myLine.left = BORDER_WIDTH + myLine.right;
                                myLine.right = BORDER_WIDTH + fullLine.right;
                                FillRect(dc, &myLine, BkgndBrush);
                            }
                        }
                    }

                    lineOffset += len;
                    if (lineOffset == FileSize)
                    {
                        r.left = BORDER_WIDTH;
                        if (FileSize > 0) // JR: up to AS2.52b1 (inclusive), we displayed a 0-byte file in hex by not underlining the first line (it happened when resizing the window)
                            r.top = CharHeight * (i + 1);
                        else
                            r.top = 0;
                        r.bottom = Height;
                        if (r.top <= r.bottom)
                            FillRect(dc, &r, BkgndBrush);
                        break;
                    }
                }
                break;
            }

            case vtText:
            {
                __int64 xRollLimit = (Width - BORDER_WIDTH) / CharWidth / 6;
                FirstLineSize = LastLineSize = 0;

                WrapIsBeforeFirstLine = FALSE;
                if (WrapText && SeekY > 0)
                {
                    __int64 len = Prepare(NULL, SeekY - (SeekY > 1 ? 2 : 1), SeekY > 1 ? 2 : 1, fatalErr);
                    // if (fatalErr) FatalFileErrorOccured(); // is below
                    if (fatalErr)
                        break;

                    unsigned char* s = Buffer + (SeekY - Seek) - 1;
                    if (!(*s == '\n' && Configuration.EOL_LF ||
                          *s == '\r' && Configuration.EOL_CR ||
                          *s == 0 && Configuration.EOL_NULL ||
                          SeekY > 1 && *(s - 1) == '\r' && *s == '\n' && Configuration.EOL_CRLF))
                    {
                        WrapIsBeforeFirstLine = TRUE;
                    }
                }

                RECT endRect = fullLine;
                __int64 lineOffset = SeekY; // offset of the beginning of the line in bytes
                BOOL EOL = FALSE;           // TRUE if the last line ended with EOL (next line exists, may be empty)
                int lineEOLSize = 0;        // length of EOL (CR=1, LF=1, CRLF=2, NULL=1)
                for (int i = 0; i < lines; i++)
                {
                    __int64 len = Prepare(NULL, lineOffset, APROX_LINE_LEN, fatalErr);
                    // if (fatalErr) FatalFileErrorOccured(); // is below
                    if (fatalErr)
                        break;
                    if (len == 0 && i + 1 != lines && SeekY != 0)
                    {
                        __int64 size = FileSize;
                        FileChanged(NULL, TRUE, fatalErr, FALSE);
                        // if (fatalErr) FatalFileErrorOccured(); // is below
                        if (fatalErr || ExitTextMode)
                            break;
                        if (size != FileSize)
                        {
                            setFindOffset = FALSE; // we will let it be done at the next round of drawing
                            LineOffset.DestroyMembers();
                            ViewSize = 0;
                            FindNewSeekY(SeekY, fatalErr);
                            if (fatalErr || ExitTextMode)
                                break;
                            FirstLineSize = LastLineSize = 0;
                            // if an expanding text file was being viewed, where the last line ended
                            // When scrolling down to the end of the file, there was a faulty transition to a new line.
                            // redrawing, the following invalidate will ensure complete redraw
                            InvalidateRect(HWindow, NULL, FALSE);
                            break;
                        }
                    }

                    if (len == 0)
                    {
                        int redrI = i;
                        if (redrI < clipFirstRow)
                            redrI = clipFirstRow; // we will not redraw unnecessarily
                        r.left = BORDER_WIDTH;
                        r.top = CharHeight * redrI;
                        r.bottom = CharHeight * clipLastRow;
                        if (r.bottom > Height)
                            r.bottom = Height;
                        if (r.top <= r.bottom)
                            FillRect(dc, &r, BkgndBrush);

                        if (EOL) // adding the last empty line to the LineOffset array -> the line cannot be ignored
                        {
                            LineOffset.Add(lineOffset);
                            LineOffset.Add(lineOffset);
                            LineOffset.Add(0);
                        }
                        break;
                    }

                    unsigned char* st;                                               // start of buffer with content of line
                    unsigned char* s2;                                               // character processed from the buffer containing a line
                    __int64 lineLen = 0;                                             // length of line in characters (tabulator != 1 character)
                    BOOL lineEndIsWrapped = FALSE;                                   // Is the end of the line broken due to the enabled wrap mode?
                    __int64 fullLineLen = 0;                                         // length of line in bytes
                    __int64 endX = OriginX + (Width - BORDER_WIDTH) / CharWidth + 1; // offset of the screen edge in characters
                    BOOL onlyOne = (len == 1);                                       // last character of the file ?
                    __int64 startSel = min(StartSelection, EndSelection);            // start of the label - offset in bytes
                    if (startSel == -1)
                        startSel = 0;
                    __int64 endSel = max(StartSelection, EndSelection); // end of the tag - offset in bytes
                    if (endSel == -1)
                        endSel = 0;
                    if (startSel == endSel)
                        startSel = endSel = 0;
                    BOOL startSelDone = startSel <= lineOffset; // the beginning of the label is already drawn
                    BOOL endSelDone = endSel < lineOffset;      // end of the marking is already drawn

                    __int64 tabSpaces = 0; // shift due to tabulators
                    EOL = FALSE;
                    lineEOLSize = 0;
                    while (len != 0)
                    {
                        st = s2 = Buffer + (lineOffset + fullLineLen - Seek);
                        while (len--)
                        {
                            if (*s2 == '\r') // 'end of line \r' or '\r\n'
                            {
                                BOOL ok = FALSE;
                                if (len > 0)
                                {
                                    if (*(s2 + 1) == '\n' && Configuration.EOL_CRLF)
                                    {
                                        s2 += 2; // '\r\n'
                                        len--;
                                        ok = TRUE;
                                        EOL = TRUE;
                                        lineEOLSize = 2;
                                    }
                                    else if (Configuration.EOL_CR)
                                    {
                                        ok = TRUE;
                                        s2++; // '\r'
                                        EOL = TRUE;
                                        lineEOLSize = 1;
                                    }
                                }
                                else
                                {
                                    if (!onlyOne)
                                    { // the line has not ended yet, '\n' can be beyond the border
                                        if (Configuration.EOL_CRLF)
                                        {
                                            len = -1;
                                            break;
                                        }
                                        else
                                        {
                                            if (Configuration.EOL_CR)
                                            {
                                                ok = TRUE;
                                                s2++; // '\r'
                                                EOL = TRUE;
                                                lineEOLSize = 1;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (Configuration.EOL_CR)
                                        {
                                            ok = TRUE;
                                            s2++; // the last character of the file is '\r'
                                            EOL = TRUE;
                                            lineEOLSize = 1;
                                        }
                                    }
                                }

                                if (ok)
                                    break;
                                else
                                    goto COMMON_CHAR;
                            }
                            else
                            {
                                if (*s2 == '\n' || *s2 == 0) // end of line '\n' or 0
                                {
                                    if ((*s2 == '\n') ? Configuration.EOL_LF : Configuration.EOL_NULL)
                                    {
                                        s2++;
                                        EOL = TRUE;
                                        lineEOLSize = 1;
                                        break;
                                    }
                                    else
                                        goto COMMON_CHAR;
                                }
                                else
                                {
                                    if (*s2 == '\t')
                                    {
                                        __int64 curOff = lineOffset + fullLineLen + (s2 - st);
                                        if (!startSelDone && startSel <= curOff)
                                        {
                                            startSelDone = TRUE;
                                            startSel += tabSpaces;
                                        }
                                        if (!endSelDone && endSel <= curOff)
                                        {
                                            endSelDone = TRUE;
                                            endSel += tabSpaces;
                                        }
                                        int tab = (int)(Configuration.TabSize - (lineLen % Configuration.TabSize));
                                        if (WrapText && lineLen + tab - 1 >= columns)
                                        {
                                            tab = (int)max(1, columns - lineLen); // max. to the edge, at least 1 character
                                        }
                                        tabSpaces += tab - 1;
                                        if (lineLen > OriginX - tab && endX > lineLen)
                                            memset(line + max(0, lineLen - OriginX), ' ', min(tab, (int)(endX - lineLen)));

                                        lineLen += tab - 1;
                                    }
                                    else
                                    {
                                    COMMON_CHAR:

                                        if (lineLen >= OriginX && lineLen < endX)
                                            line[lineLen - OriginX] = *s2;
                                    }
                                }
                            }
                            if (WrapText && lineLen >= columns)
                            {
#ifdef _DEBUG
                                if (lineLen > columns)
                                    TRACE_E("something's wrong");
#endif // _DEBUG
                                lineEndIsWrapped = TRUE;
                                break; // premature end of line
                            }
                            s2++;
                            lineLen++;
                        }
                        fullLineLen += s2 - st;

                        // test for the length of the text line (over 10000 characters we offer HEX mode)
                        if (CanSwitchToHex && !ForceTextMode && fullLineLen > TEXT_MAX_LINE_LEN)
                        {
                            if (!CanSwitchQuietlyToHex)
                                CanSwitchToHex = FALSE;
                            if (CanSwitchQuietlyToHex ||
                                SalMessageBox(HWindow, LoadStr(IDS_VIEWER_BINFILE), LoadStr(IDS_VIEWERTITLE),
                                              MB_YESNO | MB_ICONQUESTION) == IDYES)
                            {
                                CanSwitchQuietlyToHex = FALSE;
                                ExitTextMode = TRUE;
                                PostMessage(HWindow, WM_COMMAND, CM_TO_HEX, 0);
                                break;
                            }
                            else
                            {
                                ForceTextMode = TRUE;
                            }
                        }

                        if (len == -1) // line continues to an as yet unread section
                        {
                            len = Prepare(NULL, lineOffset + fullLineLen, APROX_LINE_LEN, fatalErr);
                            // if (fatalErr) FatalFileErrorOccured(); // is below
                            if (fatalErr)
                                break;
                            onlyOne = (len == 1);
                        }
                        else
                            break; // line is fully loaded
                    }
                    if (fatalErr || ExitTextMode)
                        break;
                    LineOffset.Add(lineOffset);                             // offset at the beginning of the line
                    LineOffset.Add(lineOffset + fullLineLen - lineEOLSize); // Offset of end of line (before EOL)
                    LineOffset.Add(lineLen);                                // length of line in displayed characters (TAB counts as multiple characters)
                    if (!startSelDone)
                        startSel += tabSpaces;
                    if (!endSelDone)
                        endSel += tabSpaces;

                    if (ScrollToSelection)
                    {
                        int len2 = (Width - BORDER_WIDTH) / CharWidth;
                        if (len2 - 2 * xRollLimit < endSel - startSel)
                        { // we will try to display as many wide chains as possible, so xRollLimit -> 0
                            xRollLimit = (len2 - (endSel - startSel)) / 2;
                            if (xRollLimit < 0)
                                xRollLimit = 0;
                        }
                        __int64 left = lineOffset + OriginX;
                        __int64 right = lineOffset + OriginX + len2;
                        if (startSel < lineOffset + lineLen)
                        {
                            __int64 originX = OriginX;
                            if (startSel < left) // view is too far to the right
                            {
                                originX = startSel - lineOffset - xRollLimit;
                                if (originX < 0)
                                    originX = 0;
                            }
                            else
                            {
                                if (startSel >= right ||
                                    endSel < lineOffset + lineLen && endSel >= right)
                                { // view is too much to the left
                                    originX = startSel - lineOffset - xRollLimit;
                                    if (originX < 0)
                                        originX = 0;
                                    __int64 originX2 = endSel - lineOffset - len2 + 1 + xRollLimit;
                                    if (originX2 < 0)
                                        originX2 = 0;
                                    originX = min(originX, originX2);
                                }
                            }
                            if (originX != OriginX) // change has occurred
                            {
                                setFindOffset = FALSE; // reset FindOffset probably won't be necessary, but if needed, we will do it during the next drawing cycle
                                OriginX = originX;
                                InvalidateRect(HWindow, NULL, FALSE);
                                break; // it is necessary to start over
                            }
                            else
                                ScrollToSelection = FALSE; // not needed
                        }
                    }

                    if (i == 0)
                        FirstLineSize = fullLineLen;
                    if (i + 1 < lines)
                    {
                        ViewSize += fullLineLen; // only whole visible lines
                        if (i + 2 == lines)
                            LastLineSize = fullLineLen;
                    }

                    BOOL blackEnd;
                    if (OriginX < lineLen)
                    {
                        __int64 len2 = min((Width - BORDER_WIDTH) / CharWidth + 1, lineLen - OriginX);
                        __int64 left = lineOffset + OriginX;
                        __int64 right = lineOffset + OriginX + len2;
                        __int64 u1 = len2, u2 = 0, u3 = 0;
                        blackEnd = (lineEndIsWrapped ? startSel < right : startSel <= right) && endSel > right;
                        if (startSel <= left)
                        {
                            if (endSel > left)
                            {
                                u1 = 0;
                                u2 = min(right, endSel) - left;
                                u3 = len2 - u2;
                            }
                        }
                        else if (startSel < right)
                        {
                            if (endSel > startSel)
                            {
                                u1 = startSel - left;
                                u2 = min(right, endSel) - left - u1;
                                u3 = len2 - u2 - u1;
                            }
                        }

                        if (i >= clipFirstRow && i <= clipLastRow)
                        {
                            RECT myLine = fullLine;                                        // myLine is the area with text that we will draw slowly using BitBlt
                            myLine.right = min(myLine.right, (int)(len2 + 1) * CharWidth); // Extra character for italics

                            if (blackEnd)
                            {
                                endRect.left = 0;
                                endRect.right = (int)((u1 + u2 + u3) * CharWidth);
                                FillRect(Bitmap.HMemDC, &endRect, BkgndBrush);
                                endRect.left = endRect.right;
                                endRect.right = Width - BORDER_WIDTH;
                                FillRect(Bitmap.HMemDC, &endRect, BkgndBrushSel);
                            }
                            else
                                FillRect(Bitmap.HMemDC, &myLine, BkgndBrush);

                            if (lineLen > OriginX)
                            { // print text to Bitmap.HMemDC
                                if (u3 > 0)
                                    MyTextOut(Bitmap.HMemDC, (int)((u1 + u2) * CharWidth), 0, line + u1 + u2, (int)u3);
                                if (u2 > 0)
                                {
                                    SetBkColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_BK_SELECTED]));
                                    SetTextColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_FG_SELECTED]));
                                    SetBkMode(Bitmap.HMemDC, OPAQUE);
                                    MyTextOut(Bitmap.HMemDC, (int)(u1 * CharWidth), 0, line + u1, (int)u2);
                                    SetBkMode(Bitmap.HMemDC, TRANSPARENT);
                                    SetTextColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_FG_NORMAL]));
                                    SetBkColor(Bitmap.HMemDC, GetCOLORREF(ViewerColors[VIEWER_BK_NORMAL]));
                                }
                                if (u1 > 0)
                                    MyTextOut(Bitmap.HMemDC, 0, 0, line, (int)u1);
                            }

                            // bitblt the entire row to the screen
                            BitBlt(dc, BORDER_WIDTH, CharHeight * i, myLine.right,
                                   CharHeight, Bitmap.HMemDC, 0, 0, SRCCOPY);

                            // if necessary, we will reclaim space to the right
                            if (myLine.right < fullLine.right)
                            {
                                myLine.top = CharHeight * i;
                                myLine.bottom = myLine.top + CharHeight;
                                myLine.left = BORDER_WIDTH + myLine.right;
                                myLine.right = BORDER_WIDTH + fullLine.right;
                                FillRect(dc, &myLine, blackEnd ? BkgndBrushSel : BkgndBrush);
                            }
                        }
                    }
                    else
                    {
                        if (i >= clipFirstRow && i <= clipLastRow)
                        {
                            blackEnd = (lineEndIsWrapped ? startSel < lineOffset + lineLen : startSel <= lineOffset + lineLen) &&
                                       endSel > lineOffset + lineLen;
                            r.left = BORDER_WIDTH;
                            r.top = CharHeight * i;
                            r.right = Width;
                            r.bottom = r.top + CharHeight;
                            FillRect(dc, &r, blackEnd ? BkgndBrushSel : BkgndBrush);
                        }
                    }
                    lineOffset += fullLineLen;
                }
                break;
            }
            }
            if (setFindOffset && !fatalErr && !ExitTextMode)
            {
                ResetFindOffsetOnNextPaint = FALSE;
                FindOffset = SeekY;
                if (!FindDialog.Forward)
                    FindOffset += ViewSize;
            }
        }
        EnablePaint = TRUE;
        ScrollToSelection = FALSE;
        SetBkMode(Bitmap.HMemDC, oldMode);
        //---
        SelectObject(dc, oldFont);
        SelectObject(Bitmap.HMemDC, oldFont2);
        //---
        if (fatalErr)
            FatalFileErrorOccured();
        if (fatalErr || ExitTextMode)
            return;
        SetScrollBar();
        //    SetCursor(oldCursor);
    }
    else // let's at least clear the screen
    {
        RECT r;
        r.left = 0;
        r.right = Width;
        r.top = 0;
        r.bottom = Height;
        FillRect(dc, &r, BkgndBrush); // delete columns to the left of the text
        SetScrollBar();
    }
}

//
// ****************************************************************************

BOOL CViewerWindow::CreateViewerBrushs()
{
    BkgndBrush = HANDLES(CreateSolidBrush(GetCOLORREF(ViewerColors[VIEWER_BK_NORMAL])));
    if (BkgndBrush == NULL)
    {
        TRACE_E("Unable to create window background brush.");
        return FALSE;
    }
    BkgndBrushSel = HANDLES(CreateSolidBrush(GetCOLORREF(ViewerColors[VIEWER_BK_SELECTED])));
    if (BkgndBrushSel == NULL)
    {
        TRACE_E("Unable to create window selected text background brush.");
        return FALSE;
    }
    return TRUE;
}

void UpdateViewerColors(SALCOLOR* colors)
{
    if (GetFValue(colors[VIEWER_FG_NORMAL]) & SCF_DEFAULT)
        SetRGBPart(&colors[VIEWER_FG_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
    if (GetFValue(colors[VIEWER_BK_NORMAL]) & SCF_DEFAULT)
        SetRGBPart(&colors[VIEWER_BK_NORMAL], GetSysColor(COLOR_WINDOW));
    if (GetFValue(colors[VIEWER_FG_SELECTED]) & SCF_DEFAULT)
        SetRGBPart(&colors[VIEWER_FG_SELECTED], GetSysColor(COLOR_WINDOW));
    if (GetFValue(colors[VIEWER_BK_SELECTED]) & SCF_DEFAULT)
        SetRGBPart(&colors[VIEWER_BK_SELECTED], GetSysColor(COLOR_WINDOWTEXT));
}

BOOL InitializeViewer()
{
    int i;
    for (i = 0; i < VIEWER_HISTORY_SIZE; i++)
        ViewerHistory[i] = NULL;

    GetDefaultViewerLogFont(&ViewerLogFont);

    HANDLES(InitializeCriticalSection(&ViewerFontMeasureCS));

    UpdateViewerColors(ViewerColors);
    ViewerMenu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_VIEWERMENU));
    if (ViewerMenu == NULL)
    {
        TRACE_E("Unable to load menu for viewer.");
        return FALSE;
    }
    MENUITEMINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_TYPE | MIIM_SUBMENU;
    mi.fType = MFT_STRING;
    mi.hSubMenu = CreatePopupMenu();
    mi.dwTypeData = LoadStr(IDS_VIEWERCODINGMENU);
    InsertMenuItem(ViewerMenu, CODING_MENU_INDEX, TRUE, &mi);

    ViewerTable = HANDLES(LoadAccelerators(HInstance, MAKEINTRESOURCE(IDA_VIEWERACCELS)));
    if (ViewerTable == NULL)
    {
        TRACE_E("Unable to load accelerators for viewer.");
        return FALSE;
    }

    if (!CViewerWindow::RegisterUniversalClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
                                               0,
                                               0,
                                               HANDLES(LoadIcon(HInstance,
                                                                MAKEINTRESOURCE(IDI_VIEWER))),
                                               LoadCursor(NULL, IDC_ARROW),
                                               (HBRUSH)(COLOR_WINDOW + 1),
                                               NULL,
                                               CVIEWERWINDOW_CLASSNAME,
                                               NULL))
    {
        TRACE_E("Unable to register window class for viewer.");
        return FALSE;
    }

    ViewerContinue = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    if (ViewerContinue == NULL)
    {
        TRACE_E("Unable to create ViewerContinue event.");
        return FALSE;
    }

    return TRUE;
}

void CViewerWindow::ReleaseViewerBrushs()
{
    if (BkgndBrush != NULL)
    {
        HANDLES(DeleteObject(BkgndBrush));
        BkgndBrush = NULL;
    }
    if (BkgndBrushSel != NULL)
    {
        HANDLES(DeleteObject(BkgndBrushSel));
        BkgndBrushSel = NULL;
    }
}

void ClearViewerHistory(BOOL dataOnly)
{
    int i;
    for (i = 0; i < VIEWER_HISTORY_SIZE; i++)
    {
        if (ViewerHistory[i] != NULL)
        {
            free(ViewerHistory[i]);
            ViewerHistory[i] = NULL;
        }
    }

    if (!dataOnly)
    {
        // We need to also modify the combobox in the open Find windows
        ViewerWindowQueue.BroadcastMessage(WM_USER_CLEARHISTORY, 0, 0);
    }
}

void ReleaseViewer()
{
    if (ViewerMenu != NULL)
        DestroyMenu(ViewerMenu);
    ClearViewerHistory(TRUE); // we just want to trim the data
    if (ViewerContinue != NULL)
        HANDLES(CloseHandle(ViewerContinue));
    HANDLES(DeleteCriticalSection(&ViewerFontMeasureCS));
}

void CViewerWindow::SetViewerFont()
{
    if (ViewerFont != NULL)
        HANDLES(DeleteObject(ViewerFont));
    LOGFONT lf;
    if (UseCustomViewerFont)
        lf = ViewerLogFont;
    else
        GetDefaultViewerLogFont(&lf);
    ViewerFont = HANDLES(CreateFontIndirect(&lf));
    if (ViewerFont == NULL)
    {
        TRACE_E("Unable to create ViewerFont.");
        return;
    }
    else
    {
        HDC dc = HANDLES(GetDC(NULL));
        HFONT old = (HFONT)SelectObject(dc, ViewerFont);
        TEXTMETRIC tm;
        BOOL ok = GetTextMetrics(dc, &tm);
        CharHeight = max(1, tm.tmHeight);
        CharWidth = max(1, tm.tmAveCharWidth);
        SelectObject(dc, old);
        HANDLES(ReleaseDC(NULL, dc));

        if (!ok)
        {
            TRACE_E("Unable to get text metrics for ViewerFont.");
            HANDLES(DeleteObject(ViewerFont));
            ViewerFont = NULL;
            return;
        }

        if (Bitmap.HBmp != NULL)
            Bitmap.ReCreateForScreenDC(Width, CharHeight);

        HANDLES(EnterCriticalSection(&ViewerFontMeasureCS));

        // Vista: the font fixedsys contains characters that do not have the expected width (even though it is a fixed-width font), therefore
        // We iterate over individual characters and map those that do not have the correct width to a replacement character of the correct width
        if (!WindowsXP64AndLater && !ViewerFontMeasured) // Before XP64 and Vista, we did not encounter this issue, so we will not test it (on XP, W2K, NT4, etc.)
        {
            ViewerFontMeasured = TRUE;
            ViewerFontNeedsMapping = FALSE;
        }
        if (!ViewerFontMeasured)
        {
            HFONT oldFont = (HFONT)SelectObject(Bitmap.HMemDC, ViewerFont);
            int oldMode = SetBkMode(Bitmap.HMemDC, TRANSPARENT);

            ViewerFontNeedsMapping = FALSE;
            char ch[2];
            ch[1] = 0;
            RECT rect;
            char substChar = (char)0xB7 /* middle dot*/;
            int x;
            for (x = 0; x < 256; x++)
            {
                ViewerFontMapping[x] = x;
                ch[0] = x;
                rect.left = 0;
                rect.right = Width;
                rect.top = 0;
                rect.bottom = CharHeight;
                if (DrawTextEx(Bitmap.HMemDC, ch, 1, &rect, DT_LEFT | DT_TOP | DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE, NULL))
                {
                    if (rect.right - rect.left != CharWidth)
                    {
                        if (x == 0xB7 /* middle dot*/) // if the 'middle-dot' is also too wide, we substitute it with a space, which should be fine
                        {
                            substChar = ' ';
                            int z;
                            for (z = 0; z < x; z++)
                                if (ViewerFontMapping[z] == (char)0xB7 /* middle dot*/)
                                    ViewerFontMapping[z] = substChar;
                        }
                        ViewerFontMapping[x] = substChar;
                        ViewerFontNeedsMapping = TRUE;
                    }
                }
                else
                    TRACE_I("CViewerWindow::SetViewerFont(): DrawTextEx: error for: " << x);
            }

            SetBkMode(Bitmap.HMemDC, oldMode);
            SelectObject(Bitmap.HMemDC, oldFont);

            ViewerFontMeasured = TRUE;
        }

        HANDLES(LeaveCriticalSection(&ViewerFontMeasureCS));
    }
}
