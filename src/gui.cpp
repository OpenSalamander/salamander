// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "svg.h"
#include "gui.h"
#include "toolbar.h"
#include "menu.h"
#include "tooltip.h"
#include <uxtheme.h>
#include <vssym32.h>

#include "nanosvg\nanosvg.h"
#include "nanosvg\nanosvgrast.h"

#include "mainwnd.h"

//****************************************************************************
//
// CGuiBitmap
//

// podpora pro nablble kresleni disabled varianty tlacikta
class CGuiBitmap : public CBitmap
{
public:
    HBITMAP CreateCopyBitmap()
    {
        CALL_STACK_MESSAGE1("CSharedBitmapAndDC::CreateCopyBitmap()");
        HDC hdc = HANDLES(GetDC(NULL));

        HBITMAP HCopyBitmap = HANDLES(CreateCompatibleBitmap(hdc, Width, Height));

        HDC hMemDC = HANDLES(CreateCompatibleDC(hdc));
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, HCopyBitmap);

        HBITMAP hOld = (HBITMAP)SelectObject(HMemDC, HOldBmp);

        HIMAGELIST hImageList = ImageList_Create(Width, Height, ILC_MASK | GetImageListColorFlags(), 1, 0);
        ImageList_AddMasked(hImageList, HBmp, GetSysColor(COLOR_BTNFACE)); // j.r. zde byla barva natvrdo 192,192,192, coz zlobilo pod XPlookem
        RECT r;
        r.left = 0;
        r.top = 0;
        r.right = Width;
        r.bottom = Height;
        FillRect(hMemDC, &r, (HBRUSH)HANDLES(GetStockObject(WHITE_BRUSH)));
        ImageList_Draw(hImageList, 0, hMemDC, 0, 0, ILD_TRANSPARENT);
        ImageList_Destroy(hImageList);

        SelectObject(HMemDC, hOld);

        SelectObject(hMemDC, hOldBitmap);
        HANDLES(DeleteDC(hMemDC));

        HANDLES(ReleaseDC(NULL, hdc));
        return HCopyBitmap;
    }
};

//****************************************************************************
//
// CProgressBar
//

CProgressBar::CProgressBar(HWND hDlg, int ctrlID)
    : CWindow(hDlg, ctrlID, ooAllocated)
{
    if (HWindow != NULL)
    {
        RECT r;
        GetClientRect(HWindow, &r);
        Width = r.right - r.left;
        Height = r.bottom - r.top;
    }
    else
    {
        Width = 10;
        Height = 10;
    }

    Progress = 0;
    SelfMoveTime = 0xFFFFFFFF; // po zavolani SetProgress(-1) se obdelnik bude posouvat bez omezeni
    SelfMoveTicks = 0;
    SelfMoveSpeed = 50; // 20 pohybu za vterinu
    TimerIsRunning = FALSE;
    Bitmap = new CBitmap();
    if (Bitmap != NULL)
    {
        HDC hDC = HANDLES(GetDC(NULL));
        if (!Bitmap->CreateBmp(hDC, Width, Height))
        {
            delete Bitmap;
            Bitmap = NULL;
        }
        HANDLES(ReleaseDC(NULL, hDC));
    }
    Text = NULL;

    // default font vytahneme z dialogu
    HFont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
    if (HFont == NULL)
        HFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT); // pouziva systemovy font, tak si ho vytahneme ze systemu
}

CProgressBar::~CProgressBar()
{
    Stop();
    if (Bitmap != NULL)
        delete (Bitmap);
    if (Text != NULL)
        free(Text);
}

void CProgressBar::SetProgress(DWORD progress, const char* text)
{
    // misto primeho volani pouzijeme SendMessage pro prekonani hranice threadu
    SendMessage(HWindow, WM_USER_SETPROGRESS, progress, (LPARAM)text);
}

void CProgressBar::SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal, const char* text)
{
    // muze se stat, ze progressTotal je 1 a progressCurrent je velke cislo, pak je vypocet
    // nesmyslny (navic pada diky RTC) a je treba explicitne zadat 0% nebo 100% (hodnotu 1000)
    SetProgress(progressCurrent >= progressTotal ? (progressTotal.Value == 0 ? 0 : 1000) : (DWORD)((progressCurrent * CQuadWord(1000, 0)) / progressTotal).Value,
                text);
}

void CProgressBar::SetSelfMoveTime(DWORD time)
{
    SelfMoveTime = time;
}

void CProgressBar::SetSelfMoveSpeed(DWORD moveTime)
{
    SelfMoveSpeed = moveTime;
    if (TimerIsRunning)
    {
        KillTimer(HWindow, IDT_PROGRESSSELFMOVE);
        SetTimer(HWindow, IDT_PROGRESSSELFMOVE, SelfMoveSpeed, NULL);
    }
}

void CProgressBar::Stop()
{
    if (TimerIsRunning)
    {
        KillTimer(HWindow, IDT_PROGRESSSELFMOVE);
        TimerIsRunning = FALSE;
    }
}

void CProgressBar::Paint(HDC hDC)
{
    BOOL releaseDC = FALSE;
    if (hDC == NULL)
    {
        hDC = HANDLES(GetDC(HWindow));
        releaseDC = TRUE;
    }

    // pokud mame bitmapu, pojedeme pres cache
    HDC hMemDC = NULL;
    if (Bitmap != NULL && Progress != -1) // Progress==-1 je zbytecne cachovat, nema tam co blikat
        hMemDC = Bitmap->HMemDC;
    else
        hMemDC = hDC;

    if (Progress == -1)
    {
        // neurcity rezim: bila, modry obdelnicek, bila
        RECT r;
        r.top = 1;
        r.bottom = Height - 1;

        int mid = BarX + 1;
        int midW = Height * 2;

        SelectObject(hMemDC, HFont);

        COLORREF oldBkColor = SetBkColor(hMemDC, GetCOLORREF(CurrentColors[PROGRESS_BK_NORMAL]));

        r.left = 1;
        r.right = mid - midW;
        if (r.left < 1)
            r.left = 1;
        if (r.left > Width - 1)
            r.left = Width - 1;
        if (r.right < 1)
            r.right = 1;
        if (r.right > Width - 1)
            r.right = Width - 1;
        ExtTextOut(hMemDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);

        SetBkColor(hMemDC, GetCOLORREF(CurrentColors[PROGRESS_BK_SELECTED]));
        r.left = 1 + mid - midW;
        r.right = mid + midW;
        if (r.left < 1)
            r.left = 1;
        if (r.left > Width - 1)
            r.left = Width - 1;
        if (r.right < 1)
            r.right = 1;
        if (r.right > Width - 1)
            r.right = Width - 1;
        ExtTextOut(hMemDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);

        SetBkColor(hMemDC, GetCOLORREF(CurrentColors[PROGRESS_BK_NORMAL]));
        r.left = 1 + mid + midW;
        r.right = Width - 1;
        if (r.left < 1)
            r.left = 1;
        if (r.left > Width - 1)
            r.left = Width - 1;
        if (r.right < 1)
            r.right = 1;
        if (r.right > Width - 1)
            r.right = Width - 1;
        ExtTextOut(hMemDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);

        SetBkColor(hMemDC, oldBkColor);
    }
    else
    {
        // pripravime a omerime retezec
        char buff[50];

        char* progress;
        int progressLen;

        if (Text != NULL)
        {
            progress = Text;
            progressLen = (int)strlen(progress);
        }
        else
        {
            progress = buff;
            progressLen = sprintf(progress, "%d %%", (int)((Progress /*+ 5*/) / 10)); // nezaokrouhlujeme progres, protoze jinak je 100% videt od 99,5%-100%, coz nektere uzivatele rozciluje (markatni u FTP, kde to muze byt treba i na pul minuty)
        }

        SIZE sz;
        GetTextExtentPoint32(hMemDC, progress, progressLen, &sz);

        // pozice textu -- centorvane v obou osach
        int x = (Width - sz.cx) / 2;
        int y = (Height - sz.cy) / 2;

        // leva cast progressu (SELECTED)
        RECT r;
        r.left = 1;
        r.right = 1 + (Width - 2) * Progress / 1000;
        r.top = 1;
        r.bottom = Height - 1;

        SelectObject(hMemDC, HFont);

        COLORREF oldTextColor = SetTextColor(hMemDC, GetCOLORREF(CurrentColors[PROGRESS_FG_SELECTED]));
        COLORREF oldBkColor = SetBkColor(hMemDC, GetCOLORREF(CurrentColors[PROGRESS_BK_SELECTED]));
        ExtTextOut(hMemDC, x, y, ETO_OPAQUE | ETO_CLIPPED, &r, progress, progressLen, NULL);

        // prava cast progressu (NORMAL)
        r.left = r.right;
        r.right = Width - 1;

        SetTextColor(hMemDC, GetCOLORREF(CurrentColors[PROGRESS_FG_NORMAL]));
        SetBkColor(hMemDC, GetCOLORREF(CurrentColors[PROGRESS_BK_NORMAL]));
        ExtTextOut(hMemDC, x, y, ETO_OPAQUE | ETO_CLIPPED, &r, progress, progressLen, NULL);
        SetTextColor(hMemDC, oldTextColor);
        SetBkColor(hMemDC, oldBkColor);
    }

    HPEN hOldPen = (HPEN)SelectObject(hMemDC, BtnShadowPen);
    MoveToEx(hMemDC, 0, 0, NULL);
    LineTo(hMemDC, Width - 1, 0);
    LineTo(hMemDC, Width - 1, Height - 1);
    LineTo(hMemDC, 0, Height - 1);
    LineTo(hMemDC, 0, 0);
    SelectObject(hMemDC, hOldPen);

    // pokud jedeme pres cache, posleme ji do obrazovky
    if (Bitmap != NULL && hMemDC != hDC)
        BitBlt(hDC, 0, 0, Width, Height, hMemDC, 0, 0, SRCCOPY);

    if (releaseDC)
        HANDLES(ReleaseDC(HWindow, hDC));
}

void CProgressBar::MoveBar()
{
    if (Progress != -1)
    {
        // nastartujeme pohyb
        Progress = -1;
        BarX = 0;
        MoveBarRight = TRUE;
    }
    else
    {
        if (MoveBarRight)
        {
            BarX += 4;
            if (BarX > Width - 2)
            {
                BarX = Width - 2;
                MoveBarRight = !MoveBarRight;
            }
        }
        else
        {
            BarX -= 4;
            if (BarX < 0)
            {
                BarX = 0;
                MoveBarRight = !MoveBarRight;
            }
        }
    }
}

LRESULT
CProgressBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CProgressBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_TIMER:
    {
        if (wParam == IDT_PROGRESSSELFMOVE)
        {
            if ((SelfMoveTime != 0xFFFFFFFF) && (GetTickCount() - SelfMoveTicks > SelfMoveTime))
            {
                KillTimer(HWindow, IDT_PROGRESSSELFMOVE);
                TimerIsRunning = FALSE;
            }
            else
            {
                MoveBar();
                Paint(NULL);
            }
            return 0;
        }
        break;
    }

    case WM_USER_SETPROGRESS:
    {
        DWORD progress = (DWORD)wParam;
        const char* text = (const char*)lParam;

        BOOL paint = TRUE;
        BOOL textChanged = FALSE;

        if ((text != NULL || Text != NULL) &&
            (text == NULL || Text == NULL || strcmp(text, Text) != 0))
        {
            textChanged = TRUE;
            if (Text != NULL)
            {
                free(Text);
                Text = NULL;
            }
            if (text != NULL)
            {
                Text = DupStr(text);
                if (Text == NULL)
                    TRACE_E(LOW_MEMORY);
            }
        }

        if (progress == (DWORD)-1)
        {
            if (SelfMoveTime > 0)
            {
                SelfMoveTicks = GetTickCount();
                if (!TimerIsRunning)
                {
                    SetTimer(HWindow, IDT_PROGRESSSELFMOVE, SelfMoveSpeed, NULL);
                    TimerIsRunning = TRUE;
                    MoveBar();
                }
                else
                    paint = FALSE; // ke zmene dojde az na timer, nyni nema smysl prekreslovat
            }
            else
                MoveBar();
        }
        else
        {
            if (TimerIsRunning)
                Stop();
            if (progress > 1000)
                progress = 1000; // max. 100% (kopie "aktivniho" souboru muze hlasit progress >100%)
            if (progress != Progress)
                Progress = progress;
            else
                paint = textChanged; // nedoslo ke zmene progresu, pokud nedoslo ani ke zmene textu, nebude se prekreslovat
                                     /*
        BOOL redraw = Progress == 0 ||                     // 0% zobrazime vzdycky
                      Progress == 1000 ||                  // 100% zobrazime vzdycky
                      Progress - DisplayedProgress >= 100; // zmenu o vice nez 10% zobrazime taky vzdycky
        if (redraw && Progress != DisplayedProgress)
          Paint(NULL);
        */
        }
        if (paint)
            Paint(NULL);
        return 0;
    }

    case WM_SIZE:
    {
        Width = LOWORD(lParam);
        Height = HIWORD(lParam);
        Bitmap->Enlarge(Width, Height);
        return 0;
    }

    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        Paint(hDC);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CStaticText
//

CStaticText::CStaticText(HWND hDlg, int ctrlID, DWORD flags)
    : CWindow(hDlg, ctrlID, ooAllocated)
{
    if ((flags & STF_HANDLEPREFIX) && ((flags & STF_END_ELLIPSIS) || (flags & STF_PATH_ELLIPSIS)))
    {
        TRACE_E("Flag STF_HANDLEPREFIX cannot be used with STF_END_ELLIPSIS or STF_PATH_ELLIPSIS.");
        flags &= ~STF_HANDLEPREFIX;
    }

    Flags = flags;
    Text = NULL;
    TextLen = 0;
    Text2 = NULL;
    Text2Len = 0;
    AlpDX = NULL;
    Allocated = 0;
    Bitmap = NULL;
    HFont = NULL;
    DestroyFont = FALSE;
    ClipDraw = FALSE;
    Text2Draw = FALSE;
    Alignment = 0; // left
    PathSeparator = '\\';
    MouseIsTracked = FALSE;
    ToolTipText = NULL;
    HToolTipNW = NULL;
    ToolTipID = 0;
    HintMode = FALSE;

    if (HWindow == NULL)
        return; // at neblikame obrazovkou

    UIState = (WORD)SendMessage(HWindow, WM_QUERYUISTATE, 0, 0);

    // vytahneme zarovnani
    DWORD style = (DWORD)GetWindowLongPtr(HWindow, GWL_STYLE);
    if (style & SS_RIGHT)
        Alignment = 2;
    else if (style & SS_CENTER)
        Alignment = 1;

    // omerime maximalni rozmer staticu
    RECT r;
    GetClientRect(HWindow, &r);
    Width = r.right - r.left;
    Height = r.bottom - r.top;

    // pokud mame kreslit pres cache, vytvorime bitmapu
    if (Flags & STF_CACHED_PAINT)
    {
        Bitmap = new CBitmap(); // pokud alokace nedopadne, paint nebude cachovany
        if (Bitmap != NULL)
        {
            HDC hDC = HANDLES(GetDC(HWindow));
            if (!Bitmap->CreateBmp(hDC, Width, Height))
            {
                delete Bitmap;
                Bitmap = NULL;
            }
            HANDLES(ReleaseDC(HWindow, hDC));
        }
    }

    // default font vytahneme z controlu
    HFont = (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0);
    if ((Flags & STF_BOLD) || (Flags & STF_UNDERLINE))
    {
        // pokud je text BOLD nebo UNDERLINE, pripravime si vlastni font
        LOGFONT lf;
        GetObject(HFont, sizeof(lf), &lf);
        if (Flags & STF_BOLD)
            lf.lfWeight = FW_BOLD;
        if (Flags & STF_UNDERLINE)
            lf.lfUnderline = TRUE;
        HFont = HANDLES(CreateFontIndirect(&lf));
        DestroyFont = TRUE;
    }

    // vytahneme uvodni text staticu
    char buff[4096];
    CWindow::WindowProc(WM_GETTEXT, 4096, (LPARAM)buff);
    buff[4095] = 0; // pro jistotu...
    if (buff[0] != 0)
        SetText(buff);
}

