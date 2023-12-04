// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dialogs.h"
#include "stswnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "gui.h"
#include "logo.h"

// pomocny objekt pro zasilani Ctrl+C do parenta prostrednictvim zpravy WM_COPY
class CKeyForwarderWindow : public CWindow
{
public:
    CKeyForwarderWindow(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID)
    {
    }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_KEYDOWN)
        {
            BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (!shiftPressed && controlPressed && !altPressed)
            {
                if (wParam == 'C')
                {
                    HWND hParent = GetParent(HWindow);
                    if (hParent != NULL)
                        PostMessage(hParent, WM_COPY, 0, 0);
                }
            }
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

//****************************************************************************
//
// CMessageBox
//

CMessageBox::CMessageBox(HWND parent, DWORD flags, const char* title, const char* text,
                         const char* checkText, BOOL* check, HICON hOwnIcon,
                         DWORD contextHelpId, MSGBOXEX_CALLBACK helpCallback,
                         const char* aliasBtnNames, const char* url, const char* urlText)
    : CCommonDialog(HInstance, IDD_MSGBOX, parent, ooStandard)
{
    Flags = flags;
    if (title == NULL)
        Title = DupStr(LoadStr(IDS_ERRORTITLE));
    else
        Title = DupStr(title);
    Text.Set(text, NULL);
    if (checkText != NULL)
    {
        if (check == NULL)
        {
            TRACE_E("CMessageBox::CMessageBox: check parameter is NULL.");
            CheckText = NULL;
        }
        else
            CheckText = DupStr(checkText);
    }
    else
        CheckText = NULL;
    Check = check;
    HOwnIcon = hOwnIcon;
    if (Flags & MSGBOXEX_HELP)
        SetHelpID(contextHelpId);
    HelpCallback = helpCallback;
    AliasBtnNames = DupStr(aliasBtnNames);
    URL = DupStr(url);
    URLText = DupStr(urlText);
    BackgroundSeparator = 0;
}

CMessageBox::CMessageBox(HWND parent, DWORD flags, const char* title, CTruncatedString* text,
                         const char* checkText, BOOL* check, HICON hOwnIcon,
                         DWORD contextHelpId, MSGBOXEX_CALLBACK helpCallback,
                         const char* aliasBtnNames, const char* url, const char* urlText)
    : CCommonDialog(HInstance, IDD_MSGBOX, parent)
{
    Flags = flags;
    if (title == NULL)
        Title = DupStr(LoadStr(IDS_ERRORTITLE));
    else
        Title = DupStr(title);
    Text.CopyFrom(text);
    if (checkText != NULL)
    {
        if (check == NULL)
        {
            TRACE_E("CMessageBox::CMessageBox: check parameter is NULL.");
            CheckText = NULL;
        }
        else
            CheckText = DupStr(checkText);
    }
    else
        CheckText = NULL;
    Check = check;
    HOwnIcon = hOwnIcon;
    if (Flags & MSGBOXEX_HELP)
        SetHelpID(contextHelpId);
    HelpCallback = helpCallback;
    AliasBtnNames = DupStr(aliasBtnNames);
    URL = DupStr(url);
    URLText = DupStr(urlText);
    BackgroundSeparator = 0;
}

CMessageBox::~CMessageBox()
{
    if (Title != NULL)
        free(Title);
    if (CheckText != NULL)
        free(CheckText);
    if (AliasBtnNames != NULL)
        free(AliasBtnNames);
    if (URL != NULL)
        free(URL);
    if (URLText != NULL)
        free(URLText);
}

void CMessageBox::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CMessageBox::Transfer()");
    if (CheckText != NULL && Check != NULL)
        ti.CheckBox(IDS_MSGBOX_CHECK, *Check);
}

int CMessageBox::Execute()
{
    SplashScreenCloseIfExist();

    HWND mainWnd = GetWndToFlash(Parent);

    // tento patch prinesl hromadu padacek v SS2.5RC1, takze od nej opoustime
    // a opravujeme FileComparator, aby se WM_USER_ACTIVATEWINDOW chovala mene
    // agresivne
    /*
  // napred dorucime zpravy (standardni MessageBox se chova stejne)
  // napriklad File Comparator ma ve fronte postuntou zpravu WM_USER_ACTIVATEWINDOW
  // kterou bez teto pumpy dorucime az po aktivaci messageboxu, takze nam nasledne
  // diffwnd ukradnul aktivaci
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  */

    int ret = (int)CCommonDialog::Execute();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);

    return ret;
}

