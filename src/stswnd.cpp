// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "toolbar.h"
#include "cfgdlg.h"
#include "mainwnd.h"
#include "stswnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "shellib.h"
#include "svg.h"

//
// ****************************************************************************
// CStatusWindow
//

CStatusWindow::CStatusWindow(CFilesWindow* filesWindow, int border, CObjectOrigin origin) : CWindow(origin), HotTrackItems(10, 5)
{
    CALL_STACK_MESSAGE_NONE
    Text = NULL;
    AlpDX = NULL;
    Allocated = 0;
    PathLen = -1;
    TextLen = 0;
    Border = border;
    Size = NULL;
    Hidden = FALSE;
    History = FALSE;
    ShowThrobber = FALSE;
    DelayedThrobber = FALSE;
    DelayedThrobberShowTime = 0;
    Throbber = FALSE;
    ThrobberTooltip = NULL;
    ThrobberID = -1;
    Security = sisNone;
    SecurityTooltip = NULL;
    HiddenFilesCount = 0;
    HiddenDirsCount = 0;
    Left = TRUE; // dummy
    ToolBar = NULL;
    ToolBarWidth = 0;
    EllipsedChars = -1;
    EllipsedWidth = -1;
    NeedToInvalidate = FALSE;
    Width = 0;
    Height = 0;
    HotItem = NULL;
    LastHotItem = NULL;
    HotSize = FALSE;
    HotHistory = FALSE;
    HotZoom = FALSE;
    HotHidden = FALSE;
    HotSecurity = FALSE;
    MouseCaptured = FALSE;
    FilesWindow = filesWindow;
    LButtonDown = FALSE;
    RButtonDown = FALSE;
    SubTexts = NULL;
    SubTextsCount = 0;
    IDropTargetPtr = NULL;
}

CStatusWindow::~CStatusWindow()
{
    CALL_STACK_MESSAGE1("CStatusWindow::~CStatusWindow()");
    if (SubTexts != NULL)
        free(SubTexts);
    if (Text != NULL)
        free(Text);
    if (AlpDX != NULL)
        free(AlpDX);
    if (Size != NULL)
        free(Size);
    if (ThrobberTooltip != NULL)
        free(ThrobberTooltip);
    if (SecurityTooltip != NULL)
        free(SecurityTooltip);
    if (ToolBar != NULL)
    {
        if (ToolBar->HWindow != NULL)
            ToolBar->DetachWindow();
        delete ToolBar;
    }
}

