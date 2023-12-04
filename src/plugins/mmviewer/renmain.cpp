// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mmviewer.rh"
#include "mmviewer.rh2"
#include "lang\lang.rh"
#include "output.h"
#include "renderer.h"
//#include "dialogs.h"
#include "mmviewer.h"
#include "parser.h"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

//****************************************************************************
//
// CRendererWindow
//

CRendererWindow::CRendererWindow(int enumFilesSourceUID, int enumFilesCurrentIndex)
    : CWindow(ooStatic)
{
    FileName[0] = 0;
    Viewer = NULL;
    Creating = TRUE;
    EnumFilesSourceUID = enumFilesSourceUID;
    EnumFilesCurrentIndex = enumFilesCurrentIndex;
}

CRendererWindow::~CRendererWindow()
{
}

void CRendererWindow::OnFileOpen()
{
    char file[MAX_PATH];
    file[0] = 0;
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = HWindow;
    char* s = LoadStr(IDS_VIEWERFILTER);
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (SalGeneral->SafeGetOpenFileName(&ofn))
    {
        OpenFile(file);
    }
}

BOOL CRendererWindow::OpenFile(const char* name)
{
    // pokud pri OpenFile dojde k chybe, bude podmazano pozadi
    InvalidateRect(HWindow, NULL, TRUE);

    Output.DestroyItems();

    CParserInterface* parser;
    CParserResultEnum result = CreateAppropriateParser(name, &parser);
    if (result == preOK)
    {
        const char* oFName = name;
        const char* tmp = strrchr(name, '\\');
        if (tmp)
            oFName = tmp + 1;

        Output.AddHeader(oFName, TRUE);
        Output.AddSeparator();

        parser->GetFileInfo(&Output);
        parser->CloseFile();
        delete (parser);
        Output.PrepareForRender(HWindow);

        lstrcpy(FileName, name);
    }
    else
    {
        char buff[2 * MAX_PATH];
        sprintf(buff, LoadStr(IDS_ERROR_OPENING), name);
        SalGeneral->SalMessageBox(HWindow, buff, LoadStr(IDS_PLUGIN_NAME), MB_ICONEXCLAMATION);

        FileName[0] = 0;
    }

    SetViewerTitle();

    HDC hDC;
    if ((hDC = GetDC(HWindow)) != NULL)
    {
        int headerW = ComputeExtents(hDC, sLeft, FALSE, TRUE);
        ComputeExtents(hDC, sRight, TRUE);

        //nedelej sirsi, blbe se to cte
        if (sRight.cx > 800)
            sRight.cx = 800;

        if (sLeft.cx + sRight.cx < headerW)
            sRight.cx = headerW - sLeft.cx;

        width = sLeft.cx + sRight.cx;
        height = sRight.cy;

        TRACE_I("MMV: sRight.cx: " << sRight.cx << ", sLeft.cx: " << sLeft.cx);
        ReleaseDC(HWindow, hDC);
    }

    Paint(hDC, TRUE, 0);
    Paint(hDC, TRUE, 0xFFFFFFFF);
    SetupScrollBars();
    InvalidateRect(HWindow, NULL, TRUE);
    PostMessage(HWindow, WM_SIZE, 0, 0);

    Viewer->UpdateEnablers();

    return (result == preOK);
}

void CRendererWindow::SetupScrollBars()
{
    RECT r;
    GetClientRect(HWindow, &r);

    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE | SIF_RANGE;
    si.nMin = 0;
    si.nMax = width;
    si.nPage = r.right;
    SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);

    si.nMax = height;
    si.nPage = r.bottom;
    SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
}

void CRendererWindow::SetViewerTitle()
{
    char title[MAX_PATH + 300];
    if (FileName[0] != 0)
        sprintf(title, "%s - %s", FileName, LoadStr(IDS_PLUGIN_NAME));
    else
        sprintf(title, "%s", LoadStr(IDS_PLUGIN_NAME));

    SetWindowText(GetParent(HWindow), title);
}