CStaticText::~CStaticText()
{
    if (ToolTipText != NULL)
        free(ToolTipText);
    if (Text != NULL)
        free(Text);
    if (Text2 != NULL)
        free(Text2);
    if (AlpDX != NULL)
        free(AlpDX);
    if (Bitmap != NULL)
        delete Bitmap;
    if (HFont != NULL && DestroyFont)
        HANDLES(DeleteObject(HFont));
}

// zamezuje cetnym realokacim pri postupnem alokovani vetsich a vetsich retezcu
#define ST_ALLOC_GRANULARITY 20

BOOL CStaticText::SetText(const char* text)
{
    CALL_STACK_MESSAGE2("CStaticText::SetText(%s)", text);

    if (text == NULL)
        text = "";
    if (Text != NULL && strcmp(Text, text) == 0)
        return TRUE;

    int l = (int)strlen(text) + 1;
    if (Allocated < l)
    {
        char* newText = (char*)realloc(Text, l + ST_ALLOC_GRANULARITY);
        if (newText == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        if (Flags & (STF_PATH_ELLIPSIS | STF_END_ELLIPSIS))
        {
            int* newAlpDX = (int*)realloc(AlpDX, (l + ST_ALLOC_GRANULARITY) * sizeof(int));
            if (newAlpDX == NULL)
            {
                TRACE_E(LOW_MEMORY);
                free(newText);
                return FALSE;
            }
            char* newText2 = (char*)realloc(Text2, l + ST_ALLOC_GRANULARITY + 3); // 3: prostor pro "..." (muzu ubrat W a pridat "...")
            if (newText2 == NULL)
            {
                TRACE_E(LOW_MEMORY);
                free(newText);
                free(newAlpDX);
                return FALSE;
            }
            AlpDX = newAlpDX;
            Text2 = newText2;
        }
        Text = newText;
        Allocated = l + ST_ALLOC_GRANULARITY;
    }
    memmove(Text, text, l);
    TextLen = l - 1;

    PrepareForPaint();

    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
    return TRUE;
}

BOOL CStaticText::SetTextToDblQuotesIfNeeded(const char* text)
{
    CALL_STACK_MESSAGE2("CStaticText::SetTextToDblQuotesIfNeeded(%s)", text);

    if (text != NULL)
    {
        int len = (int)strlen(text);
        if (len > 0 && (text[0] <= ' ' || text[len - 1] <= ' ') && len < 2 * MAX_PATH)
        {
            char buf[2 * MAX_PATH + 2];
            sprintf(buf, "\"%s\"", text); // v uvozovkach budou videt mezery na zacatku a na konci (jinak jsou neviditelne)
            return SetText(buf);
        }
    }
    return SetText(text);
}

void CStaticText::PrepareForPaint()
{
    ClipDraw = FALSE;
    Text2Draw = FALSE;

    if (Text == NULL || TextLen == 0) // alogritmus je staveny pouze na nenulovy pocet znaku
    {
        TextWidth = 0;
        TextHeight = 0;
        return;
    }

    HDC hDC = HANDLES(GetDC(HWindow));
    HFONT hOldFont = (HFONT)SelectObject(hDC, HFont);
    SIZE sz;
    if (Flags & (STF_PATH_ELLIPSIS | STF_END_ELLIPSIS))
    {
        if (Flags & STF_END_ELLIPSIS)
        {
            // STF_END_ELLIPSIS: retezec bude vypustkou zakoncen
            // potrebujeme delky pouze pro znaky, ktere se vejdou
            int fitChars;
            GetTextExtentExPoint(hDC, Text, TextLen, Width, &fitChars, AlpDX, &sz);

            if (fitChars < TextLen)
            {
                // nevesli jsme se -- musime vlozit vypustku

                // vytahneme sirku "..." pro vypustku
                SIZE ellipsisSZ;
                GetTextExtentPoint32(hDC, "...", 3, &ellipsisSZ);
                int ellipsisWidth = ellipsisSZ.cx;

                // hledame zprava, kolik mame uzobnout z retezce, abychom mohli pripojit vypustku
                while (fitChars > 0 && AlpDX[fitChars - 1] + ellipsisWidth > Width)
                    fitChars--;
                if (fitChars > 0)
                {
                    memmove(Text2, Text, fitChars);
                    TextWidth = AlpDX[fitChars - 1];
                    Text2Len = fitChars;
                }
                else
                {
                    TextWidth = 0;
                    Text2Len = 0;
                }
                strcpy(Text2 + fitChars, "...");
                TextWidth += ellipsisWidth;
                Text2Len += 3;

                Text2Draw = TRUE;
            }
            else
            {
                TextWidth = sz.cx;
            }
        }
        else
        {
            // STF_PATH_ELLIPSIS: vypustka bude unvitr textu
            // potrebujeme delky vsech podretezcu
            GetTextExtentExPoint(hDC, Text, TextLen, 0, NULL, AlpDX, &sz);

            if (sz.cx > Width)
            {
                // nevesli jsme se -- musime vlozit vypustku

                // vytahneme sirku "..." pro vypustku
                SIZE ellipsisSZ;
                GetTextExtentPoint32(hDC, "...", 3, &ellipsisSZ);
                int ellipsisWidth = ellipsisSZ.cx;

                // zprava hledame separator cesty
                const char* p = Text + TextLen - 1;
                while (*p != PathSeparator && p > Text)
                    p--;
                const char* p2 = p;
                if (p > Text)
                    p--;
                int pIndex = (int)(p - Text);

                // text od 'p' dal by se mel vejit cely vcetne elipsy
                if (ellipsisWidth + sz.cx - AlpDX[pIndex] > Width)
                {
                    // nevesel se => hledame zleva misto, kam vlozime vypustku
                    while (pIndex < TextLen && (ellipsisWidth + sz.cx - AlpDX[pIndex] > Width))
                        pIndex++;

                    // vlozime vypustku a za ni zbytek textu
                    pIndex++;
                    strcpy(Text2, "...");
                    Text2Len = 3;
                    TextWidth = ellipsisWidth;
                    if (pIndex < TextLen)
                    {
                        memmove(Text2 + 3, Text + pIndex, TextLen - pIndex + 1); // vcetne terminatoru
                        Text2Len += TextLen - pIndex;
                        TextWidth += sz.cx - AlpDX[pIndex - 1];
                    }
                }
                else
                {
                    int rightPartWidth = sz.cx - AlpDX[pIndex];
                    // zjistime, kolik znaku nechame vlevo pred vypustkou
                    while (pIndex >= 0 && (AlpDX[pIndex] + ellipsisWidth + rightPartWidth) > Width)
                        pIndex--;
                    // leva cast
                    Text2Len = 0;
                    TextWidth = 0;
                    if (pIndex >= 0)
                    {
                        memmove(Text2, Text, pIndex + 1);
                        Text2Len += pIndex + 1;
                        TextWidth += AlpDX[pIndex];
                    }
                    // vypustka
                    memmove(Text2 + Text2Len, "...", 3);
                    Text2Len += 3;
                    TextWidth += ellipsisWidth;
                    // prava cast
                    int rightPartLen = TextLen - (int)(p2 - Text);
                    memmove(Text2 + Text2Len, p2, rightPartLen + 1);
                    Text2Len += rightPartLen;
                    TextWidth += rightPartWidth;
                }

                Text2Draw = TRUE;
            }
            else
            {
                TextWidth = sz.cx;
            }
        }
        TextHeight = sz.cy;
    }
    else
    {
        // staci nam celkove rozmery
        if (Flags & STF_HANDLEPREFIX)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            DrawText(hDC, Text, TextLen, &r, DT_CALCRECT | DT_SINGLELINE | DT_LEFT);
            TextWidth = r.right;
            TextHeight = r.bottom;
        }
        else
        {
            GetTextExtentPoint32(hDC, Text, TextLen, &sz);
            TextWidth = sz.cx + 1;
            TextHeight = sz.cy;
        }
    }
    // pokud by text mel prelest hranici okna, musime pri paintu clipovat
    if (TextWidth > Width)
    {
        TextWidth = Width;
        ClipDraw = TRUE;
    }
    if (TextHeight > Height)
    {
        TextHeight = Height;
        ClipDraw = TRUE;
    }
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(HWindow, hDC));
}

void CStaticText::SetPathSeparator(char separator)
{
    if (separator == 0)
        TRACE_E("CStaticText::SetPathSeparator == 0");
    else
    {
        if (separator != PathSeparator)
        {
            PathSeparator = separator;
            InvalidateRect(HWindow, NULL, FALSE);
            PrepareForPaint();
        }
    }
}

int CStaticText::GetTextXOffset()
{
    int xOffset = 0; // SS_LEFT
    if (Alignment == 1)
        xOffset = (Width - TextWidth) / 2; // SS_CENTER
    else if (Alignment == 2)
        xOffset = Width - TextWidth; // SS_RIGHT
    return xOffset;
}

BOOL CStaticText::TextHitTest(POINT* screenCursorPos)
{
    POINT p = *screenCursorPos;
    ScreenToClient(HWindow, &p);

    int xOffset = GetTextXOffset();

    RECT r;
    r.left = xOffset;
    r.top = 0;
    r.right = xOffset + TextWidth;
    r.bottom = TextHeight;

    return PtInRect(&r, p);
}

BOOL CStaticText::SetToolTipText(const char* text)
{
    if (text != NULL && ToolTipText != NULL && strcmp(ToolTipText, text) == 0)
        return TRUE;

    if (text == NULL)
    {
        if (ToolTipText != NULL)
            free(ToolTipText);
        ToolTipText = NULL;
        HToolTipNW = NULL;
        ToolTipID = 0;
        return TRUE;
    }

    char* newText = DupStr(text);
    if (newText == NULL)
        return FALSE;

    if (ToolTipText != NULL)
        free(ToolTipText);

    ToolTipText = newText;
    HToolTipNW = NULL;
    ToolTipID = 0;

    PostMessage(MainWindow->ToolTip->HWindow, WM_USER_REFRESHTOOLTIP, 0, 0); // pozadame okenko, aby nasalo novy text a znovu se vykreslilo

    return TRUE;
}

void CStaticText::SetToolTip(HWND hNotifyWindow, DWORD id)
{
    if (ToolTipText != NULL)
        free(ToolTipText);
    ToolTipText = NULL;

    HToolTipNW = hNotifyWindow;
    ToolTipID = id;
}

void CStaticText::EnableHintToolTip(BOOL enable)
{
    HintMode = enable;
}

BOOL CStaticText::ToolTipAssigned()
{
    return ToolTipText != NULL || HToolTipNW != NULL;
}

void CStaticText::DrawFocus(HDC hDC)
{
    BOOL releaseDC = FALSE;
    if (hDC == NULL)
    {
        hDC = HANDLES(GetDC(HWindow));
        releaseDC = TRUE;
    }

    int xOffset = GetTextXOffset();

    RECT r;
    r.left = xOffset;
    r.top = 0;
    r.right = xOffset + TextWidth;
    r.bottom = TextHeight;

    int oldColor = SetTextColor(hDC, GetSysColor(COLOR_BTNFACE));
    int oldBkColor = SetBkColor(hDC, GetSysColor(COLOR_BTNTEXT));
    POINT oldBrushPoint;
    SetBrushOrgEx(hDC, 0, 0, &oldBrushPoint); // pod XP s Normal skinem zlobil paint pokud byl static umisten na gradientovem pozadi (konfigurace FTP klienta)
    DrawFocusRect(hDC, &r);
    SetBrushOrgEx(hDC, oldBrushPoint.x, oldBrushPoint.y, NULL);
    SetTextColor(hDC, oldColor);
    SetBkColor(hDC, oldBkColor);

    if (releaseDC)
        HANDLES(ReleaseDC(HWindow, hDC));
}

BOOL CStaticText::ShowHint()
{
    SetCurrentToolTip(NULL, 0);

    RECT r;
    GetWindowRect(HWindow, &r);
    int xOffset = GetTextXOffset();

    MainWindow->ToolTip->SetCurrentToolTip(HWindow, 1, -1);
    MainWindow->ToolTip->Show(r.left + xOffset, r.bottom, FALSE, TRUE, HWindow);
    // pozor, Show ma parametr 'modal'==TRUE, takze sem se to vrati az po zavreni tooltipu
    return TRUE;
}