BOOL CStatusWindow::SetSubTexts(DWORD* subTexts, DWORD subTextsCount)
{
    CALL_STACK_MESSAGE2("CStatusWindow::SetSubTexts(, %u)", subTextsCount);
    HotItem = NULL;
    LastHotItem = NULL;
    if (SubTexts != NULL)
    {
        SubTextsCount = 0;
        free(SubTexts);
    }

    if (subTexts == NULL || subTextsCount == 0)
        return TRUE;

    SubTexts = (DWORD*)malloc(subTextsCount * sizeof(DWORD));
    if (SubTexts == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    memmove(SubTexts, subTexts, subTextsCount * sizeof(DWORD));
    SubTextsCount = subTextsCount;

    // nechame sestavit pole pro sledovani kurzoru
    BuildHotTrackItems();

    return TRUE;
}

BOOL CStatusWindow::SetText(const char* txt, int pathLen)
{
    CALL_STACK_MESSAGE3("CStatusWindow::SetText(%s, %d)", txt, pathLen);
    if (Text != NULL && strcmp(Text, txt) == 0)
    {
        PathLen = pathLen;
        return TRUE;
    }
    HotTrackItemsMeasured = FALSE;
    HotItem = NULL;
    LastHotItem = NULL;

    int l = (int)strlen(txt) + 1;
    if (Allocated < l)
    {
        char* newText = (char*)realloc(Text, l);
        int* newAlpDX = (int*)realloc(AlpDX, l * sizeof(int));
        if (newText == NULL || newAlpDX == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        Text = newText;
        AlpDX = newAlpDX;
        Allocated = l;
    }
    memmove(Text, txt, l);
    PathLen = pathLen;
    TextLen = l - 1;

    if (SubTexts != NULL)
    {
        SubTextsCount = 0;
        free(SubTexts);
        SubTexts = NULL;
    }

    // nechame sestavit pole pro sledovani kurzoru
    BuildHotTrackItems();

    if (MouseCaptured)
        WindowProc(WM_MOUSELEAVE, 0, 0);

    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
    return TRUE;
}

void CStatusWindow::BuildHotTrackItems()
{
    CALL_STACK_MESSAGE1("CStatusWindow::BuildHotTrackItems()");

    HDC dc = HANDLES(GetDC(HWindow));
    HFONT oldFont = (HFONT)SelectObject(dc, EnvFont);

    HotItem = NULL;
    LastHotItem = NULL;
    if (Border == blTop)
    {
        // naplnim HotTrackItems
        CHotTrackItem item;
        HotTrackItems.DestroyMembers();
        if (Text != NULL)
        {
            // zde nam to padalo v SS2.0:execution address = 0x7800D9B0
            // doslo k zavolani strlen v pripade, ze Text byl jeste NULL
            int pathLen = (PathLen != -1) ? PathLen : (int)strlen(Text);
            // ziskame pozice vsech znaku
            SIZE s;
            GetTextExtentExPoint(dc, Text, TextLen, 0, NULL, AlpDX, &s);

            if (FilesWindow->Is(ptDisk) || FilesWindow->Is(ptZIPArchive))
            {
                int chars;
                if (Text[0] == '\\' && Text[1] == '\\' &&
                    (Text[2] != '.' || Text[3] != '\\' || Text[4] == 0 || Text[5] != ':') &&
                    Plugins.GetFirstNethoodPluginFSName())
                {
                    chars = 2;
                }
                else
                {
                    char rootPath[MAX_PATH];
                    GetRootPath(rootPath, Text);
                    chars = (int)strlen(rootPath);

                    // u UNC upicnu posledni zpetne lomitko
                    BOOL isDotDriveFormat = Text[0] == '\\' && Text[1] == '\\' && Text[2] == '.' &&
                                            Text[3] == '\\' && Text[4] != 0 && Text[5] == ':';
                    if (chars > pathLen || !isDotDriveFormat && chars > 3)
                        chars--;
                }

                BOOL exit;
                do
                {
                    item.Offset = 0;
                    item.PixelsOffset = 0;
                    item.Chars = chars;
                    item.Pixels = chars != 0 ? (WORD)AlpDX[chars - 1] : 0;
                    HotTrackItems.Add(item);

                    if (Text[chars] == '\\')
                        chars++;

                    exit = TRUE;
                    while (chars < pathLen)
                    {
                        exit = FALSE;
                        if (Text[chars] == '\\')
                            break;
                        chars++;
                    }
                } while (!exit);
            }
            else
            {
                if (FilesWindow->Is(ptPluginFS))
                {
                    int chars = 0;
                    while (1)
                    {
                        int lastChars = chars;
                        if (!FilesWindow->GetPluginFS()->GetNextDirectoryLineHotPath(Text, pathLen, chars))
                        {
                            chars = pathLen;
                        }
                        if (chars == lastChars)
                            chars++; // to by byla nekonecna smycka, radeji osetrime...
                        if (chars > pathLen)
                            chars = pathLen;

                        item.Offset = 0;
                        item.PixelsOffset = 0;
                        item.Chars = chars;
                        item.Pixels = chars != 0 ? (WORD)AlpDX[chars - 1] : 0;
                        HotTrackItems.Add(item);

                        if (chars == pathLen)
                            break;
                    }
                }
            }
            HotTrackItemsMeasured = TRUE;
        }
    }
    if (Border == blBottom)
    {
        // naplnim HotTrackItems
        CHotTrackItem item;
        HotTrackItems.DestroyMembers();
        if (Text != NULL)
        {
            // ziskame pozice vsech znaku
            SIZE s;
            GetTextExtentExPoint(dc, Text, TextLen, 0, NULL, AlpDX, &s);

            DWORD len = TextLen;
            SIZE sOffset;
            SIZE sSub;
            DWORD i;
            for (i = 0; i < (DWORD)SubTextsCount; i++)
            {
                WORD charOffset = LOWORD(SubTexts[i]);
                WORD charLen = HIWORD(SubTexts[i]);
                if (charOffset + charLen > (WORD)len)
                {
                    TRACE_E("charOffset + charLen >= len");
                    continue;
                }
                GetTextExtentPoint32(dc, Text, charOffset, &sOffset);
                GetTextExtentPoint32(dc, Text + charOffset, charLen, &sSub);
                item.PixelsOffset = (WORD)sOffset.cx;
                item.Pixels = (WORD)sSub.cx;
                item.Offset = charOffset;
                item.Chars = charLen;
                HotTrackItems.Add(item);
                HotTrackItemsMeasured = TRUE;
            }
        }
    }
    SelectObject(dc, oldFont);
    HANDLES(ReleaseDC(HWindow, dc));
}

void CStatusWindow::DestroyWindow()
{
    CALL_STACK_MESSAGE1("CStatusWindow::DestroyWindow()");
    if (ToolBar != NULL)
    {
        if (ToolBar->HWindow != NULL)
            ToggleToolBar();
        delete ToolBar;
        ToolBar = NULL;
    }
    if (Throbber || DelayedThrobber)
        SetThrobber(FALSE, 0, TRUE); // potrebujeme sestrelit timer

    ::DestroyWindow(HWindow);
}

void CStatusWindow::SetHidden(int hiddenFilesCount, int hiddenDirsCount)
{
    CALL_STACK_MESSAGE_NONE
    BOOL hidden = hiddenFilesCount != 0 || hiddenDirsCount != 0;
    HiddenFilesCount = hiddenFilesCount;
    HiddenDirsCount = hiddenDirsCount;
    if (Hidden != hidden)
    {
        Hidden = hidden;
        if (HWindow != NULL)
        {
            NeedToInvalidate = TRUE;
            InvalidateIfNeeded();
        }
    }
}

void CStatusWindow::SetHistory(BOOL history)
{
    CALL_STACK_MESSAGE_NONE
    if (History != history)
    {
        History = history;
        if (HWindow != NULL)
        {
            NeedToInvalidate = TRUE;
            InvalidateIfNeeded();
        }
    }
}

void CStatusWindow::SetThrobber(BOOL show, int delay, BOOL calledFromDestroyWindow)
{
    CALL_STACK_MESSAGE_NONE
    if (!calledFromDestroyWindow)
        ShowThrobber = show;
    if (show)
    {
        if (DelayedThrobber) // ceka se na zobrazeni
        {
            if (HWindow == NULL)
                TRACE_E("Unexpected situation in CStatusWindow::SetThrobber(): DelayedThrobber is TRUE but HWindow is NULL");
            if (Throbber)
                TRACE_E("Unexpected situation in CStatusWindow::SetThrobber(): DelayedThrobber and Throbber are both TRUE");
            KillTimer(HWindow, IDT_DELAYEDTHROBBER);
            if (Throbber /* jen korekce nekonzistentniho stavu */ ||
                delay <= 0 || !SetTimer(HWindow, IDT_DELAYEDTHROBBER, delay, NULL))
            {
                DelayedThrobber = FALSE; // ma se zobrazit hned nebo doslo k chybe pri nastavovani timeru, takze ho zobrazime hned
                DelayedThrobberShowTime = 0;
            }
            else
            {
                DelayedThrobberShowTime = GetTickCount() + delay;
                if (DelayedThrobberShowTime == 0)
                    DelayedThrobberShowTime++; // 0 je neplatna hodnota
            }
        }
        else
        {
            if (!Throbber && delay > 0)
            {
                if (HWindow != NULL && SetTimer(HWindow, IDT_DELAYEDTHROBBER, delay, NULL))
                    DelayedThrobber = TRUE; // neni zobrazen + ma se zobrazit se zpozdenim + okno je videt (pokud neni, napocita se jen DelayedThrobberShowTime)
                DelayedThrobberShowTime = GetTickCount() + delay;
                if (DelayedThrobberShowTime == 0)
                    DelayedThrobberShowTime++; // 0 je neplatna hodnota
            }
        }
    }
    else
    {
        if (DelayedThrobber) // ceka se na zobrazeni, ale throbber se ma schovat, konec cekani
        {
            if (HWindow == NULL)
                TRACE_E("Unexpected situation 2 in CStatusWindow::SetThrobber(): DelayedThrobber is TRUE but HWindow is NULL");
            KillTimer(HWindow, IDT_DELAYEDTHROBBER);
            DelayedThrobber = FALSE;
        }
        if (!calledFromDestroyWindow)
            DelayedThrobberShowTime = 0;
    }
    if (HWindow == NULL && Throbber)
    {
        Throbber = FALSE;
        TRACE_E("Unexpected situation in CStatusWindow::SetThrobber(): Throbber is TRUE but HWindow is NULL");
    }
    if (HWindow != NULL && !DelayedThrobber && Throbber != show)
    {
        Throbber = show;

        if (Throbber)
        {
            ThrobberFrame = 0; // zaciname -> budeme animovat od prvniho policka
            SetTimer(HWindow, IDT_THROBBER, IDT_THROBBER_DELAY, NULL);
        }
        else
        {
            KillTimer(HWindow, IDT_THROBBER);
        }

        if (StopStatusbarRepaint == 0)
        {
            NeedToInvalidate = TRUE;
            InvalidateIfNeeded();
        }
        else
        {
            PostStatusbarRepaint = TRUE;
        }
    }
}

void CStatusWindow::SetThrobberTooltip(const char* throbberTooltip)
{
    if (ThrobberTooltip != NULL)
    {
        free(ThrobberTooltip);
        ThrobberTooltip = NULL;
    }
    if (throbberTooltip != NULL)
        ThrobberTooltip = DupStr(throbberTooltip);
}

void CStatusWindow::SetSecurity(CSecurityIconState iconState)
{
    CALL_STACK_MESSAGE_NONE
    if (Security != iconState)
    {
        Security = iconState;
        if (HWindow != NULL)
        {
            NeedToInvalidate = TRUE;
            InvalidateIfNeeded();
        }
    }
}

void CStatusWindow::SetSecurityTooltip(const char* tooltip)
{
    if (SecurityTooltip != NULL)
    {
        free(SecurityTooltip);
        SecurityTooltip = NULL;
    }
    if (tooltip != NULL)
        SecurityTooltip = DupStr(tooltip);
}

int CStatusWindow::ChangeThrobberID()
{
    static int NewID = 0; // id throbberu musi byt unikatni (tzn. jediny counter pro oba panely)
    ThrobberID = NewID++;
    if (ThrobberID == -1)
        ThrobberID = NewID++;
    return ThrobberID;
}

void CStatusWindow::HideThrobberAndSecurityIcon()
{
    SetThrobber(FALSE);
    SetThrobberTooltip(NULL);
    SetSecurity(sisNone);
    SetSecurityTooltip(NULL);
}

void CStatusWindow::InvalidateIfNeeded()
{
    CALL_STACK_MESSAGE_NONE
    if (NeedToInvalidate)
    {
        NeedToInvalidate = FALSE;
        if (HWindow != NULL)
            InvalidateRect(HWindow, NULL, TRUE);
    }
}

int CStatusWindow::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    int height = 2 + EnvFontCharHeight + 2;
    if (Border & blTop)
    {
        height += 2 + 2;
        //    int needed = ToolBar->GetNeededHeight();
        int needed = 3 + 16 + 3;
        if (height < needed)
            height = needed;
    }
    if (Border & blBottom)
        height++;
    return height;
}

void CStatusWindow::SetSize(const CQuadWord& size)
{
    CALL_STACK_MESSAGE_NONE
    if (Size == NULL)
    {
        Size = (char*)malloc(30);
        Size[0] = 0;
    }
    if (Size != NULL)
    {
        if (size == CQuadWord(-1, -1))
            Size[0] = 0;
        else
        {
            char buf[100];
            PrintDiskSize(buf, size, 0);
            if (strcmp(buf, Size) == 0)
                return;
            else
                strcpy(Size, buf);
        }
    }
    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
}

void CStatusWindow::SetLeftPanel(BOOL left)
{
    CALL_STACK_MESSAGE_NONE
    Left = left;
    if (ToolBar != NULL)
    {
        ToolBar->SetType(Left ? mtbtLeft : mtbtRight);
        ToolBar->Load(Left ? Configuration.LeftToolBar : Configuration.RightToolBar);
    }
}

BOOL CStatusWindow::ToggleToolBar()
{
    CALL_STACK_MESSAGE1("CStatusWindow::ToggleToolBar()");
    if (ToolBar == NULL)
        return FALSE;
    if (ToolBar->HWindow != NULL)
    {
        ::DestroyWindow(ToolBar->HWindow);
        return TRUE;
    }
    else
    {
        if (!ToolBar->CreateWnd(HWindow))
            return FALSE;
        ToolBar->SetImageList(HGrayToolBarImageList);
        ToolBar->SetHotImageList(HHotToolBarImageList);
        ToolBar->SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_ADJUSTABLE);
        ToolBar->Load(Left ? Configuration.LeftToolBar : Configuration.RightToolBar);
        SendMessage(ToolBar->HWindow, TB_SETPARENT, (WPARAM)MainWindow->HWindow, 0);
        ShowWindow(ToolBar->HWindow, SW_SHOW);
        return TRUE;
    }
    return TRUE;
}

BOOL CStatusWindow::SetDriveIcon(HICON hIcon)
{
    CALL_STACK_MESSAGE_NONE
    if (ToolBar != NULL && ToolBar->HWindow != NULL)
        ToolBar->ReplaceImage(Left ? CM_LCHANGEDRIVE : CM_RCHANGEDRIVE, FALSE, hIcon, TRUE, TRUE);
    return TRUE;
}

void CStatusWindow::SetDrivePressed(BOOL pressed)
{
    CALL_STACK_MESSAGE_NONE
    if (ToolBar != NULL && ToolBar->HWindow != NULL)
    {
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_STATE;
        tii.State = pressed ? TLBI_STATE_PRESSED : 0;
        ToolBar->SetItemInfo2(Left ? CM_LCHANGEDRIVE : CM_RCHANGEDRIVE, FALSE, &tii);
        UpdateWindow(ToolBar->HWindow);
    }
}

void CStatusWindow::LayoutWindow()
{
    CALL_STACK_MESSAGE_NONE
    SendMessage(HWindow, WM_SIZE, 0, 0);
    InvalidateRect(HWindow, NULL, TRUE);
    UpdateWindow(HWindow);
}

void CStatusWindow::GetHotText(char* buffer, int bufSize)
{
    CALL_STACK_MESSAGE_NONE
    if (HotItem != NULL && Text != NULL)
    {
        lstrcpyn(buffer, Text + HotItem->Offset, min(HotItem->Chars + 1, bufSize));
        // u Directory Line s pluginovym FS je jeste potreba umoznit pluginu posledni upravy cesty (pridani ']' u VMS cest u FTP)
        if ((Border & blTop) && FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->NotEmpty())
            FilesWindow->GetPluginFS()->CompleteDirectoryLineHotPath(buffer, bufSize);
    }
    else
        buffer[0] = 0;
}