LRESULT CRendererWindow::OnCommand(WPARAM wParam, LPARAM lParam)
{
    switch (LOWORD(wParam))
    {
    case CM_COPY:
    {
#ifndef _UNICODE
        TDirectArray<wchar_t> text(4096, 4096);

        int i;
        for (i = 0; i < Output.GetCount(); i++)
        {
            const COutputItem* item = Output.GetItem(i);

            if (item->Name)
            {
                wchar_t* str = AnsiToWide(item->Name, (int)strlen(item->Name));
                text.Add(str, (int)wcslen(str));

                if (item->Value)
                {
                    text.Add('\t');
                    if (item->Flags & OIF_UTF8)
                        str = UTF8ToWide(item->Value, (int)strlen(item->Value));
                    else
                        str = AnsiToWide(item->Value, (int)strlen(item->Value));
                    text.Add(str, (int)wcslen(str));
                }
            }

            text.Add(L"\r\n", 2);
        }
        text.Add('\0');
        SalGeneral->CopyTextToClipboardW(&text[0], -1, FALSE, NULL);
#else // _UNICODE
        TDirectArray<TCHAR> text(4096, 4096);

        int i;
        for (i = 0; i < Output.GetCount(); i++)
        {
            const COutputItem* item = Output.GetItem(i);

            if (item->Name)
            {
                text.Add(item->Name, _tcslen(item->Name));

                if (item->Value)
                {
                    text.Add('\t');
                    text.Add(item->Value, _tcslen(item->Value));
                }
            }

            text.Add(_T("\r\n"), 2);
        }
        text.Add('\0');
        SalGeneral->CopyTextToClipboard(&text[0], -1, FALSE, NULL);
#endif
    }
    break;

    case CM_FILE_FIRST:
    case CM_FILE_PREV:
    case CM_FILE_NEXT:
    case CM_FILE_LAST:
    {
        BOOL ok = FALSE;
        BOOL srcBusy = FALSE;
        BOOL noMoreFiles = FALSE;
        char fileName[MAX_PATH];
        fileName[0] = 0;
        int enumFilesCurrentIndex = EnumFilesCurrentIndex;
        if (LOWORD(wParam) == CM_FILE_PREV || LOWORD(wParam) == CM_FILE_LAST)
        {
            if (LOWORD(wParam) == CM_FILE_LAST)
                enumFilesCurrentIndex = -1;
            ok = SalGeneral->GetPreviousFileNameForViewer(EnumFilesSourceUID,
                                                          &enumFilesCurrentIndex,
                                                          FileName, FALSE, TRUE,
                                                          fileName, &noMoreFiles,
                                                          &srcBusy);
        }
        else
        {
            if (LOWORD(wParam) == CM_FILE_FIRST)
                enumFilesCurrentIndex = -1;
            ok = SalGeneral->GetNextFileNameForViewer(EnumFilesSourceUID,
                                                      &enumFilesCurrentIndex,
                                                      FileName, FALSE, TRUE,
                                                      fileName, &noMoreFiles,
                                                      &srcBusy);
        }

        if (ok) // mame nove jmeno
        {
            if (lstrcmpi(fileName, FileName) != 0)
            {
                if (Viewer->Lock != NULL)
                {
                    SetEvent(Viewer->Lock);
                    Viewer->Lock = NULL; // ted uz je to jen na disk-cache
                }
                if (!OpenFile(fileName))
                {
                    FileName[0] = 0;
                }

                // index nastavime i v pripade neuspechu, aby user mohl prejit na dalsi/predchozi obrazek
                EnumFilesCurrentIndex = enumFilesCurrentIndex;
            }
        }
        else
            int fixme = 0;
        return 0;
    }

    case CM_FILES_EXPORT_HTML:
    {
        char fname[MAX_PATH];
        char* ext = LoadStr(IDS_HTMLEXT);
        strcpy(fname, FileName);
        char* b = strrchr(fname, '.'); // ".cvspass" ve Windows je pripona
        if (b)
            *b = 0;
        strcat(fname, ext);

        char* s = LoadStr(IDS_HTMLEXPFILTER);

        if (GetOpenFileName(HWindow, NULL, s, fname, ext, TRUE))
        {
            int r;
            if ((r = ExportToHTML(fname, Output)) > 0)
            {
                if (SalGeneral->SalMessageBox(HWindow, LoadStr(IDS_EXPORTOPEN), LoadStr(IDS_PLUGIN_NAME), MB_YESNO | MB_ICONQUESTION) == DIALOG_YES)
                    ExecuteFile(fname);
                //SalGeneral->ExecuteAssociation(HWindow, NULL, fname);
            }
            else
            {
                switch (r)
                {
                case -1:
                case -2:
                default: //prozatim vsechny chyby write error
                    SalGeneral->SalMessageBox(HWindow, LoadStr(IDS_MMV_WRITE_ERROR), LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
                    break;
                }
            }
        }
    }
        return 0;

    case CM_FILES_EXPORT_XML:
    {
        char fname[MAX_PATH];
        strcpy(fname, FileName);
        char* b = strrchr(fname, '.'); // ".cvspass" ve Windows je pripona
        if (b)
            *b = 0;
        strcat(fname, ".xml");

        //TODO
    }
        return 0;

    case CM_FILES_OPEN:
    {
        OnFileOpen();
        return 0;
    }

    case CM_ABOUT:
    {
        MMViewerAbout(HWindow);
        return 0;
    }

    case CM_EXIT:
    {
        DestroyWindow(GetParent(HWindow));
        return 0;
    }

    case CM_FULLSCREEN:
    {
        WPARAM wParam2;
        if (IsZoomed(GetParent(HWindow)))
            wParam2 = SC_RESTORE;
        else
            wParam2 = SC_MAXIMIZE;
        SendMessage(GetParent(HWindow), WM_SYSCOMMAND, wParam2, 0);
        return 0;
    }
    }
    return CWindow::WindowProc(WM_COMMAND, wParam, lParam);
} /* CRendererWindow::OnCommand */

LRESULT
CRendererWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_TAB:
        {
            if (Output.GetCount() > 1)
            {
                HWND h = NULL;

                if (KEY_DOWN(VK_SHIFT))
                {
                    //najdi prvni edit control odzadu
                    int i;
                    for (i = Output.GetCount(); i--;)
                        if (Output.GetItem(i)->hwnd)
                        {
                            h = Output.GetItem(i)->hwnd;
                            break;
                        }
                }
                else
                {
                    //najdi prvni edit control odpredu
                    int i;
                    for (i = 0; i < Output.GetCount(); i++)
                        if (Output.GetItem(i)->hwnd)
                        {
                            h = Output.GetItem(i)->hwnd;
                            break;
                        }
                }

                SetFocus(h);
                SendMessage(h, EM_SETSEL, 0, -1);
                SendMessage(h, WM_ENSUREVISIBLE, 0, 0);
                TRACE_I("CRendererWindow::WindowProc: VK_TAB: " << h);
                return 0;
            }
        }
        break;
        }

        WORD wScrollNotify = -1;

        switch (wParam)
        {
        case VK_ESCAPE:
            PostMessage(GetParent(HWindow), WM_CLOSE, wParam, lParam);
            break;

        case VK_UP:
            wScrollNotify = SB_LINEUP;
            break;

        case VK_PRIOR:
            wScrollNotify = SB_PAGEUP;
            break;

        case VK_NEXT:
            wScrollNotify = SB_PAGEDOWN;
            break;

        case VK_DOWN:
            wScrollNotify = SB_LINEDOWN;
            break;

        case VK_HOME:
            wScrollNotify = SB_TOP;
            break;

        case VK_END:
            wScrollNotify = SB_BOTTOM;
            break;
        }

        if (wScrollNotify != -1)
            SendMessage(HWindow, WM_VSCROLL, MAKELONG(wScrollNotify, 0), 0L);

        wScrollNotify = -1;

        switch (wParam)
        {
        case VK_LEFT:
            wScrollNotify = (KEY_DOWN(VK_CONTROL)) ? SB_PAGELEFT : SB_LINELEFT;
            break;

        case VK_RIGHT:
            wScrollNotify = (KEY_DOWN(VK_CONTROL)) ? SB_PAGERIGHT : SB_LINERIGHT;
            break;

        case VK_HOME:
        case VK_END:
            wScrollNotify = SB_LEFT;
            break;
        }

        if (wScrollNotify != -1)
            SendMessage(HWindow, WM_HSCROLL, MAKELONG(wScrollNotify, 0), 0L);

        break;
    }
        /*
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:*/
    case WM_MOUSEACTIVATE:
        SetFocus(HWindow);
        break;

    case WM_COMMAND:
    {
        LRESULT res = OnCommand(wParam, lParam);
        Viewer->UpdateEnablers();
        return res;
    }

    case WM_ERASEBKGND:
    {
        if (Output.GetCount())
            return 1;
        // Let Windows erase the bground for us if we have nothing to show...
        break;
    }

    case WM_HSCROLL:
    {
        SCROLLINFO si;

        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;

        GetScrollInfo(HWindow, SB_HORZ, &si);

        switch (LOWORD(wParam))
        {
        case SB_LEFT:
            si.nPos = 0;
            break;

        case SB_LINELEFT:
            si.nPos -= (FontHeight + 1);
            break;

        case SB_LINERIGHT:
            si.nPos += (FontHeight + 1);
            break;

        case SB_PAGELEFT:
            si.nPos -= si.nPage;
            break;

        case SB_PAGERIGHT:
            si.nPos += si.nPage;
            break;

        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;

        default:
            break;
        }

        si.fMask = SIF_POS;
        SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);
        GetScrollInfo(HWindow, SB_HORZ, &si);

        InvalidateRect(HWindow, NULL, TRUE);
        Paint(NULL, TRUE); //posun editboxy
    }
        return 0;

    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
    {
        SCROLLINFO si;

        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(HWindow, SB_VERT, &si);

        if (uMsg == WM_MOUSEWHEEL)
        {
            int zDelta = ((short)HIWORD(wParam));

            wParam = (zDelta < 0) ? SB_LINEDOWN : SB_LINEUP;
        }

        switch (LOWORD(wParam))
        {
        case SB_TOP:
            si.nPos = si.nMin;
            break;

        case SB_BOTTOM:
            si.nPos = si.nMax;
            break;

        case SB_LINEUP:
            si.nPos -= (FontHeight + 1);
            break;

        case SB_LINEDOWN:
            si.nPos += (FontHeight + 1);
            break;

        case SB_PAGEUP:
            si.nPos -= si.nPage;
            break;

        case SB_PAGEDOWN:
            si.nPos += si.nPage;
            break;

        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;

        default:
            break;
        }

        si.fMask = SIF_POS;
        SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
        GetScrollInfo(HWindow, SB_VERT, &si);

        InvalidateRect(HWindow, NULL, TRUE);
        Paint(NULL, TRUE); //posun editboxy
    }
        return 0;

    case WM_SIZE:
        SetupScrollBars();
        Paint(NULL, TRUE); //posun editboxy
        InvalidateRect(HWindow, NULL, TRUE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(HWindow, &ps);
        if (hDC != NULL)
            Paint(hDC, FALSE);
        EndPaint(HWindow, &ps);
        return 0;
    }

    case WM_ENSUREVISIBLE:
    {
#define OFSX(ofs) \
    { \
        xPos += ofs; \
        r.left = rr.left; \
        r.right = rr.right; \
        OffsetRect(&r, -xPos, 0); \
    }
#define OFSY(ofs) \
    { \
        yPos += ofs; \
        r.top = rr.top; \
        r.bottom = rr.bottom; \
        OffsetRect(&r, 0, -yPos); \
    }

        RECT r, rr, fr;

        fr = *(RECT*)lParam;
        GetClientRect(HWindow, &r);
        rr = r;

        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        GetScrollInfo(HWindow, SB_VERT, &si);
        int yPos = si.nPos;
        GetScrollInfo(HWindow, SB_HORZ, &si);
        int xPos = si.nPos;

        OffsetRect(&r, -xPos, -yPos);
        OffsetRect(&fr, -xPos, -yPos);

        TRACE_I("CRendererWindow::WindowProc:WM_ENSUREVISIBLE (" << fr.left << ", " << fr.top << ", " << fr.right << ", " << fr.bottom << ") ->(" << r.left << ", " << r.top << ", " << r.right << ", " << r.bottom << ")");
        /*
      if (fr.right > r.right)
        OFSX(fr.right - r.right)*/

        if (fr.left < r.left)
            OFSX(fr.left - r.left)
        /*else
        if (fr.left > r.right)
          OFSX(fr.left - r.left)*/

        if (fr.top < r.top)
            OFSY(fr.top - r.top)
        else if (fr.bottom > r.bottom)
            OFSY(fr.bottom - r.bottom);

        si.fMask = SIF_POS;
        si.nPos = yPos;
        SetScrollInfo(HWindow, SB_VERT, &si, TRUE);
        si.nPos = xPos;
        SetScrollInfo(HWindow, SB_HORZ, &si, TRUE);

        InvalidateRect(HWindow, NULL, TRUE);
        Paint(NULL, TRUE); //posun editboxy
    }
        return 0;
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}
