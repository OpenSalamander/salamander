// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "tabwnd.h"
/*  #include "toolbar.h"
#include "cfgdlg.h"
#include "mainwnd.h"
#include "stswnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "shellib.h"*/

//
// ****************************************************************************
// CTabWindow
//

CTabWindow::CTabWindow(CFilesWindow* filesWindow) : CWindow(ooStatic) /*, HotTrackItems(10, 5)*/
                                                    {
                                                        CALL_STACK_MESSAGE_NONE}

                                                    CTabWindow::~CTabWindow()
{
    CALL_STACK_MESSAGE1("CTabWindow::~CTabWindow()");
}

int CTabWindow::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    int height = 2 + EnvFontCharHeight + 2;
    /*    if (Border & blTop)
  {
    height += 2 + 2;
//    int needed = ToolBar->GetNeededHeight();
    int needed = 3 + 16 + 3;
    if (height < needed) height = needed;
  }
  if (Border & blBottom) height++;*/
    return height;
}

LRESULT
CTabWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CTabWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        return 0;
    }

    case WM_DESTROY:
    {
        return 0;
    }

    case WM_SIZE:
    {
        RECT r;
        GetClientRect(HWindow, &r);

        break;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}