LRESULT
CStaticText::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CStaticText::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_SIZE:
    {
        Width = LOWORD(lParam);
        Height = HIWORD(lParam);
        if (Bitmap != NULL)
        {
            if (!Bitmap->Enlarge(Width, Height))
            {
                delete Bitmap;
                Bitmap = NULL;
            }
        }
        InvalidateRect(HWindow, NULL, FALSE);
        PrepareForPaint();
        return 0;
    }

    case WM_ERASEBKGND:
    {
        // podmazeme az v paintu
        return TRUE;
    }

    case WM_ENABLE:
    {
        InvalidateRect(HWindow, NULL, FALSE);
        PrepareForPaint();
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (ToolTipAssigned())
        {
            POINT p;
            DWORD messagePos = GetMessagePos();
            p.x = GET_X_LPARAM(messagePos);
            p.y = GET_Y_LPARAM(messagePos);
            if (TextHitTest(&p))
            {
                if (ToolTipText != NULL)
                    SetCurrentToolTip(HWindow, 1);
                else if (HToolTipNW != NULL)
                    SetCurrentToolTip(HWindow, ToolTipID);
            }
            else
                SetCurrentToolTip(NULL, 0);

            if (!MouseIsTracked)
            {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = HWindow;
                MouseIsTracked = TrackMouseEvent(&tme);
            }
        }
        break;
    }

    case WM_SHOWWINDOW:
    {
        if (wParam == TRUE)
            break;
        // pokud nas nekdo zhasne, musime sestrelit tooltip
        if (MainWindow != NULL && MainWindow->ToolTip != NULL && MainWindow->ToolTip->HWindow != NULL)
            MainWindow->ToolTip->Hide();
        //PostMessage(MainWindow->ToolTip->HWindow, WM_CANCELMODE, 0, 0);
    } // propadneme do WM_MOUSELEAVE
    case WM_MOUSELEAVE:
    {
        if (ToolTipAssigned())
        {
            SetCurrentToolTip(NULL, 0);
            MouseIsTracked = FALSE;
        }
        break;
    }

    case WM_USER_TTGETTEXT:
    {
        if (ToolTipText != NULL)
            lstrcpyn((char*)lParam, ToolTipText, TOOLTIP_TEXT_MAX);
        return 0;
    }

    case WM_SETTEXT:
    {
        return SetText((char*)lParam);
    }

    case WM_GETTEXT:
    {
        if (Text == NULL || wParam < 2)
            return 0;

        int len = (int)strlen(Text);
        if (len > (int)wParam - 1)
            len = (int)wParam - 1;
        memcpy((char*)lParam, Text, len);
        ((char*)lParam)[len + 1] = 0;
        return len;
    }

    case WM_GETDLGCODE:
    {
        LRESULT ret = DLGC_STATIC;
        if (HintMode)
            ret |= DLGC_WANTARROWS;
        return ret;
    }

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    {
        if (GetWindowLongPtr(HWindow, GWL_STYLE) & WS_TABSTOP)
        {
            DrawFocus(NULL);
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        if (HintMode)
            ShowHint();
        break;
    }

    case WM_KEYDOWN:
    {
        if (HintMode && (wParam == VK_SPACE || wParam == VK_UP || wParam == VK_DOWN))
            ShowHint();
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));

        // pokud mame bitmapu, budeme kreslit do ni, jinak primo do obrazovky
        HDC hDC;
        if (Bitmap != NULL)
            hDC = Bitmap->HMemDC;
        else
            hDC = ps.hdc;

        RECT r;
        r.left = 0;
        r.top = 0;
        r.right = Width;
        r.bottom = Height;

        // zobrazime vlastni text
        if (Text != NULL)
        {
            // pod XPTheme si musime nechat podmazat pozadi od windows
            BOOL bkErased = FALSE;
            if (IsAppThemed())
            {
                DrawThemeParentBackground(HWindow, hDC, &r);
                bkErased = TRUE;
            }

            // nastavime parametry DC a ulozime jejich puvodni hodnoty
            int oldBkMode = SetBkMode(hDC, TRANSPARENT);

            HWND hParent = GetParent(HWindow);
            if (hParent != NULL)
                SendMessage(hParent, WM_CTLCOLORSTATIC, (WPARAM)hDC, (LPARAM)HWindow);
            if (Flags & STF_HYPERLINK_COLOR)
                SetTextColor(hDC, RGB(0, 0, 255));
            BOOL enabled = IsWindowEnabled(HWindow);
            if (!enabled)
                SetTextColor(hDC, GetSysColor(COLOR_GRAYTEXT));

            //        COLORREF textClr;
            //        if (Flags & STF_HYPERLINK_COLOR)
            //          textClr = RGB(0, 0, 255);
            //        else
            //          textClr = GetSysColor(COLOR_BTNTEXT);
            //        COLORREF oldTextColor = SetTextColor(hDC, textClr);
            //        COLORREF oldBkColor = SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
            HFONT hOldFont = (HFONT)SelectObject(hDC, HFont);

            // vykreslime text
            if (Flags & STF_HANDLEPREFIX)
            {
                DWORD drawFlags = DT_SINGLELINE | DT_TOP;
                if (Alignment == 1)
                    drawFlags |= DT_CENTER;
                else if (Alignment == 2)
                    drawFlags |= DT_RIGHT;
                else
                    drawFlags |= DT_LEFT;
                // kvuli cleartype, ktery presahuje mimo control a nechava parazitni barevne body musime
                // clipovat vse; problem je videt v Plugins Manageru Salamander 2.51, kdyz jezdime seznamem
                // pluginu nahoru/dolu, tak pred URL zustava cerveny bod
                // if (!ClipDraw)
                drawFlags |= DT_NOCLIP;

                if (UIState & UISF_HIDEACCEL)
                    drawFlags |= DT_HIDEPREFIX;

                DrawText(hDC, Text, TextLen, &r, drawFlags);
            }
            else
            {
                const char* text;
                int textLen;
                if (Text2Draw)
                {
                    text = Text2;
                    textLen = Text2Len;
                }
                else
                {
                    text = Text;
                    textLen = TextLen;
                }
                DWORD drawFlags = (bkErased) ? 0 : ETO_OPAQUE;
                // if (ClipDraw) // stejny problem jako nahore
                drawFlags |= ETO_CLIPPED;

                int xOffset = GetTextXOffset();
                ExtTextOut(hDC, r.left + xOffset, r.top, drawFlags, &r, text, textLen, NULL);
            }

            if (Flags & STF_DOTUNDERLINE)
            {
                // carkovane podtrzeni
                int xOffset = GetTextXOffset();

                HPEN hDottedPen = HANDLES(CreatePen(PS_DOT, 0, GetTextColor(hDC)));
                HPEN hOldPen = (HPEN)SelectObject(hDC, hDottedPen);
                MoveToEx(hDC, r.left + xOffset, r.bottom - 1, NULL);
                LineTo(hDC, r.left + xOffset + TextWidth, r.bottom - 1);
                SelectObject(hDC, hOldPen);
                HANDLES(DeleteObject(hDottedPen));
            }

            // obnovime puvodni hodnoty DC
            SelectObject(hDC, hOldFont);
            //        SetBkColor(hDC, oldBkColor);
            //        SetTextColor(hDC, oldTextColor);
            SetBkMode(hDC, oldBkMode);
        }
        else
        {
            // nedrzime zadny text; musime alespon podmaznout pozadi
            if (IsAppThemed())
            {
                DrawThemeParentBackground(HWindow, hDC, &r);
            }
            else
                FillRect(hDC, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        }

        if ((GetWindowLongPtr(HWindow, GWL_STYLE) & WS_TABSTOP) && GetFocus() == HWindow)
            DrawFocus(hDC);

        if (Bitmap != NULL)
        {
            // pokud jedeme pres cache, musime ji soupnout do okna
            BitBlt(ps.hdc, 0, 0, Width, Height, Bitmap->HMemDC, 0, 0, SRCCOPY);
        }

        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_UPDATEUISTATE:
    {
        // bohuzel nemuzeme spolehnout na handling standardniho staticku, protoze
        // ten nam pod Vistou (mozna i drive) vykresli na Alt podtrzitko na nesmyslne
        // misto; resenim by bylo sejmout text do naseho bufferu a zobrazovat z neho
        // zvolil jsem jiny pristup a state si drzim u nas
        if (LOWORD(wParam) == UIS_CLEAR)
            UIState &= ~HIWORD(wParam);
        else if (LOWORD(wParam) == UIS_SET)
            UIState |= HIWORD(wParam);

        BOOL showAccel = (LOWORD(wParam) == UIS_CLEAR) && ((HIWORD(wParam) & UISF_HIDEACCEL) != 0);
        if (showAccel)
        {
            InvalidateRect(HWindow, NULL, TRUE); // pokud nejsme cachovany, lehce bliknem, ale kaslu na to8
            UpdateWindow(HWindow);
        }
        return 0;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CHyperLink
//

CHyperLink::CHyperLink(HWND hDlg, int ctrlID, DWORD flags)
    : CStaticText(hDlg, ctrlID, flags)
{
    File[0] = 0;
    Command = 0;
    HDialog = hDlg;

    // upravime styl - abychom dostavali message
    DWORD style = (DWORD)GetWindowLongPtr(HWindow, GWL_STYLE);
    style |= SS_NOTIFY;
    SetWindowLongPtr(HWindow, GWL_STYLE, style);
}

void CHyperLink::SetActionOpen(const char* file)
{
    EnableHintToolTip(FALSE);
    lstrcpyn(File, file, MAX_PATH);
}

void CHyperLink::SetActionPostCommand(WORD command)
{
    EnableHintToolTip(FALSE);
    Command = command;
}

BOOL CHyperLink::SetActionShowHint(const char* text)
{
    EnableHintToolTip(TRUE);
    if (text == NULL)
        return TRUE;
    else
        return SetToolTipText(text);
}

BOOL CHyperLink::ExecuteIt()
{
    BOOL ret = TRUE;
    if (File[0] != 0)
    {
        // neprechazet na shellExecuteWnd, pouzivame z BugReportu
        int err = (int)(INT_PTR)ShellExecute(HWindow, "open", File, NULL, NULL, SW_SHOWNORMAL);
        if (err <= 32)
        {
            ret = FALSE;
            SalMessageBox(HDialog, GetErrorText(err), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        }
    }
    if (Command != 0)
    {
        PostMessage(HDialog, WM_COMMAND, Command, 0);
    }
    return ret;
}

void CHyperLink::OnContextMenu(int x, int y)
{
    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertMenu() dole...
MENU_TEMPLATE_ITEM HyperLinkMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_COPYTOCLIPBOARD
  {MNTT_PE, 0
};
*/
    HMENU hMenu = CreatePopupMenu();
    InsertMenu(hMenu, 0, MF_BYPOSITION, 1, LoadStr(IDS_COPYTOCLIPBOARD));
    DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                 x, y, HWindow, NULL);
    DestroyMenu(hMenu);
    if (cmd == 1)
    {
        CopyTextToClipboard(Text, -1, TRUE, HWindow);
    }
}

LRESULT
CHyperLink::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CHyperLink::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_SETCURSOR:
    {
        POINT p;
        DWORD messagePos = GetMessagePos();
        p.x = GET_X_LPARAM(messagePos);
        p.y = GET_Y_LPARAM(messagePos);
        if (TextHitTest(&p))
            SetHandCursor();
        else
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        POINT p;
        DWORD messagePos = GetMessagePos();
        p.x = GET_X_LPARAM(messagePos);
        p.y = GET_Y_LPARAM(messagePos);
        if (TextHitTest(&p))
        {
            SetCapture(HWindow);
            if (GetWindowLongPtr(HWindow, GWL_STYLE) & WS_TABSTOP)
                SetFocus(HWindow);
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        if (GetCapture() != HWindow)
            break;
        ReleaseCapture();
        POINT p;
        DWORD messagePos = GetMessagePos();
        p.x = GET_X_LPARAM(messagePos);
        p.y = GET_Y_LPARAM(messagePos);
        if (TextHitTest(&p))
            ExecuteIt();
        break;
    }

    case WM_RBUTTONUP:
    {
        if (GetCapture() != HWindow)
            break;
        ReleaseCapture();
        POINT p;
        DWORD messagePos = GetMessagePos();
        p.x = GET_X_LPARAM(messagePos);
        p.y = GET_Y_LPARAM(messagePos);
        if (TextHitTest(&p))
            OnContextMenu(p.x, p.y);
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (wParam == VK_SPACE || wParam == VK_RETURN)
            ExecuteIt();

        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if ((wParam == VK_F10 && shiftPressed || wParam == VK_APPS))
        {
            RECT r;
            GetWindowRect(HWindow, &r);
            OnContextMenu(r.left, r.bottom);
        }

        // podpora pro nase message boxy, preposleme si Ctrl+C
        // v klasickem dialogu by nemelo byt poslani WM_COPY na prekazku
        if (!shiftPressed && controlPressed && !altPressed)
        {
            if (wParam == 'C')
            {
                HWND hParent = GetParent(HWindow);
                if (hParent != NULL)
                    PostMessage(hParent, WM_COPY, 0, 0);
            }
        }

        break;
    }
    }

    return CStaticText::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CButton
//

CColorRectangle::CColorRectangle(HWND hDlg, int ctrlID, CObjectOrigin origin)
    : CWindow(hDlg, ctrlID, origin)
{
    Color = RGB(255, 255, 128);
}

void CColorRectangle::SetColor(COLORREF color)
{
    Color = color;
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

void CColorRectangle::PaintFace(HDC hdc)
{
    RECT r;
    GetClientRect(HWindow, &r);

    COLORREF oldBkColor = SetBkColor(hdc, Color);
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
    SetBkColor(hdc, oldBkColor);
}

LRESULT
CColorRectangle::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = HANDLES(BeginPaint(HWindow, &ps));
        if (hdc != NULL)
        {
            PaintFace(hdc);
            HANDLES(EndPaint(HWindow, &ps));
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CColorGraph
//

CColorGraph::CColorGraph(HWND hDlg, int ctrlID, CObjectOrigin origin)
    : CWindow(hDlg, ctrlID, origin)
{
    Color1Light = NULL;
    Color1Dark = NULL;
    Color2Light = NULL;
    Color2Dark = NULL;
    UsedProc = 0;

    GetClientRect(HWindow, &ClientRect);
}

CColorGraph::~CColorGraph()
{
    if (Color1Light != NULL)
        HANDLES(DeleteObject(Color1Light));
    if (Color1Dark != NULL)
        HANDLES(DeleteObject(Color1Dark));
    if (Color2Light != NULL)
        HANDLES(DeleteObject(Color2Light));
    if (Color2Dark != NULL)
        HANDLES(DeleteObject(Color2Dark));
}

void CColorGraph::SetColor(COLORREF color1Light, COLORREF color1Dark,
                           COLORREF color2Light, COLORREF color2Dark)
{
    Color1Light = HANDLES(CreateSolidBrush(color1Light));
    Color1Dark = HANDLES(CreateSolidBrush(color1Dark));
    Color2Light = HANDLES(CreateSolidBrush(color2Light));
    Color2Dark = HANDLES(CreateSolidBrush(color2Dark));

    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

void CColorGraph::SetUsed(double used)
{
    UsedProc = used;
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

#define GRAPH_HEIGHT 4
#define PI 3.141592653589793

void CColorGraph::PaintFace(HDC hdc)
{
    CALL_STACK_MESSAGE1("CColorGraph::PaintFace()");
    RECT r;
    GetClientRect(HWindow, &r);

    double beta = UsedProc * 360 * PI / 180;

    double elX0 = r.right / 2;
    double elY0 = (r.bottom - GRAPH_HEIGHT) / 2;
    double elA = r.right / 2;
    double elB = (r.bottom - GRAPH_HEIGHT) / 2;
    double elX = elX0 + elA * cos(beta);
    double elY = elB * sin(beta);

    // vykreslim spodek
    HBRUSH hOldBrush = (HBRUSH)GetCurrentObject(hdc, OBJ_BRUSH);
    HPEN hBlackPen = HANDLES(CreatePen(PS_SOLID, 0, RGB(0, 0, 0)));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hBlackPen);

    if (UsedProc < 0.5)
        SelectObject(hdc, Color1Dark); // free color
    else
        SelectObject(hdc, Color2Dark); // used color

    Ellipse(hdc, r.left, r.top + GRAPH_HEIGHT, r.right, r.bottom);

    // osetrim variantu (b)
    if (UsedProc > 0 && UsedProc < 0.5)
    {
        SelectObject(hdc, Color2Dark); // used color
        int x = (int)elX;
        int y1 = (int)(elY0 + GRAPH_HEIGHT + elY);
        int y2 = (int)(elY0 + GRAPH_HEIGHT - elY);
        Chord(hdc, r.left, r.top + GRAPH_HEIGHT, r.right, r.bottom,
              x, y1, x, y2);
    }

    // vykreslim vrsek
    if (UsedProc >= 0 && UsedProc < 1)
        SelectObject(hdc, Color1Light); // free color
    else
        SelectObject(hdc, Color2Light); // used color

    Ellipse(hdc, r.left, r.top, r.right, r.bottom - GRAPH_HEIGHT);

    if (UsedProc > 0 && UsedProc < 1)
    {
        SelectObject(hdc, Color2Light); // used color
        int y1 = (int)(elY0 + elY);
        int y2 = (int)elY0;
        if (y1 == y2 && UsedProc < 0.1)     // prasarnicka
            SelectObject(hdc, Color1Light); // used color
        Pie(hdc, r.left, r.top, r.right, r.bottom - GRAPH_HEIGHT,
            (int)elX, y1, r.right, y2);
    }

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    HANDLES(DeleteObject(hBlackPen));
}

/*
LRESULT
CColorGraph::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("CColorGraph::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
  switch (uMsg)
  {
    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = HANDLES(BeginPaint(HWindow, &ps));
      FillRect(hdc, &ClientRect, (HBRUSH)(COLOR_BTNFACE + 1));
      PaintFace(hdc);
      HANDLES(EndPaint(HWindow, &ps));
      return 0;
    }
  }
  return  CWindow::WindowProc(uMsg, wParam, lParam);
}
*/

// tato varianta pres memDC funguje i pod XP (varianta nahore ma pod winXP okousane krivky)

LRESULT
CColorGraph::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CColorGraph::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = HANDLES(BeginPaint(HWindow, &ps));
        if (hdc != NULL)
        {
            CBitmap bitmap;
            bitmap.CreateBmp(hdc, ClientRect.right, ClientRect.bottom);

            HBRUSH hBrush = (HBRUSH)(COLOR_BTNFACE + 1);
            FillRect(bitmap.HMemDC, &ClientRect, hBrush);

            PaintFace(bitmap.HMemDC);

            BitBlt(hdc,
                   0, 0,
                   ClientRect.right, ClientRect.bottom,
                   bitmap.HMemDC,
                   0, 0,
                   SRCCOPY);

            HANDLES(EndPaint(HWindow, &ps));
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CButton
//

CButton::CButton(HWND hDlg, int ctrlID, DWORD flags, CObjectOrigin origin)
    : CWindow(hDlg, ctrlID, origin)
{
    Flags = flags;
    DropDownPressed = FALSE;
    Checked = FALSE;
    ButtonPressed = FALSE;
    Pressed = FALSE;
    DefPushButton = FALSE;
    Captured = FALSE;
    Space = FALSE;
    MouseIsTracked = FALSE;
    ToolTipText = NULL;
    HToolTipNW = NULL;
    ToolTipID = 0;
    Hot = FALSE;
    GetClientRect(HWindow, &ClientRect);
    DropDownUpTime = GetTickCount();
    UIState = (WORD)SendMessage(HWindow, WM_QUERYUISTATE, 0, 0);
}

CButton::~CButton()
{
    if (ToolTipText != NULL)
        free(ToolTipText);
}

DWORD
CButton::GetFlags()
{
    return Flags;
}

void CButton::SetFlags(DWORD flags, BOOL updateWindow)
{
    Flags = flags;
    if (HWindow != NULL)
    {
        InvalidateRect(HWindow, NULL, FALSE);
        if (updateWindow)
            UpdateWindow(HWindow);
    }
}

void CButton::RePaint()
{
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

void CButton::NotifyParent(WORD notify)
{
    int id = GetWindowLong(HWindow, GWL_ID);
    PostMessage(GetParent(HWindow), WM_COMMAND,
                (WPARAM)(id | ((WPARAM)notify << 16)), (LPARAM)HWindow);
}

void CButton::PaintFrame(HDC hDC, const RECT* r, BOOL down)
{
    if (/*!(ButtonPressed && Pressed) && */ (Flags & BTF_CHECKBOX) && Checked)
    {
        // nejtmavsi vlevo a nahore
        HPEN hOldPen = (HPEN)SelectObject(hDC, WndFramePen);
        MoveToEx(hDC, r->left, r->bottom - 2, NULL);
        LineTo(hDC, r->left, r->top);
        LineTo(hDC, r->right - 1, r->top);
        // tmava vlevo a nahore uvnitr
        SelectObject(hDC, BtnShadowPen);
        MoveToEx(hDC, r->left + 1, r->bottom - 3, NULL);
        LineTo(hDC, r->left + 1, r->top + 1);
        LineTo(hDC, r->right - 2, r->top + 1);
        // trochu tmavsi vpravo a dole uvnitr
        SelectObject(hDC, Btn3DLightPen);
        MoveToEx(hDC, r->right - 2, r->top + 1, NULL);
        LineTo(hDC, r->right - 2, r->bottom - 2);
        LineTo(hDC, r->left, r->bottom - 2);
        // svetla vpravo a dole
        SelectObject(hDC, BtnHilightPen);
        MoveToEx(hDC, r->left, r->bottom - 1, NULL);
        LineTo(hDC, r->right - 1, r->bottom - 1);
        LineTo(hDC, r->right - 1, -1);
        SelectObject(hDC, hOldPen);
        return;
    }

    if (down)
    {
        HPEN hOldPen = (HPEN)SelectObject(hDC, BtnShadowPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, HANDLES(GetStockObject(NULL_BRUSH)));
        Rectangle(hDC, r->left, r->top, r->right, r->bottom);
        SelectObject(hDC, hOldBrush);
        SelectObject(hDC, hOldPen);
    }
    else
    {
        // svetla vlevo a nahore
        HPEN hOldPen = (HPEN)SelectObject(hDC, BtnHilightPen);
        MoveToEx(hDC, r->left, r->bottom - 2, NULL);
        LineTo(hDC, r->left, r->top);
        LineTo(hDC, r->right - 1, r->top);
        // trochu tmavsi uvnitr
        SelectObject(hDC, Btn3DLightPen);
        MoveToEx(hDC, r->left + 1, r->bottom - 3, NULL);
        LineTo(hDC, r->left + 1, r->top + 1);
        LineTo(hDC, r->right - 2, r->top + 1);
        // tmava vpravo a dole
        SelectObject(hDC, BtnShadowPen);
        MoveToEx(hDC, r->right - 2, r->top + 1, NULL);
        LineTo(hDC, r->right - 2, r->bottom - 2);
        LineTo(hDC, r->left, r->bottom - 2);
        // nejtmavsi uplne venku
        SelectObject(hDC, WndFramePen);
        MoveToEx(hDC, r->left, r->bottom - 1, NULL);
        LineTo(hDC, r->right - 1, r->bottom - 1);
        LineTo(hDC, r->right - 1, -1);
        SelectObject(hDC, hOldPen);
    }
}

void CButton::PaintDrop(HDC hDC, const RECT* r, BOOL enabled)
{
    SIZE sz;
    SVGArrowDropDown.GetSize(&sz);
    SVGArrowDropDown.AlphaBlend(hDC,
                                r->left + (r->right - r->left - sz.cx) / 2,
                                r->top + (r->bottom - r->top - sz.cy) / 2,
                                -1, -1,
                                enabled ? SVGSTATE_ENABLED : SVGSTATE_DISABLED);
}

int CButton::GetDropPartWidth()
{
    return (int)((double)SVGArrowDropDown.GetWidth() * 1.6);
}

int CButton::HitTest(LPARAM lParam)
{
    POINT p;
    p.x = LOWORD(lParam);
    p.y = HIWORD(lParam);
    if (!PtInRect(&ClientRect, p))
        return 0; // nowhere

    if (Flags & BTF_DROPDOWN)
    {
        RECT r = ClientRect;
        r.left = r.right - GetDropPartWidth() - 1;
        if (PtInRect(&r, p))
            return 2; // drop down
    }

    return 1; // button
}

void CButton::PaintFace(HDC hdc, const RECT* rect, BOOL enabled)
{
    RECT r = *rect;
    if (Flags & BTF_RIGHTARROW)
        r.right -= (int)((double)SVGArrowRight.GetWidth() * 1.5);
    if (Flags & BTF_DROPDOWN)
        r.right -= GetDropPartWidth();
    if (Flags & BTF_MORE)
        r.right -= (int)((double)SVGArrowMore.GetWidth() * 1.3);

    DWORD wndStyle = (DWORD)GetWindowLongPtr(HWindow, GWL_STYLE);
    if (wndStyle & BS_ICON)
    {
        // icon

        HICON hIcon = (HICON)SendMessage(HWindow, BM_GETIMAGE, IMAGE_ICON, 0);
        if (hIcon != NULL)
        {
            ICONINFO iconInfo;
            if (GetIconInfo(hIcon, &iconInfo))
            {
                BITMAP bm;
                GetObject(iconInfo.hbmColor, sizeof(bm), &bm);
                if (enabled)
                {
                    DrawIcon(hdc, r.left + (r.right - r.left - bm.bmWidth) / 2,
                             r.top + (r.bottom - r.top - bm.bmHeight) / 2, hIcon);
                }
                else
                {
                    // disabled
                    CGuiBitmap tmpFaceBitmap;
                    tmpFaceBitmap.CreateBmp(hdc, bm.bmWidth, bm.bmHeight);
                    RECT fillR = {0};
                    fillR.right = bm.bmWidth;
                    fillR.bottom = bm.bmHeight;
                    FillRect(tmpFaceBitmap.HMemDC, &fillR, (HBRUSH)(COLOR_BTNFACE + 1));
                    DrawIcon(tmpFaceBitmap.HMemDC, 0, 0, hIcon);
                    HBITMAP hBmp = tmpFaceBitmap.CreateCopyBitmap();
                    DrawState(hdc, NULL, NULL, (LPARAM)hBmp, 0,
                              r.left + (r.right - r.left - bm.bmWidth) / 2,
                              r.top + (r.bottom - r.top - bm.bmHeight) / 2,
                              bm.bmWidth, bm.bmHeight,
                              DST_BITMAP | DSS_DISABLED);
                    HANDLES(DeleteObject(hBmp));
                }
                DeleteObject(iconInfo.hbmMask);
                DeleteObject(iconInfo.hbmColor);
            }
        }
    }
    else
    {
        // text

        // vytahnu text tlacitka
        char buff[500];
        GetWindowText(HWindow, buff, 500);

        // vytahnu aktualni font
        HFONT hFont = (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0);

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        int oldBkMode = SetBkMode(hdc, TRANSPARENT);
        int oldTextColor = SetTextColor(hdc, GetSysColor(enabled ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
        RECT r2 = r;
        r2.top--;
        DWORD dtFlags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        if (UIState & UISF_HIDEACCEL)
            dtFlags |= DT_HIDEPREFIX;
        DrawText(hdc, buff, -1, &r2, dtFlags);
        SetTextColor(hdc, oldTextColor);
        SetBkMode(hdc, oldBkMode);
        SelectObject(hdc, hOldFont);
    }

    if (Flags & BTF_RIGHTARROW)
    {
        BOOL empty = FALSE;
        if ((wndStyle & BS_ICON) == 0)
        {
            char buff[500];
            GetWindowText(HWindow, buff, 500);
            empty = (buff[0] == 0);
        }

        SIZE sz;
        SVGArrowRight.GetSize(&sz);

        if (empty)
        {
            // pokud jde o tlacitko obsahujici pouze sipku, vycentrujeme ji v obou osach
            r = *rect;
            r.left += (int)((double)sz.cx * 0.3);
        }
        else
        {
            r.right += sz.cx;
            r.left = r.right - sz.cx;
        }
        SVGArrowRight.AlphaBlend(hdc,
                                 r.left + (r.right - r.left - sz.cx) / 2,
                                 r.top + (r.bottom - r.top - sz.cy) / 2,
                                 -1, -1,
                                 enabled ? SVGSTATE_ENABLED : SVGSTATE_DISABLED);
    }

    if (Flags & BTF_MORE)
    {
        CSVGSprite* sprite = Checked ? &SVGArrowLess : &SVGArrowMore;

        SIZE sz;
        sprite->GetSize(&sz);

        r.right += sz.cx;
        r.left = r.right - sz.cx;
        sprite->AlphaBlend(hdc,
                           r.left + (r.right - r.left - sz.cx) / 2,
                           r.top + (r.bottom - r.top - sz.cy) / 2,
                           -1, -1,
                           enabled ? SVGSTATE_ENABLED : SVGSTATE_DISABLED);
    }
}

BOOL CButton::SetToolTipText(const char* text)
{
    if (text == NULL)
    {
        if (ToolTipText != NULL)
            free(ToolTipText);
        ToolTipText = NULL;
        HToolTipNW = NULL;
        ToolTipID = 0;
        return TRUE;
    }

    char* newText = DupStr(text);
    if (newText == NULL)
        return FALSE;

    if (ToolTipText != NULL)
        free(ToolTipText);

    ToolTipText = newText;
    HToolTipNW = NULL;
    ToolTipID = 0;
    return TRUE;
}

void CButton::SetToolTip(HWND hNotifyWindow, DWORD id)
{
    if (ToolTipText != NULL)
        free(ToolTipText);
    ToolTipText = NULL;

    HToolTipNW = hNotifyWindow;
    ToolTipID = id;
}

BOOL CButton::ToolTipAssigned()
{
    return ToolTipText != NULL || HToolTipNW != NULL;
}

LRESULT
CButton::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CButton::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_GETDLGCODE:
    {
        DWORD ret = DLGC_BUTTON;
        if (DefPushButton)
            ret |= DLGC_DEFPUSHBUTTON;
        else
            ret |= DLGC_UNDEFPUSHBUTTON;
        if (Flags & BTF_DROPDOWN)
            ret |= DLGC_WANTARROWS;
        return ret;
    }

    case WM_SETFOCUS:
    {
        RePaint();
        if (GetWindowLongPtr(HWindow, GWL_STYLE) & BS_NOTIFY)
            NotifyParent(BN_SETFOCUS);
        return 0;
    }

    case WM_KILLFOCUS:
    {
        if (Captured)
        {
            ReleaseCapture();
            Captured = FALSE;
            ButtonPressed = FALSE;
            Pressed = FALSE;
        }
        if (Space)
        {
            Space = FALSE;
            ButtonPressed = FALSE;
            Pressed = FALSE;
        }
        RePaint();
        if (GetWindowLongPtr(HWindow, GWL_STYLE) & BS_NOTIFY)
            NotifyParent(BN_KILLFOCUS);
        return 0;
    }

    case WM_ENABLE:
    {
        RePaint();
        return 0;
    }

    case WM_SIZE:
    {
        GetClientRect(HWindow, &ClientRect);
        break;
    }

    case WM_SETTEXT:
    {
        // WM_SETTEXT by explicitne prekreslil control -- tomu se budeme branit
        SendMessage(HWindow, WM_SETREDRAW, FALSE, 0);
        LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
        SendMessage(HWindow, WM_SETREDRAW, TRUE, 0);
        RePaint();
        return ret;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = HANDLES(BeginPaint(HWindow, &ps));

        if (hdc != NULL)
        {
            BOOL enabled = IsWindowEnabled(HWindow);
            //BOOL down = enabled && (ButtonPressed && Pressed || (Flags & BTF_CHECKBOX) && Checked);
            BOOL checked = enabled && (Flags & BTF_CHECKBOX) && Checked;
            BOOL down = enabled && ButtonPressed && Pressed;
            BOOL focused = GetFocus() == HWindow;

            // budeme kreslit pres memory dc
            CGuiBitmap tmpBitmap;
            tmpBitmap.CreateBmp(hdc, ClientRect.right, ClientRect.bottom);
            HDC hMemDC = tmpBitmap.HMemDC;

            if (IsAppThemed())
            {
                // pokud jedeme pod xp theme, pouzijeme je
                HTHEME hTheme = OpenThemeData(HWindow, L"Button");
                int state = PBS_DISABLED;
                if (enabled)
                {
                    state = PBS_NORMAL;
                    if (Hot)
                        state = PBS_HOT;
                    if (down || checked)
                    {
                        if (checked && Hot && !down)
                            state = PBS_HOT;
                        else
                            state = PBS_PRESSED;
                    }
                    else if (focused)
                    {
                        if (Hot)
                            state = PBS_HOT;
                        else
                            state = PBS_DEFAULTED;
                    }
                }
                // podmazeme pozadi, tlacitko ma pruhledne oblasti
                HBRUSH hBrush = (HBRUSH)(COLOR_BTNFACE + 1);
                //          if (!(ButtonPressed && Pressed) && (Flags & BTF_CHECKBOX) && Checked) hBrush = HDitherBrush;
                FillRect(hMemDC, &ClientRect, hBrush);

                // vykreslime pozadi tlacitka
                DrawThemeBackground(hTheme, hMemDC, BP_PUSHBUTTON, state, &ClientRect, NULL);

                if (Flags & BTF_DROPDOWN)
                {
                    RECT ddR;
                    ddR = ClientRect;
                    ddR.left = ddR.right - GetDropPartWidth() - 3;

                    RECT r = ddR;
                    r.left += 1;
                    r.top += 4;
                    r.right = r.left + 1;
                    r.bottom -= 4;
                    FillRect(hMemDC, &r, (HBRUSH)(COLOR_GRAYTEXT + 1));
                    r.left = r.right;
                    r.right = r.left + 1;
                    FillRect(hMemDC, &r, (HBRUSH)(COLOR_3DHILIGHT + 1));

                    if (DropDownPressed && Pressed)
                    {
                        // vykreslime pozadi tlacitka v drop-down casti
                        r = ddR;
                        r.left += 2;
                        DrawThemeBackground(hTheme, hMemDC, BP_PUSHBUTTON, PBS_PRESSED, &ClientRect, &r);

                        ddR.top++;
                        ddR.bottom++;
                    }
                    PaintDrop(hMemDC, &ddR, enabled);
                }

                // vykreslime face
                RECT fr = ClientRect;
                fr.left += 4;
                fr.top += 4;
                fr.right -= 4;
                fr.bottom -= 4;
                PaintFace(hMemDC, &fr, enabled);

                // vykreslime focus
                if (focused)
                {
                    RECT r = ClientRect;
                    r.left += 3;
                    r.top += 3;
                    r.right -= 3;
                    r.bottom -= 3;
                    DrawFocusRect(hMemDC, &r);
                }
                CloseThemeData(hTheme);
            }
            else
            {
                // jinak se nakreslime sami
                HBRUSH hBrush = (HBRUSH)(COLOR_BTNFACE + 1);
                if (/*!(ButtonPressed && Pressed) && */ (Flags & BTF_CHECKBOX) && Checked)
                {
                    hBrush = HDitherBrush;
                    SetTextColor(hMemDC, GetSysColor(COLOR_BTNFACE));
                    SetBkColor(hMemDC, GetSysColor(COLOR_3DHILIGHT));
                }
                FillRect(hMemDC, &ClientRect, hBrush);

                RECT fr = ClientRect;
                fr.left += 4;
                fr.top += 4;
                fr.right -= 4;
                fr.bottom -= 4;
                if (down)
                {
                    fr.left++;
                    fr.top++;
                    fr.right++;
                    fr.bottom++;
                }
                PaintFace(hMemDC, &fr, enabled);

                RECT clR = ClientRect;
                if (DefPushButton && !Checked)
                {
                    HPEN hOldPen = (HPEN)SelectObject(hMemDC, WndFramePen);
                    HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, HANDLES(GetStockObject(NULL_BRUSH)));
                    Rectangle(hMemDC, ClientRect.left, ClientRect.top, ClientRect.right, ClientRect.bottom);
                    SelectObject(hMemDC, hOldBrush);
                    SelectObject(hMemDC, hOldPen);
                    InflateRect(&clR, -1, -1);
                }

                RECT ddR;
                if (Flags & BTF_DROPDOWN)
                {
                    ddR = clR;
                    ddR.left = ddR.right - GetDropPartWidth() - 1;
                    clR.right -= GetDropPartWidth();
                }
                PaintFrame(hMemDC, &clR, down);
                if (Flags & BTF_DROPDOWN)
                {
                    PaintFrame(hMemDC, &ddR, DropDownPressed && Pressed);
                    if (DropDownPressed && Pressed)
                    {
                        ddR.left++;
                        ddR.top++;
                        ddR.right++;
                        ddR.bottom++;
                    }
                    PaintDrop(hMemDC, &ddR, enabled);
                }

                if (focused)
                {
                    RECT r = ClientRect;
                    InflateRect(&r, -4, -4);
                    if (Flags & BTF_DROPDOWN)
                        r.right -= GetDropPartWidth();
                    if (down)
                    {
                        r.left++;
                        r.top++;
                        r.right++;
                        r.bottom++;
                    }
                    int oldColor = SetTextColor(hMemDC, GetSysColor(COLOR_BTNFACE));
                    int oldBkColor = SetBkColor(hMemDC, GetSysColor(COLOR_BTNTEXT));
                    DrawFocusRect(hMemDC, &r);
                    SetTextColor(hMemDC, oldColor);
                    SetBkColor(hMemDC, oldBkColor);
                }
            }

            BitBlt(hdc,
                   0, 0,
                   ClientRect.right, ClientRect.bottom,
                   hMemDC,
                   0, 0,
                   SRCCOPY);

            HANDLES(EndPaint(HWindow, &ps));
        }
        return 0;
    }

    case WM_UPDATEUISTATE:
    {
        // bohuzel nemuzeme spolehnout na handling standardniho staticku, protoze
        // ten nam pod Vistou (mozna i drive) vykresli na Alt podtrzitko na nesmyslne
        // misto; resenim by bylo sejmout text do naseho bufferu a zobrazovat z neho
        // zvolil jsem jiny pristup a state si drzim u nas
        if (LOWORD(wParam) == UIS_CLEAR)
            UIState &= ~HIWORD(wParam);
        else if (LOWORD(wParam) == UIS_SET)
            UIState |= HIWORD(wParam);

        BOOL showAccel = (LOWORD(wParam) == UIS_CLEAR) && ((HIWORD(wParam) & UISF_HIDEACCEL) != 0);
        if (showAccel)
        {
            InvalidateRect(HWindow, NULL, TRUE); // jedeme pres cache bitmap, takze neblikneme
            UpdateWindow(HWindow);
        }
        return 0;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if ((Flags & BTF_DROPDOWN) && (wParam == VK_RIGHT || wParam == VK_LEFT))
        {
            // sipky vlevo a vpravo by mohly fungovat
            HWND hParent = GetParent(HWindow);
            if (hParent != NULL)
            {
                HWND hNext = GetNextDlgGroupItem(hParent, HWindow, wParam == VK_LEFT);
                if (hNext != NULL)
                {
                    SendMessage(hParent, WM_NEXTDLGCTL, (WPARAM)hNext, TRUE);
                }
            }
            return 0;
        }
        if ((Flags & BTF_DROPDOWN) && (wParam == VK_DOWN || wParam == VK_UP))
        {
            // kavesama Up/Down lze otevrit drop-down
            ButtonPressed = FALSE;
            DropDownPressed = TRUE;
            Pressed = TRUE;
            RePaint();
            SendMessage(GetParent(HWindow), WM_USER_BUTTONDROPDOWN,
                        (WPARAM)GetMenu(HWindow), MAKELPARAM(TRUE, 0));
            DropDownPressed = FALSE;
            Pressed = FALSE;
            Captured = FALSE;
            RePaint();
            return 0;
        }
        if ((int)wParam == VK_SPACE)
        {
            // space zamackne tlacitko
            ButtonPressed = TRUE;
            Pressed = TRUE;
            RePaint();
            if (Flags & BTF_LBUTTONDOWN)
            {
                SendMessage(GetParent(HWindow), WM_USER_BUTTON,
                            MAKELPARAM(GetMenu(HWindow), 0), MAKELPARAM(TRUE, 0));
                ButtonPressed = FALSE;
                Pressed = FALSE;
                RePaint();
            }
            else
                Space = TRUE;
            return 0;
        }
        else if (Space)
        {
            Space = FALSE;
            ButtonPressed = FALSE;
            Pressed = FALSE;
            RePaint();
            return 0;
        }
        break;
    }

    case WM_KEYUP:
    {
        if (Space && wParam == VK_SPACE)
        {
            Space = FALSE;
            ButtonPressed = FALSE;
            Pressed = FALSE;
            if (Flags & BTF_CHECKBOX)
                Checked = !Checked;
            RePaint();
            NotifyParent(BN_CLICKED);
            return 0;
        }
        break;
    }

    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    {
        // pokud kliknuti prislo do 25ms po odmacknuti drop downu, zahodime ho, aby nedoslo
        // ke zbytecnemu novemu zamacknuti
        if (GetTickCount() - DropDownUpTime <= 25)
            return 0;

        if (ToolTipAssigned())
        {
            SetCurrentToolTip(NULL, 0);
        }

        int hitTest = HitTest(lParam);
        if (!Captured && hitTest != 0)
        {
            if (hitTest == 1)
            {
                ButtonPressed = TRUE;
                DropDownPressed = FALSE;
            }
            else
            {
                DropDownPressed = TRUE;
                ButtonPressed = FALSE;
            }

            Pressed = TRUE;
            Captured = TRUE;
            SetCapture(HWindow);
            if (GetFocus() != HWindow)
                SetFocus(HWindow);
            RePaint();

            if (DropDownPressed)
            {
                SendMessage(GetParent(HWindow), WM_USER_BUTTONDROPDOWN,
                            (WPARAM)GetMenu(HWindow), MAKELPARAM(FALSE, 0));
                DropDownPressed = FALSE;
                Pressed = FALSE;
                Captured = FALSE;
                ReleaseCapture();
                RePaint();
                DropDownUpTime = GetTickCount();
            }
            else
            {
                if (Flags & BTF_LBUTTONDOWN)
                {
                    SendMessage(GetParent(HWindow), WM_USER_BUTTON,
                                MAKELPARAM(GetMenu(HWindow), 0), MAKELPARAM(FALSE, 0));
                    Pressed = FALSE;
                    Captured = FALSE;
                    ReleaseCapture();
                    RePaint();
                }
            }
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (Captured)
        {
            ReleaseCapture();
            Captured = FALSE;
            ButtonPressed = FALSE;
            DropDownPressed = FALSE;
            Pressed = FALSE;

            int hitTest = HitTest(lParam);
            if (hitTest == 1 && (Flags & BTF_CHECKBOX))
                Checked = !Checked;
            RePaint();
            if (hitTest == 1)
                NotifyParent(BN_CLICKED);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (Captured)
        {
            int hitTest = HitTest(lParam);
            BOOL pressed = FALSE;
            if (ButtonPressed && hitTest == 1)
                pressed = TRUE;
            if (DropDownPressed && hitTest == 2)
                pressed = TRUE;
            if (pressed != Pressed)
            {
                Pressed = pressed;
                RePaint();
            }
        }
        else
        {
            if (!Hot && IsAppThemed())
            {
                Hot = TRUE;
                RePaint();
                if (!MouseIsTracked)
                {
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = HWindow;
                    MouseIsTracked = TrackMouseEvent(&tme);
                }
            }

            if (ToolTipAssigned())
            {
                if (HitTest(lParam) != 0)
                {
                    if (ToolTipText != NULL)
                        SetCurrentToolTip(HWindow, 1);
                    else if (HToolTipNW != NULL)
                        SetCurrentToolTip(HWindow, ToolTipID);
                }
                else
                    SetCurrentToolTip(NULL, 0);

                if (!MouseIsTracked)
                {
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = HWindow;
                    MouseIsTracked = TrackMouseEvent(&tme);
                }
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
    {
        if (Hot)
        {
            Hot = FALSE;
            RePaint();
        }
        if (ToolTipAssigned())
            SetCurrentToolTip(NULL, 0);
        MouseIsTracked = FALSE;
        break;
    }

    case WM_USER_TTGETTEXT:
    {
        if (ToolTipText != NULL)
            lstrcpyn((char*)lParam, ToolTipText, TOOLTIP_TEXT_MAX);
        return 0;
    }

    case BM_SETSTATE:
    {
        BOOL highlight = (wParam != 0);
        if (highlight != ButtonPressed)
        {
            ButtonPressed = highlight;
            Pressed = ButtonPressed;
            RePaint();
        }
        return 0;
    }

    case BM_GETSTATE:
    {
        int state = 0;
        if (GetFocus() == HWindow)
            state |= BST_FOCUS;
        if (ButtonPressed && Pressed)
            state |= BST_PUSHED;
        return state;
    }

    case BM_SETCHECK:
    {
        BOOL checked = (wParam == BST_CHECKED);
        if (checked != Checked)
        {
            Checked = checked;
            RePaint();
        }
    }

    case BM_GETCHECK:
    {
        if (Checked)
            return BST_CHECKED;
        return BST_UNCHECKED;
    }

    case BM_SETSTYLE:
    {
        WORD dwStyle = LOWORD(wParam);
        BOOL fRedraw = LOWORD(lParam);
        DefPushButton = FALSE;
        if (dwStyle & BS_DEFPUSHBUTTON)
            DefPushButton = TRUE;
        if (fRedraw)
            RePaint();
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CColorButton
//
/*
CColorButton::CColorButton(HWND hDlg, int ctrlID, CObjectOrigin origin)
 : CButton(hDlg, ctrlID, origin, FALSE, FALSE)
{
  Color = RGB(255, 255, 128);
}


void
CColorButton::SetColor(COLORREF color)
{
  Color = color;
  RePaint();
}

void
CColorButton::PaintFace(HDC hdc, const RECT *rect)
{
  RECT r = *rect;
//  InflateRect(&r, -4, -4);

  COLORREF oldBkColor = SetBkColor(hdc, Color);
  ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
  SetBkColor(hdc, oldBkColor);

  HPEN hOldPen = (HPEN)SelectObject(hdc, WndFramePen);
  HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, HANDLES(GetStockObject(NULL_BRUSH)));
  Rectangle(hdc, r.left, r.top, r.right, r.bottom);
  SelectObject(hdc, hOldBrush);
  SelectObject(hdc, hOldPen);
}
*/

//****************************************************************************
//
// CColorArrowButton
//
// pozadi s textem, za kterym je jeste sipka - slouzi pro rozbaleni menu
//

CColorArrowButton::CColorArrowButton(HWND hDlg, int ctrlID, BOOL showArrow, CObjectOrigin origin)
    : CButton(hDlg, ctrlID, origin)
{
    TextColor = RGB(0, 0, 0);
    BkgndColor = RGB(255, 255, 255);
    ShowArrow = showArrow;
}

void CColorArrowButton::SetColor(COLORREF textColor, COLORREF bkgndColor)
{
    TextColor = textColor;
    BkgndColor = bkgndColor;
    RePaint();
}

void CColorArrowButton::SetTextColor(COLORREF textColor)
{
    SetColor(textColor, BkgndColor);
}

void CColorArrowButton::SetBkgndColor(COLORREF bkgndColor)
{
    SetColor(TextColor, bkgndColor);
}

void CColorArrowButton::PaintFace(HDC hdc, const RECT* rect, BOOL enabled)
{
    RECT r = *rect;
    SIZE arrowSize;

    if (ShowArrow)
    {
        SVGArrowRightSmall.GetSize(&arrowSize);
        r.right -= (int)((double)arrowSize.cx * 2.5);
    }

    COLORREF bkColor = GetNearestColor(hdc, BkgndColor);
    HBRUSH hBrush = HANDLES(CreateSolidBrush(bkColor));
    HPEN hOldPen = (HPEN)SelectObject(hdc, WndFramePen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    HANDLES(DeleteObject(hBrush));

    HFONT hFont = (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0);
    LOGFONT lf;
    GetObject(hFont, sizeof(lf), &lf);
    lf.lfHeight += 1; // fix pro 100% DPI, kdy se nam prilis velky text dotykal obdelniku
    hFont = HANDLES(CreateFontIndirect(&lf));
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    int oldTextColor = ::SetTextColor(hdc, TextColor);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, "ABC", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetBkMode(hdc, oldBkMode);
    ::SetTextColor(hdc, oldTextColor);
    SelectObject(hdc, hOldFont);
    HANDLES(DeleteObject(hFont));

    if (ShowArrow)
    {
        SVGArrowRightSmall.AlphaBlend(hdc,
                                      r.right + (rect->right - r.right - (int)((double)arrowSize.cx * 0.6)) / 2,
                                      r.top + (r.bottom - r.top - arrowSize.cy) / 2,
                                      -1, -1,
                                      enabled ? SVGSTATE_ENABLED : SVGSTATE_DISABLED);
    }
}

//****************************************************************************
//
// CToolbarHeader
//

int TlbHdrTooltips[TLBHDR_COUNT] =
    {
        IDS_EDTLB_MODIFY,
        IDS_EDTLB_NEW,
        IDS_EDTLB_DELETE,
        IDS_EDTLB_SORT,
        IDS_EDTLB_UP,
        IDS_EDTLB_DOWN,
};

CToolbarHeader::CToolbarHeader(HWND hDlg, int ctrlID, HWND hAlignWindow, DWORD buttonMask)
    : CWindow(hDlg, ctrlID, ooAllocated)
{
    CALL_STACK_MESSAGE3("CToolbarHeader::CToolbarHeader(, %d, , %u)", ctrlID, buttonMask);
    HNotifyWindow = hDlg;
    ButtonMask = buttonMask;
    ToolBar = new CToolBar(HWindow);
    ToolBar->CreateWnd(HWindow);

#ifdef TOOLBARHDR_USE_SVG
    CreateImageLists(&HEnabledImageList, &HDisabledImageList);
    ToolBar->SetImageList(HDisabledImageList);
    ToolBar->SetHotImageList(HEnabledImageList);
#else

    CSVGIcon svgIcons[TLBHDR_COUNT] = {
        {0, "Modify"},
        {1, "New_Insert"},
        {2, "Delete"},
        {3, "SortByName"},
        {4, "MoveItemUp"},
        {5, "MoveItemDown"},
    };

    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    HBITMAP hTmpMaskBitmap;
    HBITMAP hTmpGrayBitmap;
    HBITMAP hTmpColorBitmap;
    CreateToolbarBitmaps(HInstance,
                         IDB_EDTLBTB,
                         RGB(255, 0, 255), GetSysColor(COLOR_BTNFACE),
                         hTmpMaskBitmap, hTmpGrayBitmap, hTmpColorBitmap,
                         FALSE, svgIcons, TLBHDR_COUNT);
    HHotImageList = ImageList_Create(iconSize, iconSize, ILC_MASK | ILC_COLORDDB, TLBHDR_COUNT, 1);
    HGrayImageList = ImageList_Create(iconSize, iconSize, ILC_MASK | ILC_COLORDDB, TLBHDR_COUNT, 1);
    ImageList_Add(HHotImageList, hTmpColorBitmap, hTmpMaskBitmap);
    ImageList_Add(HGrayImageList, hTmpGrayBitmap, hTmpMaskBitmap);
    HANDLES(DeleteObject(hTmpMaskBitmap));
    HANDLES(DeleteObject(hTmpGrayBitmap));
    HANDLES(DeleteObject(hTmpColorBitmap));
    ToolBar->SetImageList(HGrayImageList);
    ToolBar->SetHotImageList(HHotImageList);
    //HImageList = ImageList_Create(TOOLBARHDR_WIDTH, TOOLBARHDR_HEIGHT,
    //  ILC_MASK | ILC_COLORDDB, 5, 1);
    //HBITMAP hbmp = HANDLES(LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_EDTLBTB)));
    //ImageList_AddMasked(HImageList, hbmp, RGB(255, 0, 255));
    //HANDLES(DeleteObject(hbmp));
    //ToolBar->SetImageList(HImageList);
#endif

    UIState = (WORD)SendMessage(HWindow, WM_QUERYUISTATE, 0, 0);

    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_ID | TLBI_MASK_IMAGEINDEX;
    int buttonsCount = 0;
    int i;
    for (i = 0; i < TLBHDR_COUNT; i++)
    {
        if ((1 << i) & ButtonMask)
        {
            tii.ImageIndex = i;
            tii.ID = i + 1;
            ToolBar->InsertItem2(buttonsCount, TRUE, &tii);
            buttonsCount++;
        }
    }

    SIZE sz;
    sz.cx = ToolBar->GetNeededWidth();
    sz.cy = ToolBar->GetNeededHeight();

    RECT r;
    GetWindowRect(hAlignWindow, &r);
    int width = r.right - r.left;
    int height = sz.cy + 2;
    POINT p;
    p.x = r.left;
    p.y = r.top - height;
    ScreenToClient(hDlg, &p);
    SetWindowPos(HWindow, 0, p.x, p.y, width, height, SWP_NOZORDER);
    SetWindowPos(ToolBar->HWindow, HWND_TOP, width - sz.cx - 1, 1, sz.cx, sz.cy, SWP_SHOWWINDOW);
}

#ifdef TOOLBARHDR_USE_SVG
void CToolbarHeader::CreateImageLists(HIMAGELIST* enabled, HIMAGELIST* disabled)
{
    HIMAGELIST hEnabled;
    HIMAGELIST hDisabled;
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16); // small icon size

    // http://stackoverflow.com/questions/2640823/is-it-possible-to-create-a-cimagelist-with-alpha-blending-transparency
    hEnabled = ImageList_Create(iconSize, iconSize,
                                ILC_COLOR32, TOOLBARHDR_BUTTONS, 1);
    hDisabled = ImageList_Create(iconSize, iconSize,
                                 ILC_COLOR32 /*ILC_COLORDDB */, TOOLBARHDR_BUTTONS, 1);

    HDC hDC = HANDLES(CreateCompatibleDC(NULL));

    int width = iconSize * TOOLBARHDR_BUTTONS;
    int height = iconSize;

    BITMAPINFOHEADER bmhdr;
    memset(&bmhdr, 0, sizeof(bmhdr));
    bmhdr.biSize = sizeof(bmhdr);
    bmhdr.biWidth = width;
    bmhdr.biHeight = -height; // top-down
    bmhdr.biPlanes = 1;
    bmhdr.biBitCount = 32;
    bmhdr.biCompression = BI_RGB;
    void* lpBits = NULL;
    HBITMAP hBmp = HANDLES(CreateDIBSection(NULL, (CONST BITMAPINFO*)&bmhdr,
                                            DIB_RGB_COLORS, &lpBits, NULL, 0));

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    // JRYFIXME: docasne cteme ze souboru, prejit na spolecne uloziste s toolbars
    const char* svgNames[] = {"Modify", "New_Insert", "Delete", "SortByName", "MoveItemUp", "MoveItemDown"};
    for (int j = 0; j < 2; j++)
    {
        DWORD* p = (DWORD*)lpBits;
        for (int i = 0; i < width * height; i++)
            *p++ = 0x00000000;

        HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hBmp);
        for (int i = 0; i < TOOLBARHDR_BUTTONS; i++)
            RenderSVGImage(rast, hDC, i * iconSize, 0, svgNames[i], iconSize, RGB(0xff, 0xff, 0xff), j == 0 ? TRUE : FALSE);
        SelectObject(hDC, hOldBmp);
        ImageList_Add(j == 0 ? hEnabled : hDisabled, hBmp, hBmp);
    }
    nsvgDeleteRasterizer(rast);
    HANDLES(DeleteDC(hDC));
    HANDLES(DeleteObject(hBmp));
    *enabled = hEnabled;
    *disabled = hDisabled;
}
#endif // TOOLBARHDR_USE_SVG

void CToolbarHeader::EnableToolbar(DWORD enableMask)
{
    int i;
    for (i = 0; i < TLBHDR_COUNT; i++)
    {
        if ((1 << i) & ButtonMask)
            ToolBar->EnableItem(i + 1, FALSE, ((1 << i) & enableMask) != 0);
    }
}

void CToolbarHeader::CheckToolbar(DWORD checkMask)
{
    int i;
    for (i = 0; i < TLBHDR_COUNT; i++)
    {
        if ((1 << i) & ButtonMask)
            ToolBar->CheckItem(i + 1, FALSE, ((1 << i) & checkMask) != 0);
    }
}

void CToolbarHeader::OnPaint(HDC hDC, BOOL hideAccel, BOOL prefixOnly)
{
    RECT r;
    GetClientRect(HWindow, &r);
    DrawEdge(hDC, &r, BDR_SUNKENOUTER, BF_RECT);
    r.left += 5;
    char buff[100];
    GetWindowText(HWindow, buff, 100);
    SetBkMode(hDC, TRANSPARENT);
    HFONT hOldFont = (HFONT)SelectObject(hDC, (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0));
    DWORD dtFlags = DT_SINGLELINE | DT_LEFT | DT_VCENTER;
    if (hideAccel)
        dtFlags |= DT_HIDEPREFIX;
    if (prefixOnly)
        dtFlags |= DT_PREFIXONLY;
    DrawText(hDC, buff, -1, &r, dtFlags);
    SelectObject(hDC, hOldFont);
}

LRESULT
CToolbarHeader::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CToolbarHeader::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_DESTROY:
    {
        if (ToolBar != NULL)
            DestroyWindow(ToolBar->HWindow);
#ifdef TOOLBARHDR_USE_SVG
        if (HEnabledImageList != NULL)
            ImageList_Destroy(HEnabledImageList);
        if (HDisabledImageList != NULL)
            ImageList_Destroy(HDisabledImageList);
#else
        if (HHotImageList != NULL)
            ImageList_Destroy(HHotImageList);
        if (HGrayImageList != NULL)
            ImageList_Destroy(HGrayImageList);
#endif
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        BOOL hideAccel = (UIState & UISF_HIDEACCEL) != 0;
        OnPaint(hDC, hideAccel, FALSE);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_UPDATEUISTATE:
    {
        // bohuzel nemuzeme spolehnout na handling standardniho staticku, protoze
        // ten nam pod Vistou (mozna i drive) vykresli na Alt podtrzitko na nesmyslne
        // misto; resenim by bylo sejmout text do naseho bufferu a zobrazovat z neho
        // zvolil jsem jiny pristup a state si drzim u nas
        if (LOWORD(wParam) == UIS_CLEAR)
            UIState &= ~HIWORD(wParam);
        else if (LOWORD(wParam) == UIS_SET)
            UIState |= HIWORD(wParam);

        BOOL showAccel = (LOWORD(wParam) == UIS_CLEAR) && ((HIWORD(wParam) & UISF_HIDEACCEL) != 0);
        if (showAccel)
        {
            HDC hDC = HANDLES(GetDC(HWindow));
            OnPaint(hDC, FALSE, TRUE);
            HANDLES(ReleaseDC(HWindow, hDC));
        }
        return 0;
    }

    case WM_COMMAND:
    {
        if ((HWND)lParam == ToolBar->HWindow)
            PostMessage(HNotifyWindow, WM_COMMAND,
                        MAKEWPARAM((WORD)(UINT_PTR)GetMenu(HWindow), LOWORD(wParam)), (LPARAM)HWindow);
        break;
    }

    case WM_USER_TBGETTOOLTIP:
    {
        TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
        lstrcpy(tt->Buffer, LoadStr(TlbHdrTooltips[tt->ID - 1]));
        return TRUE;
    }

    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT r;
        GetClientRect(HWindow, &r);
        FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE + 1));
        return 1;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CAnimate
//

/*
CAnimate::CAnimate(HBITMAP hBitmap, int framesCount, int firstLoopFrame, COLORREF bkColor, CObjectOrigin origin)
 : CWindow(origin)
{
  HBitmap = hBitmap;
  FramesCount = framesCount;
  FirstLoopFrame = firstLoopFrame;
  HThread = NULL;
  CurrentFrame = 0;
  SleepThread = FALSE;
  NestedCount = 0;
  BkColor = bkColor;
  MouseIsTracked = FALSE;

  HANDLES(InitializeCriticalSection(&GDICriticalSection));
  HANDLES(InitializeCriticalSection(&DataCriticalSection));

  HRunEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
  if (HRunEvent == NULL)
    TRACE_E("Unable to create HRunEvent event.");

  HTerminateEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));  // "nonsignaled" state, manual
  if (HTerminateEvent == NULL)
    TRACE_E("Unable to create HTerminateEvent event.");

  if (HRunEvent != NULL && HTerminateEvent != NULL)
  {
    DWORD threadID;
    HThread = HANDLES(CreateThread(NULL, 0, AuxThreadF, this, 0, &threadID));
    if (HThread == NULL)
      TRACE_E("Unable to start thread for animation control.");
  }
  else HThread = NULL;

  BITMAP bitmap;
  GetObject(HWorkerBitmap, sizeof(bitmap), &bitmap);
  FrameSize.cx = bitmap.bmWidth;
  FrameSize.cy = bitmap.bmHeight / framesCount;
}

BOOL
CAnimate::IsGood()
{
  return HThread != NULL && HRunEvent != NULL && HTerminateEvent != NULL;
}

void
CAnimate::Paint(HDC hdc)
{
  // pro jistotu synchronizuji pristup k bitmape
  HANDLES(EnterCriticalSection(&GDICriticalSection));

  // pokud nam nepredaji dc, vytahneme si vlastni
  HDC hDC;
  if (hdc == NULL)
    hDC = HANDLES(GetDC(HWindow));
  else
    hDC = hdc;

  // memory dc pro BitBlt
  HDC hMemDC = HANDLES(CreateCompatibleDC(NULL));
  HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, HBitmap);

  RECT r;
  GetClientRect(HWindow, &r);
  BitBlt(hDC,
         (r.right - r.left - FrameSize.cx) / 2,
         (r.bottom - r.top - FrameSize.cy) / 2,
         FrameSize.cx,
         FrameSize.cy,
         hMemDC, 0,
         CurrentFrame * FrameSize.cy,
         SRCCOPY);

  // uklid
  SelectObject(hMemDC, hOldBitmap);
  HANDLES(DeleteDC(hMemDC));
  if (hdc == NULL)
    HANDLES(ReleaseDC(HWindow, hDC));
  HANDLES(LeaveCriticalSection(&GDICriticalSection));
}

void
CAnimate::GetFrameSize(SIZE *sz)
{
  *sz = FrameSize;
}

void
CAnimate::FirstFrame()
{
  CurrentFrame = 0;
}

void
CAnimate::NextFrame()
{
  CurrentFrame++;
  if (CurrentFrame >= FramesCount)
    CurrentFrame = FirstLoopFrame;
}

void
CAnimate::Start()
{
  HANDLES(EnterCriticalSection(&DataCriticalSection));
  NestedCount++;
  SleepThread = FALSE;
  SetEvent(HRunEvent);
  HANDLES(LeaveCriticalSection(&DataCriticalSection));
}

void
CAnimate::Stop()
{
  HANDLES(EnterCriticalSection(&DataCriticalSection));
  NestedCount--;
  if (NestedCount < 1)
  {
    if (NestedCount < 0)
    {
      TRACE_E("CAnimate::Stop() NestedCount = "<<NestedCount);
      NestedCount = 0;
    }
    SleepThread = TRUE;
  }
  HANDLES(LeaveCriticalSection(&DataCriticalSection));
}

unsigned
CAnimate::ThreadF(void *param)
{
  CALL_STACK_MESSAGE1("CAnimate::ThreadF()");
  SetThreadNameInVCAndTrace("Animate");
  TRACE_I("Begin");

  CAnimate *animate = (CAnimate *)param;

  HANDLE handles[2];
  handles[0] = animate->HTerminateEvent;  // musi byt na nulte pozici, protoze bude prioritni
  handles[1] = animate->HRunEvent;

  // aby animace nedrhla, zvedneme prioritu threadu 
  // muzeme si to dovolit, protoze nebereme temer zadny cas
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

  while (TRUE)
  {
    DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

    if (wait == WAIT_OBJECT_0)
      break;  // mame ukoncit thread

    if (animate->SleepThread)     // mame zastavit animaci
    {
      animate->FirstFrame();          // presuneme se na prvni policko
      ResetEvent(animate->HRunEvent); // a zakazeme beh
    }

    // vykreslime aktualni policko
    animate->Paint();
    // presuneme se na dalsi
    animate->NextFrame();
    // 1000ms / 40ms = 25 snimku za vterinu, jako ve filmu
    WaitForSingleObject(animate->HTerminateEvent, 40); // pokud mame koncit, nebudeme tu zbytecne cekat
  }
  TRACE_I("End");
  return 0;
}

unsigned
CAnimate::AuxThreadEH(void *param)
{
  CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
  __try
  {
#endif // CALLSTK_DISABLE
    return ThreadF(param);
#ifndef CALLSTK_DISABLE
  }
  __except (CCallStack::HandleException(GetExceptionInformation()))
  {
    TRACE_I("Thread in CAnimate: calling ExitProcess(1).");
//    ExitProcess(1);
    TerminateProcess(GetCurrentProcess(), 1);  // tvrdsi exit (tenhle jeste neco vola)
    return 1;
  }
#endif // CALLSTK_DISABLE
}


DWORD WINAPI
CAnimate::AuxThreadF(void *param)
{
  CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
  CCallStack stack;
#endif // CALLSTK_DISABLE
  return AuxThreadEH(param);
}

LRESULT
CAnimate::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("CAnimate::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

  switch (uMsg)
  {
    case WM_CREATE:
    {
      break;
    }

    case WM_DESTROY:
    {
      SetEvent(HTerminateEvent);
      WaitForSingleObject(HThread, INFINITE);  // pockame na konec threadu
      HANDLES(CloseHandle(HThread));
      HANDLES(DeleteCriticalSection(&GDICriticalSection));
      HANDLES(DeleteCriticalSection(&DataCriticalSection));
      HANDLES(CloseHandle(HRunEvent));
      HANDLES(CloseHandle(HTerminateEvent));
      break;
    }

    case WM_MOUSEMOVE:
    {
      SetCurrentToolTip(HWindow, 1);

      if (!MouseIsTracked)
      {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = HWindow;
        TrackMouseEvent(&tme);
        MouseIsTracked = TRUE;
      }

      break;
    }

    case WM_MOUSELEAVE:
    {
      SetCurrentToolTip(NULL, 0);
      MouseIsTracked = FALSE;
      break;
    }

    case WM_USER_TTGETTEXT:
    {
      char *text = (char *)lParam;
      lstrcpy(text, "(CAnimate class)\nClick to Start animate, click again to Stop animate.\n\t1\nTab\t2");
      return TRUE;
    }

    case WM_RBUTTONDOWN:
    {
      SetCurrentToolTip(NULL, 0);
      break;
    }

    case WM_RBUTTONUP:
    {
      SetCurrentToolTip(NULL, 0);
      CMenuPopup popup;
      BOOL runnig = WaitForSingleObject(HRunEvent, 0) == WAIT_OBJECT_0;
      MENU_ITEM_INFO mii;
      mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING | MENU_MASK_STATE;
      mii.Type = MENU_TYPE_STRING;
      mii.ID = 1;
      mii.String = "&Start";
      mii.State = runnig ? MENU_STATE_GRAYED : 0;
      popup.InsertItem(0xFFFFFFFF, TRUE, &mii);
      mii.ID = 2;
      mii.String = "S&top";
      mii.State = runnig ? 0 : MENU_STATE_GRAYED;
      popup.InsertItem(0xFFFFFFFF, TRUE, &mii);

      DWORD pos = GetMessagePos();
      DWORD cmd = popup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON, 
                              GET_X_LPARAM(pos), GET_Y_LPARAM(pos), HWindow, NULL);
      if (cmd == 1)
        Start();
      if (cmd == 2)
        Stop();
      return 0;
    }

    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    {
      SetCurrentToolTip(NULL, 0);
      if (WaitForSingleObject(HRunEvent, 0) == WAIT_OBJECT_0)
        Stop();
      else
        Start();
      break;
    }

    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
      Paint(hDC);
      HANDLES(EndPaint(HWindow, &ps));
      return 0;
    }

    case WM_ERASEBKGND:
    {
      HDC hDC = (HDC)wParam;
      RECT r;
      GetClientRect(HWindow, &r);
      COLORREF oldColor = (COLORREF)SetBkColor(hDC, BkColor);
      ExtTextOut((HDC)wParam, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
      SetBkColor(hDC, oldColor);
      return TRUE;
    }
  }

  return CWindow::WindowProc(uMsg, wParam, lParam);
}
*/

//
// ****************************************************************************
// ChangeToArrowButton
//

BOOL ChangeToArrowButton(HWND hParent, int ctrlID)
{
    CALL_STACK_MESSAGE_NONE
    // stary pristup nefungoval pri kontrastnich barvach, kde by se sipka mela vykreslit inverzne
    // prechazime na nase vlastni kresleni
    CButton* button = new CButton(hParent, ctrlID, BTF_RIGHTARROW);
    /*
  // pod XP neni BS_ICON kresleny pres theme, ma stary look
  // vykreslime si tedy tlacitko po nasem
  CButton *button = new CButton(hParent, ctrlID, 0);
  HWND hButton = GetDlgItem(hParent, ctrlID);
  if (hButton == NULL)
  {
    TRACE_E("Cannot find button ctrlID=" << ctrlID << " in the window hParent=0x" << hParent);
    return FALSE;
  }
  LONG_PTR l = GetWindowLongPtr(hButton, GWL_STYLE);
  l |= BS_ICON;
  SetWindowLongPtr(hButton, GWL_STYLE, l);
  SendMessage(hButton, BM_SETIMAGE, IMAGE_ICON, (WPARAM)HANDLES(LoadIcon(HInstance, MAKEINTRESOURCE(IDI_BROWSE))));
*/
    return TRUE;
}

BOOL ChangeToIconButton(HWND hParent, int ctrlID, int iconID)
{
    CALL_STACK_MESSAGE_NONE
    HWND hButton = GetDlgItem(hParent, ctrlID);
    if (hButton == NULL)
    {
        TRACE_E("Cannot find button ctrlID=" << ctrlID << " in the window hParent=0x" << hParent);
        return FALSE;
    }
    DWORD stl = (DWORD)GetWindowLongPtr(hButton, GWL_STYLE);
    stl |= BS_ICON;
    SetWindowLongPtr(hButton, GWL_STYLE, stl);
    SendMessage(hButton, BM_SETIMAGE, IMAGE_ICON, (WPARAM)HANDLES(LoadIcon(HInstance, MAKEINTRESOURCE(iconID))));

    // upravim velikost tlacitka podle predesle editline nebo comboboxu
    HWND hPrevWnd = GetWindow(hButton, GW_HWNDPREV);
    if (hPrevWnd != NULL)
    {
        char className[30];
        GetClassName(hPrevWnd, className, 29);
        className[29] = 0;
        if (stricmp(className, "edit") == 0 || stricmp(className, "combobox") == 0)
        {
            RECT r;
            GetWindowRect(hPrevWnd, &r);
            RECT r2;
            GetWindowRect(hButton, &r2);
            POINT p;
            p.x = r2.left;
            p.y = r.top;
            ScreenToClient(hParent, &p);
            SetWindowPos(hButton, NULL, p.x, p.y, r2.right - r2.left, r.bottom - r.top, SWP_NOZORDER);
        }
    }

    CButton* button = new CButton(hParent, ctrlID, BTF_LBUTTONDOWN | BTF_RIGHTARROW);
    if (button != NULL)
        button->SetToolTipText(LoadStr(IDS_BROWSE_BTN_TIP));

    return TRUE;
}

//
// ****************************************************************************
// VerticalAlignChildToChild
//

void VerticalAlignChildToChild(HWND hParent, int alignID, int toID)
{
    HWND hAlign = GetDlgItem(hParent, alignID);
    HWND hTo = GetDlgItem(hParent, toID);
    if (hParent == NULL || hAlign == NULL || hTo == NULL)
    {
        TRACE_E("VerticalAlignChildToChild() Invalid parameters! hParent=" << hParent << " alignID=" << alignID << " toID=" << toID);
        return;
    }

    RECT alignR;
    RECT toR;
    GetWindowRect(hAlign, &alignR);
    GetWindowRect(hTo, &toR);

    int width = alignR.right - alignR.left;
    int height = toR.bottom - toR.top;

    ScreenToClient(hParent, (LPPOINT)&alignR);
    ScreenToClient(hParent, (LPPOINT)&toR);

    SetWindowPos(hAlign, NULL, alignR.left, toR.top, width, height, SWP_NOZORDER);
}

//
//  ****************************************************************************
// CondenseStaticTexts
//

void CondenseStaticTexts(HWND hWindow, int* staticsArr)
{
    int count = 0;
    while (staticsArr[count] != 0)
        count++;
    if (count < 2)
        return; // neni co delat

    HFONT hFont = (HFONT)SendDlgItemMessage(hWindow, staticsArr[0], WM_GETFONT, 0, 0);
    HDC hDC = HANDLES(GetDC(hWindow));
    HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
    SIZE sz;
    GetTextExtentPoint32(hDC, " ", 1, &sz);
    int spaceWidth = sz.cx;
    int pos = -1;
    for (int i = 0; i < count; i++)
    {
        HWND control = GetDlgItem(hWindow, staticsArr[i]);
        if (control != NULL)
        {
            RECT r;
            GetWindowRect(control, &r);
            POINT p;
            p.x = r.left;
            p.y = r.top;
            ScreenToClient(hWindow, &p);
            WCHAR text[1000];
            GetWindowTextW(control, text, _countof(text));
            int textLen = (int)wcslen(text);
            if (textLen > 0 && text[textLen - 1] == ' ')
                text[--textLen] = 0;
            GetTextExtentPoint32W(hDC, text, textLen, &sz);
            if (pos == -1)
                pos = p.x;
            MoveWindow(control, pos, p.y, sz.cx, r.bottom - r.top, TRUE);
            pos += sz.cx + spaceWidth;
        }
        else
            TRACE_E("CondenseStaticTexts(): invalid control ID found in array with statics IDs: " << staticsArr[i]);
    }
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(hWindow, hDC));
}

//
//  ****************************************************************************
// ArrangeHorizontalLines
//

BOOL CALLBACK FindHorizLines(HWND hwnd, LPARAM lParam)
{
    RECT r;
    GetClientRect(hwnd, &r);
    if (r.bottom == 0) // horizontalni cara vysoka 0 bodu
    {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        if ((style & SS_TYPEMASK) == SS_ETCHEDHORZ)
        {
            char className[300];
            if (GetClassName(hwnd, className, _countof(className)) && stricmp(className, "Static") == 0)
                ((TDirectArray<HWND>*)lParam)->Add(hwnd);
        }
    }
    return TRUE;
}

BOOL CALLBACK FindGroupBoxes(HWND hwnd, LPARAM lParam)
{
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if ((style & BS_TYPEMASK) == BS_GROUPBOX)
    {
        char className[300];
        if (GetClassName(hwnd, className, _countof(className)) && stricmp(className, "Button") == 0)
            ((TDirectArray<HWND>*)lParam)->Add(hwnd);
    }
    return TRUE;
}

struct CDataForFindHorizLineLabel
{
    HWND Line;
    RECT LineRect;
    HWND Label;
    BOOL IsCheckOrRadioBox;
    BOOL NoPrefix;
    BOOL MoreLabels;
};

BOOL CALLBACK FindHorizLineLabel(HWND hwnd, LPARAM lParam)
{
    CDataForFindHorizLineLabel* data = (CDataForFindHorizLineLabel*)lParam;
    if (data->Line != hwnd) // preskocime caru, pro kterou hledame label
    {
        RECT r;
        GetWindowRect(hwnd, &r);
        if (r.top <= data->LineRect.top && r.bottom >= data->LineRect.bottom) // label vertikalne prekryva caru
        {
            if (r.left < data->LineRect.left && r.right < data->LineRect.right) // label zacina pred carou + cara konci az za labelem
            {
                char className[300];
                if (GetClassName(hwnd, className, _countof(className)))
                {
                    LONG style = GetWindowLong(hwnd, GWL_STYLE);
                    if (stricmp(className, "Static") == 0) // jde o levy static text
                    {
                        if ((style & SS_TYPEMASK) == SS_LEFT ||
                            (style & SS_TYPEMASK) == SS_SIMPLE ||
                            (style & SS_TYPEMASK) == SS_LEFTNOWORDWRAP)
                        {
                            data->IsCheckOrRadioBox = FALSE;
                            data->NoPrefix = (style & SS_NOPREFIX) != 0;
                            if (data->Label != NULL)
                                data->MoreLabels = TRUE;
                            else
                                data->Label = hwnd;
                        }
                    }
                    else
                    {
                        if (stricmp(className, "Button") == 0) // jde o button (check box, radio box nebo tlacitko)
                        {
                            if (((style & BS_TYPEMASK) == BS_CHECKBOX ||
                                 (style & BS_TYPEMASK) == BS_AUTOCHECKBOX ||
                                 (style & BS_TYPEMASK) == BS_AUTO3STATE ||
                                 (style & BS_TYPEMASK) == BS_3STATE ||
                                 (style & BS_TYPEMASK) == BS_RADIOBUTTON ||
                                 (style & BS_TYPEMASK) == BS_AUTORADIOBUTTON) &&
                                (style & BS_PUSHLIKE) == 0)
                            {
                                data->IsCheckOrRadioBox = TRUE;
                                data->NoPrefix = FALSE;
                                if (data->Label != NULL)
                                    data->MoreLabels = TRUE;
                                else
                                    data->Label = hwnd;
                            }
                        }
                    }
                }
            }
        }
    }
    return TRUE;
}

struct CDataForFindGroupBoxLabel
{
    HWND GroupBox;
    RECT GroupBoxRect;
    HWND Label;
    BOOL MoreLabels;
};

BOOL CALLBACK FindGroupBoxLabel(HWND hwnd, LPARAM lParam)
{
    CDataForFindGroupBoxLabel* data = (CDataForFindGroupBoxLabel*)lParam;
    if (data->GroupBox != hwnd) // preskocime groupbox, pro ktery hledame label
    {
        RECT r;
        GetWindowRect(hwnd, &r);
        if (r.top <= data->GroupBoxRect.top && r.bottom >= data->GroupBoxRect.top &&  // label vertikalne prekryva vrchni caru groupboxu
            r.left >= data->GroupBoxRect.left && r.right <= data->GroupBoxRect.right) // label horizontalne lezi na groupboxu
        {
            char className[300];
            if (GetClassName(hwnd, className, _countof(className)) &&
                stricmp(className, "Button") == 0) // jde o button (check box, radio box nebo tlacitko)
            {
                LONG style = GetWindowLong(hwnd, GWL_STYLE);
                if (((style & BS_TYPEMASK) == BS_CHECKBOX ||
                     (style & BS_TYPEMASK) == BS_AUTOCHECKBOX ||
                     (style & BS_TYPEMASK) == BS_AUTO3STATE ||
                     (style & BS_TYPEMASK) == BS_3STATE ||
                     (style & BS_TYPEMASK) == BS_RADIOBUTTON ||
                     (style & BS_TYPEMASK) == BS_AUTORADIOBUTTON) &&
                    (style & BS_PUSHLIKE) == 0)
                {
                    if (data->Label != NULL)
                        data->MoreLabels = TRUE;
                    else
                        data->Label = hwnd;
                }
            }
        }
    }
    return TRUE;
}

void ArrangeHorizontalLines(HWND hWindow)
{
    TDirectArray<HWND> horizLines(10, 5);
    EnumChildWindows(hWindow, FindHorizLines, (LPARAM)&horizLines);

    HDC hDC = HANDLES(GetDC(hWindow));
    int spaceWidth = -1;
    RECT windowRect;
    GetWindowRect(hWindow, &windowRect);
    for (int i = 0; i < horizLines.Count; i++)
    {
        CDataForFindHorizLineLabel data;
        data.Line = horizLines[i];
        data.Label = NULL;
        data.MoreLabels = FALSE;
        data.IsCheckOrRadioBox = FALSE;
        data.NoPrefix = FALSE;
        GetWindowRect(data.Line, &data.LineRect);
        EnumChildWindows(hWindow, FindHorizLineLabel, (LPARAM)&data);
        if (data.Label != NULL)
        {
            if (data.MoreLabels)
                TRACE_E("ArrangeHorizontalLines(): unexpected situation: more labels for one horizontal line!");
            else
            {
                HFONT hFont = (HFONT)SendMessage(data.Label, WM_GETFONT, 0, 0);
                HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
                if (spaceWidth == -1)
                {
                    SIZE sz;
                    GetTextExtentPoint32(hDC, " ", 1, &sz);
                    spaceWidth = sz.cx;
                }
                RECT labelRect;
                GetWindowRect(data.Label, &labelRect);
                WCHAR text[1000];
                GetWindowTextW(data.Label, text, _countof(text));
                int textLen = (int)wcslen(text);
                if (textLen > 0 && text[textLen - 1] == ' ')
                    text[--textLen] = 0;
                DWORD dtFlags = DT_CALCRECT | DT_LEFT | DT_SINGLELINE | (data.NoPrefix ? DT_NOPREFIX : 0);
                RECT txtR = labelRect;
                txtR.right -= txtR.left;
                txtR.bottom -= txtR.top;
                txtR.left = txtR.top = 0;
                DrawTextW(hDC, text, textLen, &txtR, dtFlags);
                SelectObject(hDC, hOldFont);

                POINT p;
                p.x = labelRect.left;
                p.y = labelRect.top;
                ScreenToClient(hWindow, &p);
                POINT p2;
                p2.x = data.LineRect.left;
                p2.y = data.LineRect.top;
                ScreenToClient(hWindow, &p2);

                if (labelRect.right + 5 * spaceWidth < data.LineRect.left)
                    TRACE_E("ArrangeHorizontalLines(): unexpected situation: horizontal line begins more than five spaces behind label, ignoring label...");
                else // zkratim label, aby neprekryval caru a caru dotahnu ke konci labelu
                {
                    if (data.IsCheckOrRadioBox)
                    {
                        LOGFONT lf;
                        GetObject(hFont, sizeof(LOGFONT), &lf);
                        // na Win7 jsem zjistil, ze velikosti symbolu (napr. radio/check box) se meni
                        // jen pro 100%, 125%, 150% a 200% DPI, mezilehla DPI pouzivaji vzdy symboly
                        // z nizsiho "celeho" DPI, pro ne omerime vsechny mezilehle velikosti fontu,
                        // tim bysme meli postihnout vsechna mozna DPI
                        int boxSize = -lf.lfHeight < 13 ? 16 : // < 125% DPI
                                          -lf.lfHeight < 16 ? 20
                                                            : // < 150% DPI
                                          -lf.lfHeight < 21 ? 25
                                                            : 27; // < 200% DPI : == 200% DPI
                        if (p2.x + (data.LineRect.right - data.LineRect.left) > p.x + boxSize + txtR.right + 2 * spaceWidth)
                        {
                            MoveWindow(data.Label, p.x, p.y, boxSize + txtR.right + spaceWidth, labelRect.bottom - labelRect.top, TRUE);
                            MoveWindow(data.Line, p.x + boxSize + txtR.right + 2 * spaceWidth, p2.y,
                                       p2.x + (data.LineRect.right - data.LineRect.left) - (p.x + boxSize + txtR.right + 2 * spaceWidth),
                                       data.LineRect.bottom - data.LineRect.top, TRUE);
                        }
                    }
                    else
                    {
                        if (p2.x + (data.LineRect.right - data.LineRect.left) > p.x + txtR.right + 2 * spaceWidth)
                        {
                            MoveWindow(data.Label, p.x, p.y, txtR.right + spaceWidth, labelRect.bottom - labelRect.top, TRUE);
                            MoveWindow(data.Line, p.x + txtR.right + 2 * spaceWidth, p2.y,
                                       p2.x + (data.LineRect.right - data.LineRect.left) - (p.x + txtR.right + 2 * spaceWidth),
                                       data.LineRect.bottom - data.LineRect.top, TRUE);
                        }
                    }
                }
            }
        }
        else
        {
            POINT p;
            p.x = data.LineRect.left;
            p.y = data.LineRect.top;
            ScreenToClient(hWindow, &p);
            RECT rect = {0, 0, 20, 1};
            MapDialogRect(hWindow, &rect);
            if (p.x > rect.right)
                TRACE_E("ArrangeHorizontalLines(): label not found, but line begins more than 20 dlg-units from left side of dialog!");
        }
    }

    // zarovnani checkboxu a radioboxu slouzicich jako labely na groupboxech
    horizLines.DestroyMembers();
    EnumChildWindows(hWindow, FindGroupBoxes, (LPARAM)&horizLines);
    for (int i = 0; i < horizLines.Count; i++)
    {
        CDataForFindGroupBoxLabel data;
        data.GroupBox = horizLines[i];
        data.Label = NULL;
        data.MoreLabels = FALSE;
        GetWindowRect(data.GroupBox, &data.GroupBoxRect);
        EnumChildWindows(hWindow, FindGroupBoxLabel, (LPARAM)&data);
        if (data.Label != NULL)
        {
            if (data.MoreLabels)
                TRACE_E("ArrangeHorizontalLines(): unexpected situation: more labels for one group box!");
            else
            {
                HFONT hFont = (HFONT)SendMessage(data.Label, WM_GETFONT, 0, 0);
                HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
                if (spaceWidth == -1)
                {
                    SIZE sz;
                    GetTextExtentPoint32(hDC, " ", 1, &sz);
                    spaceWidth = sz.cx;
                }
                RECT labelRect;
                GetWindowRect(data.Label, &labelRect);
                WCHAR text[1000];
                GetWindowTextW(data.Label, text, _countof(text));
                int textLen = (int)wcslen(text);
                if (textLen > 0 && text[textLen - 1] == ' ')
                    text[--textLen] = 0;
                DWORD dtFlags = DT_CALCRECT | DT_LEFT | DT_SINGLELINE;
                RECT txtR = labelRect;
                txtR.right -= txtR.left;
                txtR.bottom -= txtR.top;
                txtR.left = txtR.top = 0;
                DrawTextW(hDC, text, textLen, &txtR, dtFlags);
                SelectObject(hDC, hOldFont);

                POINT p;
                p.x = labelRect.left;
                p.y = labelRect.top;
                ScreenToClient(hWindow, &p);
                // zkratim checkbox nebo radiobox podle obsahu a aktualniho pisma
                LOGFONT lf;
                GetObject(hFont, sizeof(LOGFONT), &lf);
                // na Win7 jsem zjistil, ze velikosti symbolu (napr. radio/check box) se meni
                // jen pro 100%, 125%, 150% a 200% DPI, mezilehla DPI pouzivaji vzdy symboly
                // z nizsiho "celeho" DPI, pro ne omerime vsechny mezilehle velikosti fontu,
                // tim bysme meli postihnout vsechna mozna DPI
                int boxSize = -lf.lfHeight < 13 ? 16 : // < 125% DPI
                                  -lf.lfHeight < 16 ? 20
                                                    : // < 150% DPI
                                  -lf.lfHeight < 21 ? 25
                                                    : 27; // < 200% DPI : == 200% DPI
                MoveWindow(data.Label, p.x, p.y, boxSize + txtR.right + 2 * spaceWidth, labelRect.bottom - labelRect.top, TRUE);
            }
        }
    }
    HANDLES(ReleaseDC(hWindow, hDC));
}

//
//  ****************************************************************************
// GetWindowFontHeight
//

int GetWindowFontHeight(HWND hWindow)
{
    HFONT hFont = (HFONT)SendMessage(hWindow, WM_GETFONT, 0, 0);
    LOGFONT lf;
    if (GetObject(hFont, sizeof(lf), &lf) == 0)
    {
        DWORD err = GetLastError();
        TRACE_E("GetObject() failed! err=" << err);
        return -12;
    }
    return lf.lfHeight;
    //  metoda ziskani velikosti pres GetTextMetrics() vraci pod W7 pri 150% DPI velikost fontu v edit line
    //  -19, zatimco GetObject() vraci -16, coz je spravne; pokud pres CreateFont() vytvorim font
    //  pro -19, lezou prilis velke texty
    //  TEXTMETRIC tm;
    //  HDC hDC = HANDLES(GetDC(hWindow));
    //  HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
    //  GetTextMetrics(hDC, &tm);
    //  SelectObject(hDC, hOldFont);
    //  HANDLES(ReleaseDC(hWindow, hDC));
    //  return tm.tmHeight;
}

//
//  ****************************************************************************
// CreateCheckboxImagelist()
//

HIMAGELIST CreateCheckboxImagelist(int itemSize)
{
    HIMAGELIST hIL = ImageList_Create(itemSize, itemSize, GetImageListColorFlags() | ILC_MASK, 0, 1);

    HDC hDC = HANDLES(GetDC(NULL));
    HBITMAP hBitmap = HANDLES(CreateCompatibleBitmap(hDC, itemSize, itemSize));
    HDC hMemDC = HANDLES(CreateCompatibleDC(hDC));
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    RECT r = {0, 0, itemSize, itemSize};

    BOOL fallBack = TRUE;
    if (IsAppThemed())
    {
        HTHEME hTheme = OpenThemeData(NULL, L"BUTTON");
        if (hTheme != NULL)
        {
            FillRect(hMemDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
            SIZE sz;
            GetThemePartSize(hTheme, hMemDC, BP_CHECKBOX, CBS_CHECKEDNORMAL, NULL, TS_TRUE, &sz);
            if (sz.cx < r.right && sz.cy < r.bottom)
            {
                r.left = (r.right - sz.cx) / 2;
                r.top = (r.bottom - sz.cy) / 2;
                r.right = r.left + sz.cx;
                r.bottom = r.top + sz.cy;
            }
            DrawThemeBackground(hTheme, hMemDC, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, &r, NULL);
            SelectObject(hMemDC, hOldBitmap);
            ImageList_Add(hIL, hBitmap, NULL);

            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
            DrawThemeBackground(hTheme, hMemDC, BP_CHECKBOX, CBS_CHECKEDNORMAL, &r, NULL);
            SelectObject(hMemDC, hOldBitmap);
            ImageList_Add(hIL, hBitmap, NULL);

            CloseThemeData(hTheme);
            fallBack = FALSE;
        }
    }
    if (fallBack)
    {
        FillRect(hMemDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        DrawFrameControl(hMemDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK);
        SelectObject(hMemDC, hOldBitmap);
        ImageList_Add(hIL, hBitmap, NULL);

        hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
        FillRect(hMemDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        DrawFrameControl(hMemDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED);
        SelectObject(hMemDC, hOldBitmap);
        ImageList_Add(hIL, hBitmap, NULL);
    }

    HANDLES(DeleteObject(hBitmap));
    HANDLES(DeleteDC(hMemDC));
    HANDLES(ReleaseDC(NULL, hDC));

    return hIL;
}

//
//  ****************************************************************************
// SalLoadIcon()
//

HICON SalLoadIcon(HINSTANCE hInst, LPCTSTR iconName, CIconSizeEnum iconSize)
{
    int width = IconSizes[iconSize];
    HICON hIcon = NULL;
    HRESULT hres = LoadIconWithScaleDown(hInst, (PCWSTR)iconName, width, width, &hIcon);
    if (hres != S_OK)
    {
        DWORD err = GetLastError();
        TRACE_E("LoadIconWithScaleDown() failed. hInst=" << hInst << " iconName=" << iconName << " err=" << err);
    }
    else
    {
        HANDLES_ADD(__htIcon, __hoLoadIcon, hIcon);
    }
    return hIcon;
}