// posune child window o dx a dy
void OffsetChildWindow(HWND hDialog, int resID, int dx, int dy)
{
    HWND hWnd = GetDlgItem(hDialog, resID);
    RECT windowR;
    GetWindowRect(hWnd, &windowR);
    POINT p;
    p.x = windowR.left;
    p.y = windowR.top;
    ScreenToClient(hDialog, &p);
    SetWindowPos(hWnd, NULL, p.x + dx, p.y + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

BOOL CMessageBox::EscapeEnabled()
{
    DWORD flagsType = Flags & MSGBOXEX_TYPEMASK;
    if (flagsType == MSGBOXEX_ABORTRETRYIGNORE)
        return (Flags & MSGBOXEX_ESCAPEENABLED) != 0;
    if (flagsType == MSGBOXEX_YESNO)
        return (Flags & MSGBOXEX_ESCAPEENABLED) != 0;
    else
        return TRUE;
}

// vraci kopii 'src', do ktere jsou vlozeny znaky 'n' tak, aby vysledna maximalni
// sirka nepresahla 'maxWidth'. Predpoklada, ze hDC ma vybrany patricny font.
// v pripade neuspechu vraci NULL

char* DuplicateStrAndInsertEOLs(const char* src, HDC hDC, int maxWidth)
{
    if (src == NULL || *src == 0)
        return NULL;

    int srcLen = (int)strlen(src);

    // pole pro ulozeni castecnych delek v retezci
    int* alpDx = (int*)malloc((1 + srcLen + 1) * sizeof(int)); // pro jistotu o polozku vice
    if (alpDx == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return NULL;
    }
    alpDx[0] = 0;

    SIZE sz;
    if (!GetTextExtentExPoint(hDC, src, srcLen, 0, NULL, alpDx + 1, &sz))
    {
        free(alpDx);
        return NULL;
    }

    int breakCount = 0;
    int lineLen = 0;
    int i;
    for (i = 0; i < srcLen; i++)
    {
        if (src[i] == '\n')
        {
            // konec radku
            lineLen = 0;
        }
        else
        {
            if (lineLen + (alpDx[i + 1] - alpDx[i]) > maxWidth)
            {
                alpDx[breakCount] = i;
                breakCount++;
                lineLen = 0;
            }
            else
            {
                lineLen += alpDx[i + 1] - alpDx[i];
            }
        }
    }

    if (breakCount > 0)
    {
        char* text = (char*)malloc((srcLen + breakCount + 1) * sizeof(char));
        if (text != NULL)
        {
            memcpy(text, src, srcLen + 1);
            int i2;
            for (i2 = 0; i2 < breakCount; i2++)
            {
                memmove(text + alpDx[i2] + 1, text + alpDx[i2], srcLen - alpDx[i2] + 1 + i2);
                text[alpDx[i2]] = '\n';
            }
            free(alpDx);
            return text;
        }
    }

    free(alpDx);
    return NULL;
}

BOOL CMessageBox::CopyToClipboard()
{
    const char* separator = "---------------------------\r\n";

    // napocitame celkovou velikost buffer
    const char* text = Text.Get();
    int urlTextLen = 0;
    if (URL != NULL)
    {
        urlTextLen += (int)strlen(URL) + 4;
        if (URLText != NULL)
            urlTextLen += (int)strlen(URLText) + 4;
    }
    int buffSize = 4 * (int)strlen(separator) +
                   (int)strlen(Title) +
                   2 * (int)strlen(text) + // vsechny znaky mohou byt '\n' budeme je konvertovat na "\r\n"
                   urlTextLen +
                   MESSAGEBOX_MAXBUTTONS * (100 + 2 + 4) + // maximalni pocet znaku v tlacitku, co jsme ochotni resit
                   50;                                     // nejaka rezerva na "\r\n"
    if (CheckText != NULL)
        buffSize += 300 + 2 + (int)strlen(separator);

    char* buff = (char*)malloc(buffSize);
    if (buff == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    DWORD written = wsprintf(buff, "%s%s\r\n%s", separator, Title, separator);
    char* ptr = buff + written;

    // prelozime '\n' -> "\r\n"
    while (*text != 0)
    {
        if (*text == '\n')
            *ptr++ = '\r';
        *ptr++ = *text++;
    }

    if (URL != NULL)
    {
        ptr += wsprintf(ptr, "\r\n");
        if (URLText != NULL)
            ptr += wsprintf(ptr, "%s ", URLText);
        ptr += wsprintf(ptr, "%s", URL);
    }

    written = wsprintf(ptr, "\r\n%s", separator);
    ptr += written;

    // pripojime seznam tlacitek, odstranime '&'
    int i;
    for (i = 0; i < MESSAGEBOX_MAXBUTTONS; i++)
    {
        if (ButtonsID[i] != 0)
        {
            char btnText[100];
            GetDlgItemText(HWindow, ButtonsID[i], btnText, 100);
            btnText[99] = 0;
            RemoveAmpersands(btnText);

            written = wsprintf(ptr, "[%s]", btnText);
            ptr += written;
            if (i < MESSAGEBOX_MAXBUTTONS - 1 && ButtonsID[i + 1] != 0)
            {
                *ptr++ = ' ';
                *ptr++ = ' ';
                *ptr++ = ' ';
            }
        }
    }

    written = wsprintf(ptr, "\r\n%s", separator);
    ptr += written;

    // pokud existuje checkbox, pripojime jeho text (vyradime &)
    if (CheckText != NULL)
    {
        char chkText[300];
        GetDlgItemText(HWindow, IDS_MSGBOX_CHECK, chkText, 300);
        chkText[299] = 0;
        RemoveAmpersands(chkText);
        written = wsprintf(ptr, "[ ] %s\r\n%s", chkText, separator);
        BOOL checked = IsDlgButtonChecked(HWindow, IDS_MSGBOX_CHECK) == BST_CHECKED;
        if (checked)
            ptr[1] = 'x';
        ptr += written;
    }

    *ptr = 0; // terminator

    BOOL ret = CopyTextToClipboard(buff);
    free(buff);

    return ret;
}

INT_PTR
CMessageBox::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // odmaskujeme jednotlive casti flagu
        DWORD flagsType = Flags & MSGBOXEX_TYPEMASK;
        DWORD flagsIcon = Flags & MSGBOXEX_ICONMASK;
        DWORD flagsDef = Flags & MSGBOXEX_DEFMASK;
        DWORD flagsMode = Flags & MSGBOXEX_MODEMASK;
        DWORD flagsMisc = Flags & MSGBOXEX_MISCMASK;
        DWORD flagsEx = Flags & MSGBOXEX_EXMASK;

        if (!EscapeEnabled())
            EnableMenuItem(GetSystemMenu(HWindow, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

        // texty to titulku okna a do tela
        SetWindowText(HWindow, Title);
        if (Text.NeedTruncate())
            Text.TruncateText(GetDlgItem(HWindow, IDS_MSGBOX_TEXT), TRUE);
        SetDlgItemText(HWindow, IDS_MSGBOX_TEXT, Text.Get());

        const char* urlText = NULL;
        if (URL != NULL)
        {
            CHyperLink* hl = new CHyperLink(HWindow, IDS_MSGBOX_URL);
            hl->SetActionOpen(URL);
            urlText = URLText != NULL ? URLText : URL;
            SetDlgItemText(HWindow, IDS_MSGBOX_URL, urlText);
        }
        else
            DestroyWindow(GetDlgItem(HWindow, IDS_MSGBOX_URL));

        // prostor mezi tlacitky a vodorovnou linkou / spodkem dialogu
        RECT lineR;
        GetWindowRect(GetDlgItem(HWindow, IDC_MSGBOX_LINE), &lineR);
        RECT btnR;
        GetWindowRect(GetDlgItem(HWindow, IDC_MSGBOX_1), &btnR);
        int btnBottomMargin = lineR.top - btnR.bottom;

        // text pro check box
        int checkLineH = 0;
        BOOL hintVisible = FALSE;
        char* hintLabel = NULL;
        if (CheckText != NULL)
        {
            if (flagsEx & MSGBOXEX_HINT)
            {
                // probehneme CheckText, musime najit 2x'\t'
                // za prvnim '\t' je "klikaci" text, ktery zobrazime vedle checkboxu
                // za druhym '\t' je vlastni HINT, ktery se zobrazi po kliknuti
                hintLabel = CheckText;
                while (*hintLabel != '\t' && *hintLabel != 0)
                    hintLabel++;
                char* hintText = hintLabel;
                if (*hintText == '\t')
                    hintText++;
                while (*hintText != '\t' && *hintText != 0)
                    hintText++;
                if (hintLabel > CheckText && *hintLabel != 0 &&
                    hintText > CheckText && *hintText != 0 &&
                    hintText > hintLabel)
                {
                    *hintLabel = 0;
                    hintLabel++;
                    *hintText = 0;
                    hintText++;

                    CHyperLink* hl = new CHyperLink(HWindow, IDS_MSGBOX_HINT, STF_DOTUNDERLINE);
                    if (hl != NULL)
                    {
                        SetDlgItemText(HWindow, IDS_MSGBOX_HINT, hintLabel);
                        hl->SetActionShowHint(hintText);
                        hintVisible = TRUE;
                    }
                }
                else
                    TRACE_E("CMessageBox: MSGBOXEX_HINT was specified, but has CheckText invalid syntax.");
            }
            else
            {
                // TAB v checkboxu nema co delat (W2K zobrazi svislou carku, XP od TABu nezobrazi nic)
                char* p = CheckText;
                while (*p != '\t' && *p != 0)
                    p++;
                if (*p == '\t')
                {
                    TRACE_E("CMessageBox: CheckText contains TAB character while MSGBOXEX_HINT was not specified. Trimming.");
                    *p = 0;
                }
            }

            SetDlgItemText(HWindow, IDS_MSGBOX_CHECK, CheckText);
        }
        else
        {
            if (flagsEx & MSGBOXEX_HINT)
                TRACE_E("CMessageBox: MSGBOXEX_HINT has sense only if CheckText is specified");

            RECT r1;
            GetWindowRect(GetDlgItem(HWindow, IDC_MSGBOX_LINE), &r1);
            ScreenToClient(HWindow, (LPPOINT)&r1);
            RECT r2;
            GetClientRect(HWindow, &r2);
            checkLineH = r2.bottom - r1.top + 1;

            DestroyWindow(GetDlgItem(HWindow, IDC_MSGBOX_LINE));
            DestroyWindow(GetDlgItem(HWindow, IDS_MSGBOX_CHECK));
        }
        if (!hintVisible)
            DestroyWindow(GetDlgItem(HWindow, IDS_MSGBOX_HINT));

        // nastavime pozadovanou ikonu
        LPCTSTR iconID = 0;
        DWORD beepID = MB_OK;

        HICON hIcon;
        if (HOwnIcon != NULL)
        {
            if (flagsType == MSGBOXEX_YESNO || flagsType == MSGBOXEX_YESNOCANCEL)
                beepID = MB_ICONQUESTION;
            else
                beepID = MB_ICONASTERISK;
            hIcon = HOwnIcon;
        }
        else
        {
            switch (flagsIcon)
            {
            case MSGBOXEX_ICONHAND:
            {
                iconID = IDI_HAND;
                beepID = MB_ICONHAND;
                break;
            }

            case MSGBOXEX_ICONQUESTION:
            {
                iconID = IDI_QUESTION;
                beepID = MB_ICONQUESTION;
                break;
            }

            case MSGBOXEX_ICONEXCLAMATION:
            {
                iconID = IDI_EXCLAMATION;
                beepID = MB_ICONEXCLAMATION;
                break;
            }

            case MSGBOXEX_ICONINFORMATION:
            {
                iconID = IDI_INFORMATION;
                beepID = MB_ICONASTERISK;
                break;
            }
            }

            hIcon = NULL;
            if (iconID != 0)
                hIcon = SalLoadIcon(NULL, iconID, ICONSIZE_32);
        }

        int iconWidth = 0;
        int topMargin = 0;
        int iconHeight = 0;
        if (hIcon != NULL)
        {
            RECT r;
            GetWindowRect(GetDlgItem(HWindow, IDI_MSGBOX_ICON), &r);
            POINT p;
            p.x = r.left;
            p.y = r.top;
            ScreenToClient(HWindow, &p);
            topMargin = p.y;
            iconHeight = r.bottom - r.top;
            SendDlgItemMessage(HWindow, IDI_MSGBOX_ICON, STM_SETICON, (WPARAM)hIcon, 0);
        }
        else
        {
            RECT r1, r2;
            GetWindowRect(GetDlgItem(HWindow, IDS_MSGBOX_TEXT), &r1);
            POINT p;
            p.x = r1.left;
            p.y = r1.top;
            ScreenToClient(HWindow, &p);
            topMargin = p.y;
            GetWindowRect(GetDlgItem(HWindow, IDI_MSGBOX_ICON), &r2);
            iconWidth = r1.left - r2.left;
            DestroyWindow(GetDlgItem(HWindow, IDI_MSGBOX_ICON));
        }

        // vytahneme rect desktopu, kam budeme umisteni
        RECT clipRect;
        HWND hParent = Parent;
        if (hParent != NULL)
            hParent = GetTopVisibleParent(hParent);
        MultiMonGetClipRectByWindow(Parent, &clipRect, NULL);

        // namerime velikost textu
        HDC hDC = HANDLES(GetDC(HWindow));
        HFONT hOldFont = (HFONT)SelectObject(hDC, (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0));
        RECT tR;
        GetClientRect(GetDlgItem(HWindow, IDS_MSGBOX_TEXT), &tR);
        RECT textR = tR;
        RECT tRCheck = tR;
        RECT tRHint = tR;

        // test prumerne sirky znaku fontu
        const char* FONT_TEST_TEXT = "ABCDEabcde12345";
        RECT fontR = {0};
        DrawText(hDC, FONT_TEST_TEXT, -1, &fontR, DT_CALCRECT | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        int fontCharWidth = (fontR.right - fontR.left) / (int)strlen(FONT_TEST_TEXT);
        int fontCharHeight = fontR.bottom - fontR.top;

        // maximalni sirka vztazena k dekstopu, na kterem bude dialog zobrazen
        int maxTextWidth = (int)((clipRect.right - clipRect.left) / 1.8);
        if (maxTextWidth > fontCharWidth * 90) // od Windows Vista jsou dialogy zase uzsi - zalameme na 90 prumernych znacich
            maxTextWidth = fontCharWidth * 90;

        tR.right = maxTextWidth;
        DrawText(hDC, Text.Get(), -1, &tR, DT_CALCRECT | DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX);

        if (tR.right > maxTextWidth)
        {
            // sirka textu presahuje hranici maxTextWidth, vytvorime novy
            // text do ktereho vlozime tvrde konce radku
            const char* newText = DuplicateStrAndInsertEOLs(Text.Get(), hDC, maxTextWidth);
            if (newText != NULL)
            {
                tR.right = maxTextWidth;
                DrawText(hDC, newText, -1, &tR, DT_CALCRECT | DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX);
                SetDlgItemText(HWindow, IDS_MSGBOX_TEXT, newText);
                free((void*)newText);
            }
        }
        else
        {
            // zmensujeme sirku textu, dokud nesjou radky optimalne vypneny
            // casove trochu narocne, ale neni kam spechat
            RECT iterRect = tR;
            int goodRight;
            do
            {
                goodRight = iterRect.right;
                iterRect.right -= 5; // pojedeme po peti bodech
                DrawText(hDC, Text.Get(), -1, &iterRect, DT_CALCRECT | DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX);
                //        TRACE_I("Iter: right="<<iterRect.right);
            } while (iterRect.bottom == tR.bottom && iterRect.right < goodRight && iterRect.right >= 0);
            tR.right = goodRight;
        }

        // pokud je pod textem URL, pripocitame ho pro zjednoduseni k vysce textu
        RECT urlR = {0};
        if (urlText != NULL)
        {
            DrawText(hDC, urlText, -1, &urlR, DT_CALCRECT | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
            tR.bottom += urlR.bottom; // vyska url
        }

        BOOL multiline = tR.bottom > textR.bottom;
        if (CheckText != NULL)
        {
            DrawText(hDC, CheckText, -1, &tRCheck, DT_SINGLELINE | DT_CALCRECT | DT_LEFT | DT_EXPANDTABS);
            //tRCheck.right -= (3 * tRCheck.bottom) / 2;
        }
        else
            memset(&tRCheck, 0, sizeof(tRCheck));
        if (hintVisible)
            DrawText(hDC, hintLabel, -1, &tRHint, DT_SINGLELINE | DT_CALCRECT | DT_LEFT);
        else
            memset(&tRHint, 0, sizeof(tRHint));
        int hintWidth = tRHint.right - tRHint.left;
        SelectObject(hDC, hOldFont);
        HANDLES(ReleaseDC(HWindow, hDC));

        int deltaY = 0;
        if (tR.bottom > iconHeight)
            deltaY = tR.bottom - iconHeight;
        if (hIcon == NULL)
            deltaY -= 2 * topMargin;
        deltaY += fontCharHeight;

        // priradime tlacitka
        int btnText[MESSAGEBOX_MAXBUTTONS];
        int btnID[MESSAGEBOX_MAXBUTTONS];
        int btnCount = 0;
        switch (flagsType)
        {
        case MSGBOXEX_OKCANCEL:
        {
            btnText[btnCount] = IDS_BUTTON_OK;
            btnID[btnCount++] = DIALOG_OK;
            btnText[btnCount] = IDS_BUTTON_CANCEL;
            btnID[btnCount++] = DIALOG_CANCEL;
            break;
        }

        case MSGBOXEX_ABORTRETRYIGNORE:
        {
            btnText[btnCount] = IDS_BUTTON_ABORT;
            btnID[btnCount++] = DIALOG_ABORT;
            btnText[btnCount] = IDS_BUTTON_RETRY;
            btnID[btnCount++] = DIALOG_RETRY;
            btnText[btnCount] = IDS_BUTTON_IGNORE;
            btnID[btnCount++] = DIALOG_IGNORE;
            break;
        }

        case MSGBOXEX_YESNOCANCEL:
        {
            btnText[btnCount] = IDS_BUTTON_YES;
            btnID[btnCount++] = DIALOG_YES;
            btnText[btnCount] = IDS_BUTTON_NO;
            btnID[btnCount++] = DIALOG_NO;
            btnText[btnCount] = IDS_BUTTON_CANCEL;
            btnID[btnCount++] = DIALOG_CANCEL;
            break;
        }

        case MSGBOXEX_YESNO:
        {
            btnText[btnCount] = IDS_BUTTON_YES;
            btnID[btnCount++] = DIALOG_YES;
            btnText[btnCount] = IDS_BUTTON_NO;
            btnID[btnCount++] = DIALOG_NO;
            break;
        }

        case MSGBOXEX_RETRYCANCEL:
        {
            btnText[btnCount] = IDS_BUTTON_RETRY;
            btnID[btnCount++] = DIALOG_RETRY;
            btnText[btnCount] = IDS_BUTTON_CANCEL;
            btnID[btnCount++] = DIALOG_CANCEL;
            break;
        }

        case MSGBOXEX_CANCELTRYCONTINUE:
        {
            btnText[btnCount] = IDS_BUTTON_CANCEL;
            btnID[btnCount++] = DIALOG_CANCEL;
            btnText[btnCount] = IDS_BUTTON_TRY;
            btnID[btnCount++] = DIALOG_TRYAGAIN;
            btnText[btnCount] = IDS_BUTTON_CONTINUE;
            btnID[btnCount++] = DIALOG_CONTINUE;
            break;
        }

        case MSGBOXEX_CONTINUEABORT:
        {
            btnText[btnCount] = IDS_BUTTON_CONTINUE;
            btnID[btnCount++] = DIALOG_CONTINUE;
            btnText[btnCount] = IDS_BUTTON_ABORT;
            btnID[btnCount++] = DIALOG_ABORT;
            break;
        }

        case MSGBOXEX_YESNOOKCANCEL:
        {
            btnText[btnCount] = IDS_BUTTON_YES;
            btnID[btnCount++] = DIALOG_YES;
            btnText[btnCount] = IDS_BUTTON_NO;
            btnID[btnCount++] = DIALOG_NO;
            btnText[btnCount] = IDS_BUTTON_OK;
            btnID[btnCount++] = DIALOG_OK;
            btnText[btnCount] = IDS_BUTTON_CANCEL;
            btnID[btnCount++] = DIALOG_CANCEL;
            break;
        }

        default: //case MSGBOXEX_OK:
        {
            // pokud k nam propadne neznamy flag, budeme se tvarit jako MSGBOXEX_OK varianta
            if (flagsType != MSGBOXEX_OK)
                TRACE_E("CMessageBox: uknown flags: " << Flags);

            btnText[btnCount] = IDS_BUTTON_OK;
            btnID[btnCount++] = DIALOG_OK;
            break;
        }
        }
        if (flagsMisc & MSGBOXEX_HELP)
        {
            if (flagsType == MSGBOXEX_YESNOOKCANCEL)
            {
                TRACE_E("Invalid flags combination, message box can have only 4 buttons.");
            }
            else
            {
                btnText[btnCount] = IDS_BUTTON_HELP;
                btnID[btnCount++] = IDHELP;
            }
        }

        // detekce rozmeru tlacitka a mezitlacitkove mezery
        RECT buttonR1;
        GetWindowRect(GetDlgItem(HWindow, IDC_MSGBOX_1), &buttonR1);
        RECT buttonR2;
        GetWindowRect(GetDlgItem(HWindow, IDC_MSGBOX_2), &buttonR2);
        int btnWidth = buttonR1.right - buttonR1.left;
        int btnHeight = buttonR1.bottom - buttonR1.top;
        int btnMargin = buttonR2.left - buttonR1.right;
        POINT p;
        p.x = buttonR1.left;
        p.y = buttonR1.top;
        ScreenToClient(HWindow, &p);
        int btnY = p.y + deltaY;

        // nastavime tlacitka (IDcka, texty, viditelnost)
        int origBtnID[MESSAGEBOX_MAXBUTTONS] = {IDC_MSGBOX_1, IDC_MSGBOX_2, IDC_MSGBOX_3, IDC_MSGBOX_4};
        HWND origBtnWnd[MESSAGEBOX_MAXBUTTONS];
        int btnAddedWidth = 0;
        int i;
        for (i = 0; i < MESSAGEBOX_MAXBUTTONS; i++)
        {
            HWND hButton = GetDlgItem(HWindow, origBtnID[i]);
            origBtnWnd[i] = hButton;
            if (i < btnCount)
            {
                // priradime ID
                ButtonsID[i] = btnID[i];
                SetWindowLongPtr(hButton, GWLP_ID, btnID[i]);

                // priradime text
                BOOL btnTextWasSet = FALSE;
                if (AliasBtnNames != NULL) // alias button names
                {
                    char tmpBuff[1000];
                    char seps[] = "\t";
                    if (i == 0 && strlen(AliasBtnNames) > 999)
                        TRACE_E("AliasBtnNames is too long");
                    lstrcpyn(tmpBuff, AliasBtnNames, 1000);

                    char* aliasID = strtok(tmpBuff, seps);
                    char* aliasName = strtok(NULL, seps);
                    while (aliasID != NULL && aliasName != NULL)
                    {
                        int id = atoi(aliasID);
                        if (btnID[i] == id)
                        {
                            btnTextWasSet = TRUE;
                            SetWindowText(hButton, aliasName);

                            // omerime jestli neni potreba rozsirit tlacitko
                            char btnText2[300];
                            lstrcpyn(btnText2, aliasName, 300);
                            RemoveAmpersands(btnText2);
                            HFONT hFont = (HFONT)SendMessage(hButton, WM_GETFONT, 0, 0);
                            SIZE sz;
                            HDC hDC2 = HANDLES(GetDC(HWindow));
                            HFONT hOldFont2 = (HFONT)SelectObject(hDC2, hFont);
                            GetTextExtentPoint32(hDC2, btnText2, (int)strlen(btnText2), &sz);
                            SelectObject(hDC2, hOldFont2);
                            HANDLES(ReleaseDC(HWindow, hDC2));

                            int neededBtnWidth = sz.cx + btnWidth / 2;
                            if (neededBtnWidth > btnWidth)
                            {
                                btnAddedWidth += neededBtnWidth - btnWidth;
                                SetWindowPos(hButton, NULL, 0, 0, neededBtnWidth, btnHeight,
                                             SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
                            }
                            break;
                        }
                        aliasID = strtok(NULL, seps);
                        aliasName = strtok(NULL, seps);
                    }
                }
                if (!btnTextWasSet)
                    SetWindowText(hButton, LoadStr(btnText[i]));

                // nechame si posilat Ctrl+C v podobe WM_COPY
                CKeyForwarderWindow* wnd = new CKeyForwarderWindow(HWindow, btnID[i]);
                if (wnd != NULL && wnd->HWindow == NULL)
                    delete wnd; // nepodarilo se attachnout - nedealokuje se samo
            }
            else
            {
                ButtonsID[i] = 0;
                // nepotrebna tlacitka vyhodime
                DestroyWindow(hButton);
            }
        }

        // celkova sirka tlacitek a mezitlacitkovych mezer
        int totalBtnWidth = btnWidth * btnCount + btnMargin * (btnCount - 1) + btnAddedWidth;

        // upravime rozmery dialogu
        RECT windowR;
        GetWindowRect(HWindow, &windowR);
        int width = windowR.right - windowR.left;
        int height = windowR.bottom - windowR.top - checkLineH;

        RECT clientR;
        GetClientRect(HWindow, &clientR);

        // uprava sirky dialogu: musi se tam vejit text, checkbox a tlacitka s okrajema
        int checkAndHint = tRCheck.right;
        if (hintVisible)
            checkAndHint += hintWidth;
        int deltaX = max(tR.right, checkAndHint) - textR.right - iconWidth;

        if (clientR.right + deltaX < totalBtnWidth + 2 * btnBottomMargin)
            deltaX = totalBtnWidth + 2 * btnBottomMargin - clientR.right;

        // zmensime vysku textu
        GetWindowRect(GetDlgItem(HWindow, IDS_MSGBOX_TEXT), &textR);
        p.x = textR.left;
        ScreenToClient(HWindow, &p);
        // text vertikalne je centrovan k ikone
        p.y = topMargin + (iconHeight - tR.bottom - tR.top) / 2;
        // ale nesmi vylezt nad ni
        if (p.y < topMargin)
            p.y = topMargin;
        SetWindowPos(GetDlgItem(HWindow, IDS_MSGBOX_TEXT), NULL, p.x - iconWidth, p.y,
                     tR.right - tR.left, tR.bottom - tR.top,
                     SWP_NOZORDER);

        if (urlText != NULL)
        {
            SetWindowPos(GetDlgItem(HWindow, IDS_MSGBOX_URL), NULL, p.x - iconWidth, p.y + tR.bottom - tR.top - (urlR.bottom - urlR.top),
                         tR.right - tR.left, urlR.bottom - urlR.top, SWP_NOZORDER);
        }

        // vlastni dialog
        SetWindowPos(HWindow, NULL, 0, 0, width + deltaX, height + deltaY,
                     SWP_NOZORDER | SWP_NOMOVE);

        GetClientRect(HWindow, &clientR);

        // rozmistime tlacitka
        int x = (clientR.right - totalBtnWidth) / 2;
        if (WindowsVistaAndLater)
            x = clientR.right - totalBtnWidth - btnBottomMargin;
        BackgroundSeparator = btnY - btnBottomMargin + 1;
        for (i = 0; i < btnCount; i++)
        {
            SetWindowPos(origBtnWnd[i], NULL, x, btnY, 0, 0,
                         SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);

            RECT buttonRect;
            GetWindowRect(origBtnWnd[i], &buttonRect);
            x += (buttonRect.right - buttonRect.left) + btnMargin;
        }

        int defIndex = 0;
        switch (flagsDef)
        {
        case MSGBOXEX_DEFBUTTON2:
            defIndex = 1;
            break;
        case MSGBOXEX_DEFBUTTON3:
            defIndex = 2;
            break;
        case MSGBOXEX_DEFBUTTON4:
            defIndex = 3;
            break;
        default:
        {
            if (flagsDef != MSGBOXEX_DEFBUTTON1)
                TRACE_E("CMessageBox: uknown flags: " << Flags);
            defIndex = 0;
        }
        }
        SendMessage(HWindow, DM_SETDEFID, btnID[defIndex], 0);
        SetFocus(GetDlgItem(HWindow, btnID[defIndex]));

// casem az to bude v includech, vymazat nasledujici definice...
#define BCM_FIRST 0x1600 // Button control messages
#define BCM_SETSHIELD (BCM_FIRST + 0x000C)

        if (WindowsVistaAndLater && (flagsEx & MSGBOXEX_SHIELDONDEFBTN))
        {
            SendMessage(GetDlgItem(HWindow, btnID[defIndex]), BCM_SETSHIELD, 0, TRUE);
        }

        // posuneme checkbox a caru
        if (CheckText != NULL)
        {
            OffsetChildWindow(HWindow, IDC_MSGBOX_LINE, 0, deltaY);
            OffsetChildWindow(HWindow, IDS_MSGBOX_CHECK, 0, deltaY);

            RECT r;
            GetWindowRect(GetDlgItem(HWindow, IDC_MSGBOX_LINE), &r);
            p.x = r.left;
            p.y = r.top;
            ScreenToClient(HWindow, &p);
            SetWindowPos(GetDlgItem(HWindow, IDC_MSGBOX_LINE), NULL, p.x, p.y,
                         clientR.right - 2 * p.x, r.bottom - r.top, SWP_NOZORDER);

            GetWindowRect(GetDlgItem(HWindow, IDS_MSGBOX_CHECK), &r);
            p.x = r.left;
            p.y = r.top;
            ScreenToClient(HWindow, &p);
            int checkBoxHeight = r.bottom - r.top;
            SetWindowPos(GetDlgItem(HWindow, IDS_MSGBOX_CHECK), NULL, p.x, p.y,
                         tRCheck.right + 3 * p.x, r.bottom - r.top, SWP_NOZORDER);

            // nechame si posilat Ctrl+C v podobe WM_COPY
            CKeyForwarderWindow* wnd = new CKeyForwarderWindow(HWindow, IDS_MSGBOX_CHECK);
            if (wnd != NULL && wnd->HWindow == NULL)
                delete wnd; // nepodarilo se attachnout - nedealokuje se samo

            if (hintVisible)
            {
                GetWindowRect(GetDlgItem(HWindow, IDS_MSGBOX_HINT), &r);
                int hintHeight = r.bottom - r.top;
                SetWindowPos(GetDlgItem(HWindow, IDS_MSGBOX_HINT), NULL,
                             clientR.right - hintWidth - p.x, p.y + (checkBoxHeight - hintHeight) / 2,
                             hintWidth, hintHeight, SWP_NOZORDER);
            }
        }

        //      MessageBox(Parent, Text.Get(), Title, Flags);  // experimental

        // pipneme - jako pravy message box
        if ((Flags & MSGBOXEX_SILENT) == 0)
            MessageBeep(beepID);

        // nechame vycentrovat
        CCommonDialog::DialogProc(uMsg, wParam, lParam);

        // pod W2K pri spousteni pres shortcut s MAXIMIZED nastavenim
        // se dialog zobrazoval maximalizovany; SC_RESTORE na to zabira
        SendMessage(HWindow, WM_SYSCOMMAND, SC_RESTORE, 0);

        if (Flags & MB_TASKMODAL)
            TRACE_E("MB_TASKMODAL flags is not implemented.");

        if ((Flags & MB_SYSTEMMODAL) || (Flags & MB_TOPMOST))
            SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

        if (Flags & MSGBOXEX_SETFOREGROUND)
            SetForegroundWindow(HWindow);

        return FALSE; // prevent the system from setting the default keyboard focus
    }

    case WM_ERASEBKGND:
    {
        if (WindowsVistaAndLater)
        {
            HDC hDC = (HDC)wParam;
            RECT r;
            GetClientRect(HWindow, &r);
            RECT rOrig = r;
            int ySeparator = BackgroundSeparator;
            r.bottom = ySeparator;
            FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
            r = rOrig;
            r.top = ySeparator;
            r.bottom = r.top + 1;
            FillRect(hDC, &r, (HBRUSH)(COLOR_3DLIGHT + 1));
            r = rOrig;
            r.top = ySeparator + 1;
            FillRect(hDC, &r, (HBRUSH)(COLOR_BTNFACE + 1));
            return TRUE;
        }
        else
            break;
    }

    case WM_CTLCOLORSTATIC:
    {
        if (WindowsVistaAndLater)
        {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndStatic = (HWND)lParam;
            int resID = GetWindowLong(hwndStatic, GWL_ID);
            if (resID == IDI_MSGBOX_ICON || resID == IDS_MSGBOX_TEXT || resID == IDS_MSGBOX_URL)
            {
                COLORREF textClr = GetSysColor(COLOR_WINDOWTEXT);
                SetTextColor(hdcStatic, textClr);
                SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
                return (INT_PTR)(HBRUSH)(COLOR_WINDOW + 1);
            }
            break;
        }
        else
            break;
    }

    case WM_HELP:
    {
        if (Flags & MSGBOXEX_HELP)
            DialogProc(WM_COMMAND, IDHELP, 0);
        return TRUE;
    }

    case WM_COPY:
    {
        CopyToClipboard();
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDHELP:
        {
            HELPINFO hi;
            memset(&hi, 0, sizeof(hi));
            hi.cbSize = sizeof(hi);
            hi.iContextType = HELPINFO_WINDOW;
            hi.dwContextId = HelpID;
            GetCursorPos(&hi.MousePos);
            // pokud mame callback, volame ho
            if (HelpCallback != NULL)
                HelpCallback(&hi);
            else
            { // jinak posleme WM_HELP
                if (Parent != NULL)
                    SendMessage(Parent, WM_HELP, 0, (LPARAM)&hi);
                else
                    TRACE_E("CMessageBox::DialogProc(): received IDHELP: unable to send WM_HELP to parent (this message-box has no parent)!");
            }
            return TRUE;
        }

        // pouze tato tlacitka zaviraji dialog (chodi sem i kliknuti na checkbox)
        case DIALOG_OK:
        case DIALOG_CANCEL:
        case DIALOG_ABORT:
        case DIALOG_RETRY:
        case DIALOG_IGNORE:
        case DIALOG_YES:
        case DIALOG_NO:
        case DIALOG_TRYAGAIN:
        case DIALOG_CONTINUE:
        {
            if (LOWORD(wParam) == DIALOG_CANCEL && !EscapeEnabled()) // blokujeme VK_ESCAPE pro nektere kombinace tlacitek
                return TRUE;

            TransferData(ttDataFromWindow); // vnutime transfer i pri DIALOG_CANCEL (MSDN: If users select the option and click Cancel, this option does take effect. This setting is a meta-option, so it doesn't follow the standard Cancel behavior of leaving no side effect.)
            if (Modal)
            {
                // message box typu MSGBOXEX_OK a MSGBOXEX_YESNO nesmi vratit CANCEL (naopak MSGBOXEX_ABORTRETRYIGNORE vraci CANCEL)
                if (LOWORD(wParam) == DIALOG_CANCEL)
                {
                    DWORD flagsType = Flags & MSGBOXEX_TYPEMASK;
                    if (flagsType == MSGBOXEX_OK)
                        wParam = MAKELPARAM(DIALOG_OK, HIWORD(wParam)); // cancel -> ok
                    if (flagsType == MSGBOXEX_YESNO)
                        wParam = MAKELPARAM(DIALOG_NO, HIWORD(wParam)); // cancel -> no
                }
                EndDialog(HWindow, wParam);
            }
            else
            {
                TRACE_E("CMessageBox is not modal!");
                DestroyWindow(HWindow);
            }
            return TRUE;
        }
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// SalMessageBox / SalMessageBoxEx
//

int SalMessageBox(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
    CALL_STACK_MESSAGE5("SalMessageBox(0x%p, %s, %s, 0x%X)", hParent, lpText, lpCaption, uType);

    // omezeno, ktere SalMessageBox nezvladne
    if (uType & MSGBOXEX_HELP)
    {
        TRACE_E("SalMessageBox: use SalMessageBoxEx with MSGBOXEX_HELP flag");
        uType &= ~MSGBOXEX_HELP;
    }

    // prevedeme volani na SalMessageBoxEx
    MSGBOXEX_PARAMS params;
    memset(&params, 0, sizeof(params));
    params.HParent = hParent;
    params.Text = lpText;
    params.Caption = lpCaption;
    params.Flags = uType;
    return SalMessageBoxEx(&params);
}

int SalMessageBoxEx(const MSGBOXEX_PARAMS* params)
{
    return CMessageBox(params->HParent,
                       params->Flags,
                       params->Caption,
                       params->Text,
                       params->CheckBoxText,
                       params->CheckBoxValue,
                       params->HIcon,
                       params->ContextHelpId,
                       params->HelpCallback,
                       params->AliasBtnNames,
                       params->URL,
                       params->URLText)
        .Execute();
}