BOOL CStatusWindow::FindHotTrackItem(int xPos, int& index)
{
    CALL_STACK_MESSAGE_NONE
    int i;
    for (i = 0; i < HotTrackItems.Count; i++)
    {
        CHotTrackItem* item = &HotTrackItems[i];

        // pokud je za root slozkou vypustka, musime o ni posunout xPos
        if (i == 1 && EllipsedWidth != -1)
            xPos += EllipsedWidth - TextEllipsisWidthEnv;

        if (xPos >= item->PixelsOffset && xPos < item->PixelsOffset + item->Pixels)
        {
            index = i;
            return TRUE;
        }
    }
    return FALSE;
}

void CStatusWindow::FlashText(BOOL hotTrackOnly)
{
    CALL_STACK_MESSAGE_NONE
    Repaint(TRUE, hotTrackOnly);
    Sleep(100);
    Repaint(FALSE, hotTrackOnly);
}

void PaintSymbol(HDC hDC, HDC hMemDC, HBITMAP hBitmap, int xOffset, int width, int height, const RECT* rect, BOOL hot, BOOL activeCaption)
{
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    COLORREF textColor;
    if (hot)
    {
        if (Configuration.ShowPanelCaption)
            textColor = GetCOLORREF(CurrentColors[activeCaption ? HOT_ACTIVE : HOT_INACTIVE]);
        else
            textColor = GetCOLORREF(CurrentColors[HOT_PANEL]);
    }
    else
    {
        if (Configuration.ShowPanelCaption)
            textColor = GetCOLORREF(CurrentColors[activeCaption ? ACTIVE_CAPTION_FG : INACTIVE_CAPTION_FG]);
        else
            textColor = GetSysColor(COLOR_BTNTEXT);
    }
    COLORREF bkColor;
    if (Configuration.ShowPanelCaption)
        bkColor = GetCOLORREF(CurrentColors[activeCaption ? ACTIVE_CAPTION_BK : INACTIVE_CAPTION_BK]);
    else
        bkColor = GetSysColor(COLOR_BTNFACE);
    int oldTextColor = SetTextColor(hDC, textColor);
    int oldBkColor = SetBkColor(hDC, bkColor);
    int x = (rect->left + rect->right) / 2 - width / 2;
    int y = (rect->top + rect->bottom) / 2 - height / 2;
    BitBlt(hDC, x, y, width, height, hMemDC, xOffset, 0, SRCCOPY);
    SetBkColor(hDC, oldBkColor);
    SetTextColor(hDC, oldTextColor);
    SelectObject(hMemDC, hOldBitmap);
}

void CStatusWindow::PaintThrobber(HDC hDC)
{
    if ((Border & blTop) == 0)
        return;
    BOOL activeCaption = (FilesWindow == MainWindow->GetActivePanel()) && MainWindow->CaptionIsActive;
    RECT r = ThrobberRect;
    r.left += 2;
    r.right -= 2;
    FillRect(hDC, &r, activeCaption ? HActiveCaptionBrush : HInactiveCaptionBrush);
    int x = (ThrobberRect.left + ThrobberRect.right) / 2 - THROBBER_WIDTH / 2;
    int y = (ThrobberRect.top + ThrobberRect.bottom) / 2 - THROBBER_HEIGHT / 2;

    COLORREF fgClr;
    if (Configuration.ShowPanelCaption)
        fgClr = GetCOLORREF(CurrentColors[activeCaption ? ACTIVE_CAPTION_FG : INACTIVE_CAPTION_FG]);
    else
        fgClr = GetSysColor(COLOR_BTNTEXT);

    ThrobberFrames->Draw(ThrobberFrame, hDC, x, y, fgClr, IL_DRAW_ASALPHA);
}

void CStatusWindow::PaintSecurity(HDC hDC)
{
    if (Security == sisNone || (Border & blTop) == 0)
        return;
    BOOL activeCaption = (FilesWindow == MainWindow->GetActivePanel()) && MainWindow->CaptionIsActive;
    RECT r = SecurityRect;
    //  r.left += 2;
    //  r.right -= 2;
    FillRect(hDC, &r, activeCaption ? HActiveCaptionBrush : HInactiveCaptionBrush);
    int x = (SecurityRect.left + SecurityRect.right) / 2 - LOCK_WIDTH / 2;
    int y = (SecurityRect.top + SecurityRect.bottom) / 2 - LOCK_HEIGHT / 2 - 1;

    COLORREF fgClr;
    if (HotSecurity)
    {
        if (Configuration.ShowPanelCaption)
            fgClr = GetCOLORREF(CurrentColors[activeCaption ? HOT_ACTIVE : HOT_INACTIVE]);
        else
            fgClr = GetCOLORREF(CurrentColors[HOT_PANEL]);
    }
    else
    {
        if (Configuration.ShowPanelCaption)
            fgClr = GetCOLORREF(CurrentColors[activeCaption ? ACTIVE_CAPTION_FG : INACTIVE_CAPTION_FG]);
        else
            fgClr = GetSysColor(COLOR_BTNTEXT);
    }

    LockFrames->Draw(DWORD(Security - 1), hDC, x, y, fgClr, IL_DRAW_ASALPHA /*IL_DRAW_TRANSPARENT*/);
}

#define FILTER_WIDTH 9
#define FILTER_HEIGHT 8

#define ZOOM_WIDTH 9
#define ZOOM_HEIGHT 8

