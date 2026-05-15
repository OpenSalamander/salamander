// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//*****************************************************************************
//
// CToolTip
//
// This tooltip is supposed to eliminate the basic drawback of the original tooltip concept.
// Each window had its own tooltip object created. The second drawback was that we had
// to pass this object a list of regions over which the tooltips were supposed to pop up.
//
// New concept: CMainWindow owns only one tooltip (a single object instance).
// The tooltip window is created only at the moment when it is needed and in the thread
// that requested the display. Reason: we need the tooltip window to run in this thread.
// Up to version 2.6b6 inclusive the tooltip window ran in Salamander's main thread, and if that
// thread stopped, the tooltips were not displayed. When the mouse moves over a control
// that will use this tooltip, the control calls the SetCurrentID method whenever it enters
// a new region.
//
// The interface for working with the tooltip is declared in const.h so that all controls
// can use it without having to include mainwnd.h and tooltip.h.
//

// Messages used:
// WM_USER_TTGETTEXT - used to request the text with a specific ID
//   wParam = ID passed to SetCurrentToolTip
//   lParam = buffer (points to the tooltip buffer); the maximum number of characters is TOOLTIP_TEXT_MAX
//            before calling this message, the zeroth character is filled with the terminator
//            the text can contain \n to move to a new line and \t to insert a tab
// If the window writes a null-terminated string into the buffer, the tooltip will show it;
// otherwise the tooltip will not be shown.
//

class CToolTip : public CWindow
{
    enum TipTimerModeEnum
    {
        ttmNone,         // no timer is running
        ttmWaitingOpen,  // waiting for the tooltip to open
        ttmWaitingClose, // waiting for the tooltip to close
        ttmWaitingKill,  // waiting to exit the display mode
    };

protected:
    char Text[TOOLTIP_TEXT_MAX];
    int TextLen;
    HWND HNotifyWindow;
    DWORD LastID;
    TipTimerModeEnum WaitingMode;
    DWORD HideCounter;
    DWORD HideCounterMax;
    POINT LastCursorPos;
    BOOL IsModal;     // is our message loop running right now?
    BOOL ExitASAP;    // close as soon as possible and stop being modal
    UINT_PTR TimerID; // returned by SetTimer; needed for KillTimer

public:
    CToolTip(CObjectOrigin origin = ooStatic);
    ~CToolTip();

    BOOL RegisterClass();

    // hParent is necessary so that the tooltip closes together with it.
    // Without it we had cases where the parent's thread finished, but the tooltip window stayed
    // open and could no longer be closed (its thread no longer existed) -> crashes when
    // Salamander exited (fortunately before release 2.5b7).
    BOOL Create(HWND hParent);

    // This method starts a timer and, unless it is called again before the timer expires,
    // asks the 'hNotifyWindow' window for the text through the WM_USER_TTGETTEXT message and then
    // shows it under the cursor at its current coordinates.
    // The 'id' parameter identifies the region when communicating with the 'hNotifyWindow' window.
    // If the method is called more than once with the same 'id' parameter, the additional calls are ignored.
    // The value 0 for the 'hNotifyWindow' parameter is reserved for hiding the window and stopping
    // the running timer.
    // The 'showDelay' parameter matters if 'hNotifyWindow' != NULL:
    // if it is greater than or equal to 1, it specifies the delay before the tooltip is shown [ms]
    // if it equals 0, the default delay is used
    // if it is -1, the timer is not started at all
    void SetCurrentToolTip(HWND hNotifyWindow, DWORD id, int showDelay);

    // Suppresses showing the tooltip at the current mouse coordinates.
    // Useful to call when activating a window in which tooltips are used,
    // so there will not be unintended tooltip displays.
    void SuppressToolTipOnCurrentMousePos();

    // Returns TRUE if the text is shown; returns FALSE if no new text was supplied.
    // If considerCursor==TRUE, it checks the cursor and moves the tooltip below it.
    // If modal==TRUE, it starts a message loop that watches for messages to close the tooltip and returns only after it is hidden.
    BOOL Show(int x, int y, BOOL considerCursor, BOOL modal, HWND hParent);

    // extinguishes the tooltip
    void Hide();

    void OnTimer();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL GetText();
    void GetNeededWindowSize(SIZE* sz);

    void MessageLoop(); // for the modal tooltip variant

    void MySetTimer(DWORD elapse);
    void MyKillTimer();

    DWORD GetTime(BOOL init);
};