void CStatusWindow::Paint(HDC hdc, BOOL highlightText, BOOL highlightHotTrackOnly)
{
    CALL_STACK_MESSAGE3("CStatusWindow::Paint(, %d, %d)", highlightText, highlightHotTrackOnly);
    HDC dc = ItemBitmap.HMemDC;

    BOOL isDirectoryLine = (Border & blTop) != 0;

    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = Width;
    r.bottom = Height;
    FillRect(dc, &r, HDialogBrush);

    GetClientRect(HWindow, &r);
    if (Border & blBottom)
        r.bottom--;

    // misto pro toolbaru prosim
    if (isDirectoryLine)
        r.left += ToolBarWidth + 1;

    BOOL activeCaption = (FilesWindow == MainWindow->GetActivePanel()) && MainWindow->CaptionIsActive;
    if (isDirectoryLine && Configuration.ShowPanelCaption)
    {
        // ramecek kolem textu
        RECT textR = r;
        textR.top += 2;
        textR.bottom -= 2;
        DrawEdge(dc, &textR, BDR_SUNKENOUTER, BF_RECT);

        // vyplnime plochu pod textem (aktivni/neaktivni)
        textR.left++;
        textR.top++;
        textR.right--;
        textR.bottom--;
        FillRect(dc, &textR, activeCaption ? HActiveCaptionBrush : HInactiveCaptionBrush);
    }

    // text
    EllipsedChars = -1;
    EllipsedWidth = -1;
    if (Text != NULL)
    {
        BOOL truncateEnd = TRUE; // zkracujeme konec (TRUE) nebo za root slozkou (FALSE)
        int visibleChars = 0;

        SetBkMode(dc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(dc, EnvFont);

        SIZE s;
        RECT tmpR;
        tmpR.left = r.left + 2;
        tmpR.right = r.right - 2;
        tmpR.top = r.top + 3;
        tmpR.bottom = r.bottom - 3;

        WholeTextVisible = FALSE;
        // vsechny plochy nastavim jako nulove
        SecurityRect = tmpR;
        SecurityRect.right = SecurityRect.left;

        TextRect = tmpR;
        TextRect.right = TextRect.left;

        HiddenRect = tmpR;
        HiddenRect.right = HiddenRect.left;

        ThrobberRect = tmpR;
        ThrobberRect.right = ThrobberRect.left;

        SizeRect = tmpR;
        SizeRect.left = SizeRect.right;

        HistoryRect = tmpR;
        HistoryRect.left = HistoryRect.right;

        ZoomRect = tmpR;
        ZoomRect.left = ZoomRect.right;

        // zjistim, ktere polozky (text/history/size/zoom) se vejdou do dostupne plochy
        if (isDirectoryLine)
        {
            if (Configuration.ShowPanelZoom)
            {
                if (tmpR.right - tmpR.left < ZOOM_WIDTH + 4)
                    goto SKIP_MEASURING; // nevejde zoom tlacitko - vypadneme z mereni

                ZoomRect.left = tmpR.right - ZOOM_WIDTH - 6;
                ZoomRect.right = tmpR.right;
                tmpR.right -= 6 + ZOOM_WIDTH;
            }

            if (Size != NULL)
            {
                GetTextExtentPoint32(dc, Size, (int)strlen(Size), &s);
                if (tmpR.right - tmpR.left < s.cx)
                    goto SKIP_MEASURING; // nevejde se ani size - vypadneme z mereni

                SizeRect.left = tmpR.right - s.cx;
                SizeRect.right = tmpR.right;
                tmpR.right -= 2 + s.cx;
            }

            if (History)
            {
                if (tmpR.right - tmpR.left < SVGArrowDropDown.GetWidth())
                    goto SKIP_MEASURING; // nevejde se drop sipka - vypadneme z mereni

                HistoryRect.left = tmpR.right - SVGArrowDropDown.GetWidth() - 2;
                HistoryRect.right = tmpR.right + 2;
                tmpR.right -= 2 + SVGArrowDropDown.GetWidth();
            }

            if (Hidden)
            {
                if (tmpR.right - tmpR.left < FILTER_WIDTH)
                    goto SKIP_MEASURING; // nevejde se symbol filtru - vypadneme z mereni
                HiddenRect.left = tmpR.right - FILTER_WIDTH - 2;
                HiddenRect.right = tmpR.right + 2;
                tmpR.right -= 2 + FILTER_WIDTH;
            }

            if (Throbber)
            {
                if (tmpR.right - tmpR.left < THROBBER_WIDTH)
                    goto SKIP_MEASURING; // nevejde se throbber - vypadneme z mereni
                ThrobberRect.left = tmpR.right - THROBBER_WIDTH - 2;
                ThrobberRect.right = tmpR.right + 2;
                tmpR.right -= 2 + THROBBER_WIDTH;
            }

            if (Security != sisNone)
            {
                if (tmpR.right - tmpR.left < LOCK_WIDTH + 5)
                    goto SKIP_MEASURING; // nevejde se zamek - vypadneme z mereni
                SecurityRect.left = tmpR.left;
                SecurityRect.right = tmpR.left + 2 + LOCK_WIDTH + 3;
                tmpR.left += 2 + LOCK_WIDTH + 3;
            }
        }

        if (tmpR.right > tmpR.left + TextEllipsisWidthEnv)
        {
            visibleChars = TextLen;
            int textWidth = tmpR.right - tmpR.left;
            if (textWidth < AlpDX[TextLen - 1])
            {
                // text se do pozadovane sirky nevejde cely -> musime zkracovat
                if (isDirectoryLine && HotTrackItems.Count > 1 &&
                    HotTrackItems[0].Pixels + TextEllipsisWidthEnv <= textWidth)
                {
                    // pro horni directory line budeme zkracovat za root slozkou cesty
                    EllipsedChars = 0;
                    EllipsedWidth = 0;

                    int len = AlpDX[TextLen - 1];
                    int iter = HotTrackItems[0].Chars;
                    while (len > textWidth - TextEllipsisWidthEnv && iter < TextLen)
                    {
                        int charWidth = AlpDX[iter] - AlpDX[iter - 1];
                        len -= charWidth;
                        iter++;

                        EllipsedChars++;
                        EllipsedWidth += charWidth;
                    }
                    visibleChars = TextLen - iter;
                    truncateEnd = FALSE; // zkracujeme zevnitr
                }
                else
                {
                    // pro spodni infoline budeme hledat zezadu znak,
                    // za ktery lze nakopirovat "..."
                    while (visibleChars > 0 &&
                           AlpDX[visibleChars - 1] + TextEllipsisWidthEnv > textWidth)
                        visibleChars--;
                }
            }
            else
                WholeTextVisible = TRUE;

            int realWidth = 0;
            if (TextLen > 1)
            {
                realWidth = AlpDX[TextLen - 1];
                if (EllipsedWidth != -1)
                    realWidth = realWidth - EllipsedWidth + TextEllipsisWidthEnv;
            }
            TextRect.left = tmpR.left;
            TextRect.right = TextRect.left + realWidth;
            if (TextRect.right > tmpR.right)
                TextRect.right = tmpR.right;
            else
            {
                int leftMax = TextRect.right;
                if (Hidden)
                {
                    HiddenRect.left = TextRect.right + 2;
                    HiddenRect.right = HiddenRect.left + FILTER_WIDTH + 2;
                    leftMax = HiddenRect.right;
                }
                if (Throbber)
                {
                    ThrobberRect.left = Hidden ? HiddenRect.right : TextRect.right + 2;
                    ThrobberRect.right = ThrobberRect.left + THROBBER_WIDTH + 2;
                    leftMax = ThrobberRect.right;
                }
                if (History)
                {
                    int mid = (leftMax + SizeRect.left) / 2;
                    HistoryRect.left = mid - SVGArrowDropDown.GetWidth() - 2;
                    HistoryRect.right = mid + SVGArrowDropDown.GetWidth() + 2;
                }
            }
        }

    SKIP_MEASURING:

        int myYOffset = 0;
        if (isDirectoryLine && !Configuration.ShowPanelCaption)
            myYOffset = 1;
        int textY = (tmpR.top + tmpR.bottom - EnvFontCharHeight + myYOffset) / 2;

        // vypis hlavniho textu, pokud na nej mame nejaky prostor
        if (TextRect.right > TextRect.left)
        {
            // urcime si predem, ktera cast textu ma byt vykreslena vyrazne, tu kvuli cleartype
            // musime vyriznout z kresleni normalniho textu
            CHotTrackItem* hotItem = NULL;
            BOOL showFlashText = (highlightText && highlightHotTrackOnly && LastHotItem != NULL);
            if (HotItem != NULL)
                hotItem = HotItem;
            if (showFlashText)
                hotItem = LastHotItem;

            if (isDirectoryLine && Configuration.ShowPanelCaption)
            {
                if (activeCaption)
                    SetTextColor(dc, GetCOLORREF(CurrentColors[ACTIVE_CAPTION_FG]));
                else
                    SetTextColor(dc, GetCOLORREF(CurrentColors[INACTIVE_CAPTION_FG]));
                if (highlightText && !highlightHotTrackOnly)
                    SetTextColor(dc, GetCOLORREF(CurrentColors[activeCaption ? HOT_ACTIVE : HOT_INACTIVE]));
            }
            else
            {
                SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
                if (highlightText && !highlightHotTrackOnly)
                    SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            }

            int firstClipChar = 2 * TextLen;
            int lastClipChar = 2 * TextLen;
            if (hotItem != NULL)
            {
                firstClipChar = hotItem->Offset;
                lastClipChar = hotItem->Offset + hotItem->Chars;
            }

            // vykreslime prvni cast textu (az do hotItem, pokud text zacina hotItem, nebudeme kreslit nic)
            if (firstClipChar != 0)
            {
                if (truncateEnd)
                { // bez zkraceni nebo ustrizen konec
                    ExtTextOut(dc, TextRect.left, textY, 0, NULL, Text, min(visibleChars, firstClipChar), NULL);
                    if (visibleChars < min(TextLen, firstClipChar)) // pokud byl ustrizen konec -> pripojime "..."
                    {
                        int offset = (visibleChars > 0) ? AlpDX[visibleChars - 1] : 0;
                        ExtTextOut(dc, TextRect.left + offset, textY, 0, NULL, "...", 3, NULL);
                    }
                }
                else
                { // uriznuta cast za root slozkou
                    // root cast
                    int rootChars = HotTrackItems[0].Chars;
                    ExtTextOut(dc, TextRect.left, textY, 0, NULL, Text, rootChars, NULL);
                    // "..."
                    ExtTextOut(dc, TextRect.left + AlpDX[rootChars - 1], textY, 0, NULL, "...", 3, NULL);
                    // zbytek
                    ExtTextOut(dc, TextRect.left + AlpDX[rootChars - 1] + TextEllipsisWidthEnv,
                               textY, 0, NULL, Text + TextLen - visibleChars, visibleChars, NULL);
                }
            }

            // vykreslime druhou cast textu (za hotItem dal) -- zkracovani na konci
            if (hotItem != NULL && truncateEnd && lastClipChar <= visibleChars)
            {
                // bez zkraceni nebo ustrizen konec
                int visibleChars2 = visibleChars - lastClipChar;
                ExtTextOut(dc, TextRect.left + AlpDX[lastClipChar - 1], textY, 0, NULL, Text + lastClipChar, visibleChars2, NULL);
                if (visibleChars < TextLen) // pokud byl ustrizen konec -> pripojime "..."
                {
                    int offset = (visibleChars > 0) ? AlpDX[visibleChars - 1] : 0;
                    ExtTextOut(dc, TextRect.left + offset, textY, 0, NULL, "...", 3, NULL);
                }
            }
            // vykreslime druhou cast textu (za hotItem dal) -- zkracovani uprosted
            // takto se zkracuji pouze cesty v directory line (plati podminka !truncateEnd)
            if (hotItem != NULL && !truncateEnd && lastClipChar <= TextLen)
            { // uriznuta cast za root slozkou
                int rootChars = HotTrackItems[0].Chars;
                int firstChar = hotItem->Chars;

                if (lastClipChar <= rootChars)
                {
                    ExtTextOut(dc, TextRect.left + AlpDX[rootChars - 1], textY, 0, NULL, "...", 3, NULL); // "..."
                    firstChar += EllipsedChars;                                                           // posuneme se pres vypustene znaky
                }
                else
                {
                    if (firstChar < rootChars + EllipsedChars) // je potreba preskocit pripadne zpetne lomitko, ktere by lezlo do vypustky
                        firstChar = rootChars + EllipsedChars;
                }
                ExtTextOut(dc, TextRect.left + AlpDX[firstChar - 1] - EllipsedWidth + TextEllipsisWidthEnv,
                           textY, 0, NULL, Text + firstChar, TextLen - firstChar, NULL);
            }

            // zobrazime hot track polozku
            if (hotItem != NULL)
            {
                COLORREF oldColor;
                if (isDirectoryLine && Configuration.ShowPanelCaption)
                {
                    oldColor = SetTextColor(dc, GetCOLORREF(CurrentColors[activeCaption ? HOT_ACTIVE : HOT_INACTIVE]));
                }
                else
                {
                    oldColor = SetTextColor(dc, GetCOLORREF(CurrentColors[HOT_PANEL]));
                    if (showFlashText)
                        SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                }
                HFONT hOldFont = NULL;
                if (Configuration.SingleClick && HotItem != NULL)
                    hOldFont = (HFONT)SelectObject(dc, EnvFontUL);

                if (truncateEnd)
                { // bez zkraceni nebo ustrizen konec
                    int showChars = hotItem->Chars;
                    if (hotItem->Offset + showChars > visibleChars)
                    {
                        showChars = visibleChars - hotItem->Offset;
                        int offset = (visibleChars > 0) ? AlpDX[visibleChars - 1] : 0;
                        ExtTextOut(dc, TextRect.left + offset, textY, 0, NULL, "...", 3, NULL);
                    }
                    if (showChars > 0)
                    {
                        ExtTextOut(dc, TextRect.left + hotItem->PixelsOffset, textY, 0, NULL,
                                   Text + hotItem->Offset, showChars, NULL);
                    }
                }
                else
                { // uriznuta cast za root slozkou
                    int showChars = hotItem->Chars;

                    int rootChars = HotTrackItems[0].Chars;
                    ExtTextOut(dc, TextRect.left, textY, 0, NULL, Text, rootChars, NULL);
                    if (showChars > rootChars)
                    {
                        // "..."
                        ExtTextOut(dc, TextRect.left + AlpDX[rootChars - 1], textY, 0, NULL, "...", 3, NULL);
                        if (showChars - rootChars - EllipsedChars > 0)
                        {
                            // zbytek
                            ExtTextOut(dc, TextRect.left + AlpDX[rootChars - 1] + TextEllipsisWidthEnv,
                                       textY, 0, NULL, Text + rootChars + EllipsedChars, showChars - rootChars - EllipsedChars, NULL);
                        }
                    }
                }

                if (hOldFont != NULL)
                    SelectObject(dc, hOldFont);
                SetTextColor(dc, oldColor);
            }
        }

        HDC hMemDC = HANDLES(CreateCompatibleDC(NULL));

        // zoom symbol
        if (ZoomRect.left < ZoomRect.right)
        {
            BOOL zoomed;
            if (MainWindow->LeftPanel == FilesWindow)
                zoomed = MainWindow->IsPanelZoomed(TRUE);
            else
                zoomed = MainWindow->IsPanelZoomed(FALSE);
            PaintSymbol(dc, hMemDC, HZoomBitmap, zoomed ? ZOOM_WIDTH : 0, ZOOM_WIDTH, ZOOM_HEIGHT, &ZoomRect, HotZoom, activeCaption);
        }

        // disk free
        if (SizeRect.left < SizeRect.right)
        {
            if (HotSize)
            {
                if (isDirectoryLine && Configuration.ShowPanelCaption)
                    SetTextColor(dc, GetCOLORREF(CurrentColors[activeCaption ? HOT_ACTIVE : HOT_INACTIVE]));
                else
                    SetTextColor(dc, GetCOLORREF(CurrentColors[HOT_PANEL]));
            }
            else
            {
                if (isDirectoryLine && Configuration.ShowPanelCaption)
                    SetTextColor(dc, GetCOLORREF(CurrentColors[activeCaption ? ACTIVE_CAPTION_FG : INACTIVE_CAPTION_FG]));
                else
                    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
            }

            HFONT hOldFont = NULL;
            if (Configuration.SingleClick && HotSize)
                hOldFont = (HFONT)SelectObject(dc, EnvFontUL);
            ExtTextOut(dc, SizeRect.left, textY, 0, NULL, Size, (UINT)strlen(Size), NULL);
            if (hOldFont != NULL)
                SelectObject(dc, hOldFont);
        }
        SelectObject(dc, oldFont);

        // sipka pro historii adresaru
        if (HistoryRect.left < HistoryRect.right)
        {
            // JRYFIXME: radna podpora pro HOT barvu, viz PaintSymbol, zatim jen hack pres SVGSTATE_DISABLED
            //PaintSymbol(dc, hMemDC, HDropDownBitmap, 0, SVGArrowDropDown.GetWidth(), SVGArrowDropDown.GetHeight(), &HistoryRect, HotHistory, activeCaption);
            SVGArrowDropDown.AlphaBlend(dc,
                                        HistoryRect.left,
                                        HistoryRect.top + (HistoryRect.bottom - HistoryRect.top - SVGArrowDropDown.GetHeight()) / 2,
                                        -1, -1,
                                        HotHistory ? SVGSTATE_DISABLED : SVGSTATE_ENABLED);
        }

        // symbol filtru
        if (HiddenRect.left < HiddenRect.right)
            PaintSymbol(dc, hMemDC, HFilter, 0, FILTER_WIDTH, FILTER_HEIGHT, &HiddenRect, HotHidden, activeCaption);

        // throbber
        if (ThrobberRect.left < ThrobberRect.right)
            PaintThrobber(dc);

        // security
        if (SecurityRect.left < SecurityRect.right)
            PaintSecurity(dc);

        HANDLES(DeleteDC(hMemDC));
    }

    int delta = 0;
    if (Border & blBottom)
        delta = 1;

    BitBlt(hdc, delta + ToolBarWidth, 0, Width - ToolBarWidth - 2 * delta, Height - delta,
           dc, ToolBarWidth, 0, SRCCOPY);
}

void CStatusWindow::Repaint(BOOL flashText, BOOL hotTrackOnly)
{
    CALL_STACK_MESSAGE_NONE
    if (HWindow == NULL)
        return;
    HDC hdc = HANDLES(GetDC(HWindow));
    Paint(hdc, flashText, hotTrackOnly);
    HANDLES(ReleaseDC(HWindow, hdc));
}
/*
void
CStatusWindow::RepaintThrobber()
{
  CALL_STACK_MESSAGE_NONE
  if (HWindow == NULL)
    return;
  HDC hdc = HANDLES(GetDC(HWindow));
  PaintThrobber(hdc);
  HANDLES(ReleaseDC(HWindow, hdc));
}
*/

void CStatusWindow::InvalidateAndUpdate(BOOL update)
{
    CALL_STACK_MESSAGE_NONE
    if (HWindow == NULL)
        return;
    InvalidateRect(HWindow, NULL, FALSE);
    if (update)
        UpdateWindow(HWindow);
}

class CTextDropTarget : public IDropTarget
{
private:
    long RefCount;                    // zivotnost objektu
    IDataObject* DataObject;          // IDataObject, ktery vstoupil do dragu
    IDataObject* ForbiddenDataObject; // IDataObject, ktery nebereme (jsme jeho zdrojem)
    BOOL UseUnicode;                  // je v DataObject unicode text? (jinak zkusime ANSI text)
    CFilesWindow* FilesWindow;        // panel, ke kteremu jsme asociovani
    char Buffer[2 * MAX_PATH];

public:
    CTextDropTarget(CFilesWindow* filesWindow)
    {
        RefCount = 1;
        DataObject = NULL;
        ForbiddenDataObject = NULL;
        UseUnicode = TRUE;
        FilesWindow = filesWindow;
    }

    virtual ~CTextDropTarget()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
    }

    void SetForbiddenDataObject(IDataObject* forbiddenDataObject)
    {
        ForbiddenDataObject = forbiddenDataObject;
    }

    // vrati adresar (musi byt prave jeden)
    BOOL GetDirFromDataObject(IDataObject* pDataObject, char* path)
    {
        FORMATETC formatEtc;
        formatEtc.cfFormat = RegisterClipboardFormat(SALCF_FAKE_REALPATH);
        formatEtc.ptd = NULL;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        STGMEDIUM stgMedium;
        stgMedium.tymed = TYMED_HGLOBAL;
        stgMedium.hGlobal = NULL;
        stgMedium.pUnkForRelease = NULL;

        if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
        {
            path[0] = 0;
            if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
            {
                char* data = (char*)HANDLES(GlobalLock(stgMedium.hGlobal));
                if (data != NULL)
                {
                    if (data[0] == 'D')
                        lstrcpyn(path, data + 1, MAX_PATH);
                    HANDLES(GlobalUnlock(stgMedium.hGlobal));
                }
            }
            ReleaseStgMedium(&stgMedium);
            return path[0] != 0;
        }

        formatEtc.cfFormat = CF_HDROP;
        formatEtc.ptd = NULL;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        stgMedium.tymed = TYMED_HGLOBAL;
        stgMedium.hGlobal = NULL;
        stgMedium.pUnkForRelease = NULL;

        BOOL ret = FALSE;
        if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
        {
            if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
            {
                DROPFILES* data = (DROPFILES*)HANDLES(GlobalLock(stgMedium.hGlobal));
                if (data != NULL)
                {
                    if (data->fWide)
                    {
                        const wchar_t* fileW = (wchar_t*)(((char*)data) + data->pFiles);
                        int l = lstrlenW(fileW);
                        if (*(fileW + l + 1) == 0)
                        {
                            WideCharToMultiByte(CP_ACP, 0, fileW, l + 1, path, l + 1, NULL, NULL);
                            path[l] = 0;
                            ret = TRUE;
                        }
                    }
                    else
                    {
                        const char* fileA = ((char*)data) + data->pFiles;
                        int l = (int)strlen(fileA);
                        if (*(fileA + l + 1) == 0)
                        {
                            strcpy(path, fileA);
                            ret = TRUE;
                        }
                    }

                    HANDLES(GlobalUnlock(stgMedium.hGlobal));
                }
            }
            ReleaseStgMedium(&stgMedium);
        }
        if (ret)
        {
            DWORD attrs = SalGetFileAttributes(path);
            if (attrs == 0xFFFFFFFF)
                ret = FALSE;
            else if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) // nejedna se o adresar
                ret = FALSE;
        }
        return ret;
    }

    STDMETHOD(QueryInterface)
    (REFIID refiid, void FAR* FAR* ppv)
    {
        if (refiid == IID_IUnknown || refiid == IID_IDropTarget)
        {
            *ppv = this;
            AddRef();
            return NOERROR;
        }
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
    }

    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // nesmime sahnout do objektu, uz neexistuje
        }
        return RefCount;
    }

    STDMETHOD(DragEnter)
    (IDataObject* pDataObject, DWORD grfKeyState,
     POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragEnter(pt.x, pt.y);
        if (DataObject != NULL)
            DataObject->Release();
        DataObject = pDataObject;
        DataObject->AddRef();

        // pokud je nas panel zaroven zdrojem, zakazu paste
        if (DataObject == ForbiddenDataObject)
        {
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        // zjistime jestli je na clipboardu text
        FORMATETC formatEtc;
        ZeroMemory(&formatEtc, sizeof(formatEtc));
        formatEtc.cfFormat = CF_UNICODETEXT;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;
        UseUnicode = TRUE;
        HRESULT textRes;
        if ((textRes = pDataObject->QueryGetData(&formatEtc)) != S_OK)
        {
            formatEtc.cfFormat = CF_TEXT;
            UseUnicode = FALSE;
            textRes = pDataObject->QueryGetData(&formatEtc);
        }
        if (textRes == S_OK)
        {
            *pdwEffect = DROPEFFECT_COPY;
            return S_OK;
        }
        char dummy[MAX_PATH];
        if (GetDirFromDataObject(DataObject, dummy))
        {
            *pdwEffect = DROPEFFECT_COPY;
            return S_OK;
        }

        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHOD(DragOver)
    (DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragMove(pt.x, pt.y);
        if (DataObject != NULL)
        {
            // pokud je nas panel zaroven zdrojem, zakazu paste
            if (DataObject == ForbiddenDataObject)
            {
                *pdwEffect = DROPEFFECT_NONE;
                return S_OK;
            }
            // zjistime jestli je na clipboardu text
            FORMATETC formatEtc;
            ZeroMemory(&formatEtc, sizeof(formatEtc));
            formatEtc.cfFormat = UseUnicode ? CF_UNICODETEXT : CF_TEXT;
            formatEtc.dwAspect = DVASPECT_CONTENT;
            formatEtc.lindex = -1;
            formatEtc.tymed = TYMED_HGLOBAL;
            if (DataObject->QueryGetData(&formatEtc) == S_OK)
            {
                *pdwEffect = DROPEFFECT_COPY;
                return S_OK;
            }
            char dummy[MAX_PATH];
            if (GetDirFromDataObject(DataObject, dummy))
            {
                *pdwEffect = DROPEFFECT_COPY;
                return S_OK;
            }
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHOD(DragLeave)
    ()
    {
        if (ImageDragging)
            ImageDragLeave();
        if (DataObject != NULL)
        {
            DataObject->Release();
            DataObject = NULL;
        }
        return S_OK;
    }

    STDMETHOD(Drop)
    (IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
     DWORD* pdwEffect)
    {
        if (ImageDragging)
            ImageDragLeave();
        // pokusim se vytahnout z DataObjectu text
        FORMATETC formatEtc;
        ZeroMemory(&formatEtc, sizeof(formatEtc));
        formatEtc.cfFormat = UseUnicode ? CF_UNICODETEXT : CF_TEXT;
        formatEtc.dwAspect = DVASPECT_CONTENT;
        formatEtc.lindex = -1;
        formatEtc.tymed = TYMED_HGLOBAL;

        STGMEDIUM stgMedium;
        ZeroMemory(&stgMedium, sizeof(stgMedium));
        stgMedium.tymed = TYMED_HGLOBAL;

        if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
        {
            char* path = (char*)HANDLES(GlobalLock(stgMedium.hGlobal));
            if (path != NULL)
            {
                if (UseUnicode)
                    path = ConvertAllocU2A((const WCHAR*)path, -1);
                if (path != NULL)
                {
                    // zmenim cestu
                    lstrcpyn(Buffer, path, _countof(Buffer));

                    if (!IsPluginFSPath(Buffer))
                    {
                        int l = (int)strlen(Buffer);
                        if ((l != 2 || Buffer[0] != '\\' || Buffer[1] != '\\') && // nejde o cestu "\\\\" (Nethood root)
                            l > 0 && Buffer[l - 1] == '\\')
                            Buffer[--l] = 0;             // '\\' na konci neni vitan
                        if (l == 2 && Buffer[0] != '\\') // za neUNC root cestou musi byt '\\'
                        {
                            Buffer[l++] = '\\';
                            Buffer[l] = 0;
                        }
                        if (l == 6 && Buffer[0] == '\\' && Buffer[1] == '\\' && Buffer[2] == '.' && Buffer[3] == '\\' &&
                            Buffer[4] != 0 && Buffer[5] == ':') // za "\\.\C:\" root cestou musi byt '\\'
                        {
                            Buffer[l++] = '\\';
                            Buffer[l] = 0;
                        }
                    }

                    PostMessage(FilesWindow->HWindow, WM_USER_CHANGEDIR, TRUE, (LPARAM)Buffer);
                    if (UseUnicode)
                        free(path);
                }
                HANDLES(GlobalUnlock(stgMedium.hGlobal));
            }
        }
        else
        {
            char path[MAX_PATH];
            if (GetDirFromDataObject(pDataObject, path))
            {
                // zmenim cestu
                strcpy(Buffer, path);

                if (!IsPluginFSPath(Buffer))
                {
                    int l = (int)strlen(Buffer);
                    if (l > 0 && Buffer[l - 1] == '\\')
                        Buffer[--l] = 0;             // '\\' na konci neni vitan
                    if (l == 2 && Buffer[0] != '\\') // za neUNC root cestou musi byt '\\'
                    {
                        Buffer[l++] = '\\';
                        Buffer[l] = 0;
                    }
                }

                PostMessage(FilesWindow->HWindow, WM_USER_CHANGEDIR, FALSE, (LPARAM)Buffer);
            }
        }

        if (DataObject != NULL)
        {
            DataObject->Release();
            DataObject = NULL;
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }
};

void CStatusWindow::RegisterDragDrop()
{
    CALL_STACK_MESSAGE1("CStatusWindow::RegisterDragDrop()");
    CTextDropTarget* dropTarget = new CTextDropTarget(FilesWindow);
    if (dropTarget != NULL)
    {
        if (HANDLES(RegisterDragDrop(HWindow, dropTarget)) != S_OK)
        {
            TRACE_E("RegisterDragDrop error.");
        }
        else
            IDropTargetPtr = dropTarget;
        dropTarget->Release(); // RegisterDragDrop volala AddRef()
    }
}

void CStatusWindow::RevokeDragDrop()
{
    CALL_STACK_MESSAGE_NONE
    HANDLES(RevokeDragDrop(HWindow));
}

#define BUTTON_OFFSET 0

LRESULT
CStatusWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CStatusWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HotItem = NULL;
        LastHotItem = NULL;
        HotSize = FALSE;
        HotHistory = FALSE;
        HotZoom = FALSE;
        HotHidden = FALSE;
        HotSecurity = FALSE;
        MouseCaptured = FALSE;

        if (Border & blTop)
        {
            ToolBar = new CMainToolBar(MainWindow->HWindow, Left ? mtbtLeft : mtbtRight);
            if (ToolBar == NULL)
            {
                TRACE_E(LOW_MEMORY);
                return -1;
            }
            ToggleToolBar();
            RegisterDragDrop();
        }

        if (ShowThrobber)
        {
            int ti = DelayedThrobberShowTime == 0 /* neplatna hodnota */ ? 0 : DelayedThrobberShowTime - GetTickCount();
            if (ti < 0)
                ti = 0;
            DelayedThrobberShowTime = 0; // tuto hodnotu uz nepotrebujeme, pripadne si ji musi nastavit SetThrobber znovu, priradime neplatnou hodnotu
            SetThrobber(TRUE, ti);       // mame take zapnout throbber
        }

        return 0;
    }

    case WM_DESTROY:
    {
        if (Throbber || DelayedThrobber)
            SetThrobber(FALSE, 0, TRUE); // potrebujeme sestrelit timer
        if (Border & blTop)
            RevokeDragDrop();
        if (ToolBar != NULL)
        {
            ToolBar->DetachWindow();
            delete ToolBar;
            ToolBar = NULL;
        }
        return 0;
    }

    case WM_SIZE:
    {
        RECT r;
        GetClientRect(HWindow, &r);

        if (ToolBar != NULL && ToolBar->HWindow != NULL)
        {
            ToolBarWidth = ToolBar->GetNeededWidth();
            SetWindowPos(ToolBar->HWindow, 0, 0, 0, ToolBarWidth, r.bottom, SWP_NOACTIVATE | SWP_NOZORDER);
        }
        if (Width != r.right || Height != r.bottom)
        {
            Width = r.right;
            Height = r.bottom;
            ItemBitmap.Enlarge(Width, Height); // alokace bitmapy v ItemBitmap.HMemDC
        }

        break;
    }

    case WM_USER_TTGETTEXT:
    {
        DWORD id = (DWORD)wParam; // FIXME_X64 - overit pretypovani na (DWORD)
        char* text = (char*)lParam;
        switch (id)
        {
        case 0:
        {
            break;
        }

        case 2:
        {
            lstrcpy(text, LoadStr(IDS_PANELFILTER));
            break;
        }

        case 3:
        {
            text[0] = 0;
            ExpandPluralFilesDirs(text, 200, HiddenFilesCount, HiddenDirsCount, epfdmHidden, FALSE);
            //          lstrcat(text, " ");
            //          CQuadWord qwHidden(HiddenFilesCount + HiddenDirsCount, 0);
            //          ExpandPluralString(text + strlen(text), 100, LoadStr(IDS_PLURAL_SWITCH_HIDDEN),
            //                             1, &qwHidden);
            break;
        }

        case 4:
        {
            char* str;
            if (Border == blTop && WholeTextVisible)
                str = LoadStr(IDS_TRIM_DRAG_PATH);
            else if (Border == blBottom && WholeTextVisible)
                str = LoadStr(IDS_COPY_DRAG_TEXT);
            else
                str = Text;
            if (str == NULL)
                text[0] = 0;
            else
                lstrcpy(text, str);
            break;
        }

        case 5:
        {
            lstrcpy(text, LoadStr(IDS_DIRHISTORY));
            break;
        }

        case 6:
        {
            lstrcpy(text, LoadStr(IDS_FREESPACE));
            break;
        }

        case 7:
        {
            lstrcpy(text, LoadStr(IDS_ZOOMPANEL));
            break;
        }

        case 8:
        {
            lstrcpyn(text, ThrobberTooltip != NULL ? ThrobberTooltip : "", TOOLTIP_TEXT_MAX);
            break;
        }

        case 9:
        {
            lstrcpyn(text, SecurityTooltip != NULL ? SecurityTooltip : "", TOOLTIP_TEXT_MAX);
            break;
        }

        default:
            TRACE_E("Unknown ID:" << id);
            break;
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        short xPos = LOWORD(lParam);
        short yPos = HIWORD(lParam);

        CHotTrackItem* newHotItem = NULL;
        BOOL newHotSize = FALSE;
        BOOL newHotHistory = FALSE;
        BOOL newHotZoom = FALSE;
        BOOL newHotHidden = FALSE;
        BOOL newHotSecurity = FALSE;

        DWORD toolTipID = 0;

        if (xPos >= TextRect.left && xPos < TextRect.right && yPos >= TextRect.top && yPos < TextRect.bottom)
            toolTipID = 4;

        BOOL isInHistory = History && xPos >= HistoryRect.left && xPos < HistoryRect.right &&
                           yPos >= HistoryRect.top && yPos < HistoryRect.bottom;
        if (isInHistory)
            toolTipID = 5;

        BOOL isInRect = xPos >= TextRect.left && xPos < TextRect.right &&
                        yPos >= TextRect.top && yPos < TextRect.bottom;
        BOOL isInSizeRect = xPos >= SizeRect.left && xPos < SizeRect.right &&
                            yPos >= SizeRect.top && yPos < SizeRect.bottom;
        BOOL isInZoomRect = xPos >= ZoomRect.left && xPos < ZoomRect.right &&
                            yPos >= ZoomRect.top && yPos < ZoomRect.bottom;
        BOOL isInHiddenRect = xPos >= HiddenRect.left && xPos < HiddenRect.right &&
                              yPos >= HiddenRect.top && yPos < HiddenRect.bottom;
        BOOL isInThrobberRect = xPos >= ThrobberRect.left && xPos < ThrobberRect.right &&
                                yPos >= ThrobberRect.top && yPos < ThrobberRect.bottom;
        BOOL isInSecurityRect = xPos >= SecurityRect.left && xPos < SecurityRect.right &&
                                yPos >= SecurityRect.top && yPos < SecurityRect.bottom;
        if (isInSizeRect)
            toolTipID = 6;
        if (isInZoomRect)
            toolTipID = 7;
        if (isInHiddenRect)
            toolTipID = 3;
        if (isInThrobberRect)
            toolTipID = 8;
        if (isInSecurityRect)
            toolTipID = 9;

        if (wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON))
            toolTipID = 0;
        SetCurrentToolTip(HWindow, toolTipID);

        // osetrim utrhnuti textu a zahajeni drag&dropu
        if (MouseCaptured && (LButtonDown || RButtonDown) && HotTrackItems.Count > 0)
        {
            int x = abs(LButtonDownPoint.x - (short)LOWORD(lParam));
            int y = abs(LButtonDownPoint.y - (short)HIWORD(lParam));
            if (x > GetSystemMetrics(SM_CXDRAG) || y > GetSystemMetrics(SM_CYDRAG))
            {
                int index;
                if (FindHotTrackItem(LButtonDownPoint.x - TextRect.left, index))
                {
                    char buffer[MAX_PATH];
                    int hotChars = HotTrackItems[index].Chars;
                    if (hotChars + 1 > MAX_PATH)
                        hotChars = MAX_PATH - 1;
                    lstrcpyn(buffer, Text + HotTrackItems[index].Offset, hotChars + 1);
                    // u Directory Line s pluginovym FS je jeste potreba umoznit pluginu posledni upravy cesty (pridani ']' u VMS cest u FTP)
                    if ((Border & blTop) && FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->NotEmpty())
                    {
                        FilesWindow->GetPluginFS()->CompleteDirectoryLineHotPath(buffer, MAX_PATH);
                        FilesWindow->GetPluginFS()->GetPluginInterfaceForFS()->ConvertPathToExternal(FilesWindow->GetPluginFS()->GetPluginFSName(),
                                                                                                     FilesWindow->GetPluginFS()->GetPluginFSNameIndex(),
                                                                                                     strchr(buffer, ':') + 1);
                        hotChars = (int)strlen(buffer);
                    }

                    WindowProc(WM_MOUSELEAVE, 0, 0);
                    MouseCaptured = FALSE;
                    LButtonDown = FALSE;
                    RButtonDown = FALSE;

                    HGLOBAL h = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, hotChars + 1));
                    if (h != NULL)
                    {
                        char* s = (char*)HANDLES(GlobalLock(h));
                        if (s != NULL)
                        {
                            memcpy(s, buffer, hotChars + 1);
                            HANDLES(GlobalUnlock(h));
                        }

                        CImpIDropSource* dropSource = new CImpIDropSource(FALSE);
                        IDataObject* dataObject = new CTextDataObject(h);
                        if (IDropTargetPtr != NULL)
                            ((CTextDropTarget*)IDropTargetPtr)->SetForbiddenDataObject(dataObject);
                        if (dataObject != NULL && dropSource != NULL)
                        {
                            DWORD dwEffect;

                            HIMAGELIST hDragIL = NULL;
                            int dxHotspot, dyHotspot;
                            int imgWidth, imgHeight;
                            hDragIL = CreateDragImage(buffer, dxHotspot, dyHotspot, imgWidth, imgHeight);
                            ImageList_BeginDrag(hDragIL, 0, dxHotspot, dyHotspot);
                            ImageDragBegin(imgWidth, imgHeight, dxHotspot, dyHotspot);

                            DoDragDrop(dataObject, dropSource, DROPEFFECT_COPY, &dwEffect);

                            ImageDragEnd();
                            ImageList_EndDrag();
                            ImageList_Destroy(hDragIL);

                            isInRect = FALSE;
                            isInSizeRect = FALSE;
                            isInHistory = FALSE;
                            isInZoomRect = FALSE;
                            isInHiddenRect = FALSE;
                            isInThrobberRect = FALSE;
                            isInSecurityRect = FALSE;
                        }
                        if (IDropTargetPtr != NULL)
                            ((CTextDropTarget*)IDropTargetPtr)->SetForbiddenDataObject(NULL);
                        if (dataObject != NULL)
                            dataObject->Release();
                        if (dropSource != NULL)
                            dropSource->Release();
                    }
                }
            }
        }

        BOOL repaint = FALSE;

        // osetrim ukonceni capture modu
        if (MouseCaptured)
        {
            POINT p;
            GetCursorPos(&p);
            if (!isInRect && !isInSizeRect && !isInHistory && !isInZoomRect &&
                    !isInHiddenRect && !isInSecurityRect ||
                WindowFromPoint(p) != HWindow)
            {
                WindowProc(WM_MOUSELEAVE, 0, 0);
                MouseCaptured = FALSE;
                repaint = TRUE;
            }
        }
        else
        {
            // zahajeni capture modu
            if (!MouseCaptured && (isInRect || isInSizeRect || isInHistory ||
                                   isInZoomRect || isInHiddenRect || isInSecurityRect))
            {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = HWindow;
                TrackMouseEvent(&tme);
                MouseCaptured = TRUE;
                repaint = TRUE;
            }
        }

        // postaram se o hilighting
        if (MouseCaptured)
        {
            if (isInRect)
            {
                int index;
                if (FindHotTrackItem(xPos - TextRect.left, index))
                {
                    newHotItem = &HotTrackItems[index];
                    if (Configuration.SingleClick)
                        SetHandCursor();
                }
                else
                {
                    if (Configuration.SingleClick)
                        SetCursor(LoadCursor(NULL, IDC_ARROW));
                }
            }
            if (isInHistory)
            {
                newHotHistory = TRUE;
                if (Configuration.SingleClick)
                    SetHandCursor();
            }
            if (isInZoomRect)
            {
                newHotZoom = TRUE;
                if (Configuration.SingleClick)
                    SetHandCursor();
            }
            if (isInHiddenRect)
            {
                newHotHidden = TRUE;
                if (Configuration.SingleClick)
                    SetHandCursor();
            }
            if (isInSizeRect)
            {
                // drive-info funguje jen pokud nejde o FS, ktery drive-info nepodporuje
                if (FilesWindow->Is(ptDisk) || FilesWindow->Is(ptZIPArchive) ||
                    FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->NotEmpty() &&
                        FilesWindow->GetPluginFS()->IsServiceSupported(FS_SERVICE_SHOWINFO))
                {
                    newHotSize = TRUE;
                    if (Configuration.SingleClick)
                        SetHandCursor();
                }
            }
            if (isInSecurityRect)
            {
                if (FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->NotEmpty() &&
                    FilesWindow->GetPluginFS()->IsServiceSupported(FS_SERVICE_SHOWSECURITYINFO))
                {
                    newHotSecurity = TRUE;
                    if (Configuration.SingleClick)
                        SetHandCursor();
                }
            }
        }

        if (repaint || newHotItem != HotItem || newHotSize != HotSize ||
            newHotHistory != HotHistory || newHotZoom != HotZoom ||
            newHotHidden != HotHidden || newHotSecurity != HotSecurity)
        {
            HotItem = newHotItem;
            if (HotItem != NULL)
                LastHotItem = HotItem;
            HotSize = newHotSize;
            HotHistory = newHotHistory;
            HotZoom = newHotZoom;
            HotHidden = newHotHidden;
            HotSecurity = newHotSecurity;
            Repaint();
        }

        break;
    }

    case WM_SETCURSOR:
    {
        if (MouseCaptured)
            return TRUE;
        break;
    }

    case WM_MOUSELEAVE:
    case WM_CANCELMODE:
    {
        SetCurrentToolTip(NULL, 0);
        if (MouseCaptured)
        {
            if (GetCapture() == HWindow)
                ReleaseCapture();
            LButtonDown = FALSE;
            RButtonDown = FALSE;
            MouseCaptured = FALSE;
            if (Configuration.SingleClick)
                SetCursor(LoadCursor(NULL, IDC_ARROW));
        }
        if (HotItem != NULL || HotSize || HotHistory || HotZoom || HotHidden || HotSecurity)
        {
            HotItem = NULL;
            HotHistory = FALSE;
            HotSize = FALSE;
            HotZoom = FALSE;
            HotHidden = FALSE;
            HotSecurity = FALSE;
            Repaint();
        }
        break;
    }

    case WM_RBUTTONDOWN:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
        SetCurrentToolTip(NULL, 0);

        if (HotHistory && MainWindow->GetActivePanel() != FilesWindow)
            MainWindow->ChangePanel();

        if (!MouseCaptured && (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK))
        {
            if (MainWindow->GetActivePanel() != FilesWindow)
                MainWindow->ChangePanel();
            if (uMsg == WM_LBUTTONDBLCLK && (Border & blTop))
                SendMessage(MainWindow->HWindow, WM_COMMAND, MAKEWPARAM(CM_ACTIVE_CHANGEDIR, 0), 0);
        }
        if (MouseCaptured)
        {
            SetCapture(HWindow);
            LButtonDownPoint.x = LOWORD(lParam);
            LButtonDownPoint.y = HIWORD(lParam);
            LButtonDown = (uMsg == WM_LBUTTONDOWN);
            RButtonDown = (uMsg == WM_RBUTTONDOWN);

            if ((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && HotHistory)
            {
                FilesWindow->OpenDirHistory();
            }

            if ((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && HotHidden)
            {
                FilesWindow->OpenStopFilterMenu();
            }

            if (uMsg == WM_LBUTTONDBLCLK && HotItem != NULL && (Border & blTop))
            {
                SendMessage(MainWindow->HWindow, WM_COMMAND, MAKEWPARAM(CM_ACTIVE_CHANGEDIR, 0), 0);
            }
        }
        break;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    {
        SetCurrentToolTip(NULL, 0);
        if (MouseCaptured && uMsg == WM_LBUTTONUP && (HotItem != NULL || HotSize || HotZoom || HotSecurity))
        {
            if (GetCapture() == HWindow)
                ReleaseCapture();
            int x = abs(LButtonDownPoint.x - (short)LOWORD(lParam));
            int y = abs(LButtonDownPoint.y - (short)HIWORD(lParam));
            if (x <= GetSystemMetrics(SM_CXDRAG) && y <= GetSystemMetrics(SM_CYDRAG))
            {
                if (HotItem != NULL)
                {
                    if (Border & blTop)
                    {
                        if (MainWindow->GetActivePanel() != FilesWindow)
                            MainWindow->ChangePanel();

                        CHotTrackItem* lastItem = NULL;
                        if (HotTrackItems.Count > 0)
                            lastItem = &HotTrackItems[HotTrackItems.Count - 1];
                        //if (HotItem->Chars != (int)TextLen) // tato podminka selhala pokud byl pripojen filtr
                        if (HotItem != lastItem)
                        {
                            // zkraceni cesty
                            char path[MAX_PATH];
                            strncpy(path, Text, HotItem->Chars);
                            path[HotItem->Chars] = 0;

                            if (FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->NotEmpty())
                                FilesWindow->GetPluginFS()->CompleteDirectoryLineHotPath(path, MAX_PATH);

                            FilesWindow->ChangeDir(path, -1, NULL, 2 /* jako back/forward in history*/, NULL, FALSE);
                        }
                        else
                        {
                            // klik na posledni komponentu -- change dir
                            SendMessage(MainWindow->HWindow, WM_COMMAND, MAKEWPARAM(CM_ACTIVE_CHANGEDIR, 0), 0);
                        }
                    }
                    if (Border & blBottom)
                    {
                        if (CopyTextToClipboard(Text + HotItem->Offset, HotItem->Chars))
                            FlashText(TRUE);
                    }
                }
                if (HotSize)
                {
                    // HotSize
                    FilesWindow->DriveInfo();
                }

                if (HotZoom)
                {
                    SendMessage(MainWindow->HWindow, WM_COMMAND,
                                MainWindow->LeftPanel == FilesWindow ? CM_LEFTZOOMPANEL : CM_RIGHTZOOMPANEL, 0);
                    UpdateWindow(MainWindow->HWindow); // aby nasledujici WM_MOUSEMOVE prisel uz do prekresleneho okna
                }

                if (HotSecurity)
                {
                    if (FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->NotEmpty())
                        FilesWindow->GetPluginFS()->ShowSecurityInfo(FilesWindow->HWindow);
                }
            }
        }
        LButtonDown = FALSE;
        RButtonDown = FALSE;
        if (!MainWindow->HelpMode && GetActiveWindow() == NULL)
            SetForegroundWindow(MainWindow->HWindow);
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));
        Paint(ps.hdc);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_ERASEBKGND:
    {
        HotTrackItemsMeasured = FALSE; // mohlo dojit ke zmene fontu - nechame znovu premerit
        RECT r;
        GetClientRect(HWindow, &r);
        if (Width != r.right || Height != r.bottom)
        {
            Width = r.right;
            Height = r.bottom;
            ItemBitmap.Enlarge(Width, Height); // alokace bitmapy v ItemBitmap.HMemDC
        }
        HDC dc = (HDC)wParam;
        if (Border != blNone)
        {
            HPEN oldPen = (HPEN)SelectObject(dc, BtnShadowPen);
            if (Border & blBottom)
            {
                MoveToEx(dc, r.left, r.top, NULL);
                LineTo(dc, r.left, r.bottom);
                SelectObject(dc, BtnHilightPen);
                MoveToEx(dc, r.right - 1, r.top, NULL);
                LineTo(dc, r.right - 1, r.bottom);
                MoveToEx(dc, r.left, r.bottom - 1, NULL);
                LineTo(dc, r.right, r.bottom - 1);
                r.bottom--;
            }
            SelectObject(dc, oldPen);
        }

        // j.r. vsechny statusbary tahat pres jednu cache CBitmap

        return TRUE;
    }

    case WM_TIMER:
    {
        if (wParam == IDT_THROBBER)
        {
            if (StopStatusbarRepaint == 0)
            {
                ThrobberFrame++;
                if (ThrobberFrame >= THROBBER_COUNT)
                    ThrobberFrame = 0;
                NeedToInvalidate = TRUE;
                InvalidateIfNeeded();
                //        RepaintThrobber();  // j.r. FIXME RepaintThrobber() by byl lepsi, ale zlobi pri resizu okna
            }
            else
                PostStatusbarRepaint = TRUE;
        }
        if (wParam == IDT_DELAYEDTHROBBER)
        {
            if (DelayedThrobber)
                SetThrobber(TRUE);
            else
            {
                KillTimer(HWindow, IDT_DELAYEDTHROBBER);
                TRACE_E("CStatusWindow::WindowProc(): Unexpected timer: IDT_DELAYEDTHROBBER");
            }
        }
        break;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

HIMAGELIST
CStatusWindow::CreateDragImage(const char* text, int& dxHotspot, int& dyHotspot, int& imgWidth, int& imgHeight)
{
    CALL_STACK_MESSAGE6("CStatusWindow::CreateDragImage(%s, %d, %d, %d, %d)",
                        text, dxHotspot, dyHotspot, imgWidth, imgHeight);
    int textLen = lstrlen(text);
    HDC hDC = ItemBitmap.HMemDC;
    HFONT hOldFont = (HFONT)SelectObject(hDC, Font);
    SIZE sz;
    GetTextExtentPoint32(hDC, text, textLen, &sz);
    ItemBitmap.Enlarge(sz.cx, sz.cy); // alokace bitmapy v ItemBitmap.HMemDC
    // podmazu pozadi
    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = sz.cx;
    r.bottom = sz.cy;
    FillRect(hDC, &r, HNormalBkBrush);
    int oldBkMode = SetBkMode(hDC, TRANSPARENT);
    int oldTextColor = SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_NORMAL]));
    DrawText(hDC, text, textLen, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SetTextColor(hDC, oldTextColor);
    SetBkMode(hDC, oldBkMode);
    SelectObject(hDC, hOldFont);

    dxHotspot = -15;
    dyHotspot = 0;

    imgWidth = sz.cx;
    imgHeight = sz.cy;
    HIMAGELIST himl = ImageList_Create(sz.cx, sz.cy, ILC_COLORDDB | ILC_MASK, 1, 0);
    SelectObject(ItemBitmap.HMemDC, ItemBitmap.HOldBmp); // na chvilku pustime bitmapu z HMemDC
    ImageList_AddMasked(himl, ItemBitmap.HBmp, GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));
    SelectObject(ItemBitmap.HMemDC, ItemBitmap.HBmp); // zase ji selectneme
    return himl;
}

BOOL CStatusWindow::GetTextFrameRect(RECT* r)
{
    CALL_STACK_MESSAGE_NONE
    if (HWindow == NULL)
        return FALSE;
    GetWindowRect(HWindow, r);
    //  r->left += TextRect.left - 2;
    r->top += 2;
    r->bottom -= 2;
    return TRUE;
}

BOOL CStatusWindow::GetFilterFrameRect(RECT* r)
{
    CALL_STACK_MESSAGE_NONE
    if (HWindow == NULL)
        return FALSE;

    if (!Hidden)
        return GetTextFrameRect(r);

    *r = HiddenRect;
    MapWindowPoints(HWindow, NULL, (POINT*)r, 2);
    return TRUE;
}

void CStatusWindow::OnColorsChanged()
{
    if (ToolBar != NULL)
        ToolBar->OnColorsChanged();
}

void CStatusWindow::SetFont()
{
    // mohlo dojit ke zmene velikosti fontu
    InvalidateRect(HWindow, NULL, TRUE);
    BuildHotTrackItems();
}
