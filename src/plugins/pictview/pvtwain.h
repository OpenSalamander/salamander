// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef ENABLE_TWAIN32

//****************************************************************************
//
// CTwain
//

class CViewerWindow;

class CTwain
{
private:
    HINSTANCE HTwainDLL;
    DSMENTRYPROC DSMEntry;

    TW_IDENTITY AppIdentity;
    TW_IDENTITY SrcIdentity;
    int TwainState;

    HWND HParent;
    CViewerWindow* Viewer;
    BOOL ShouldCloseViewerAfterClosingSM; // TRUE = zavrit viewer po zavreni Source Manageru

public:
    CTwain();
    ~CTwain();

    void ReleaseTwain();
    BOOL InitTwain(CViewerWindow* viewer);

    BOOL GetModalUI() { return TwainState >= 5; }
    BOOL IsSourceManagerOpened() { return TwainState >= 3; }
    void CloseViewerAfterClosingSM() { ShouldCloseViewerAfterClosingSM = TRUE; }

    BOOL SelectSource(BOOL* closingViewer);
    BOOL Acquire();

    BOOL IsTwainMessage(const MSG* msg);

private:
    TW_UINT16 CallTwainProc(TW_IDENTITY* origin,
                            TW_IDENTITY* dest,
                            TW_UINT32 dg,
                            TW_UINT16 dat,
                            TW_UINT16 msg,
                            TW_MEMREF data);

    BOOL IsTwainLoaded();
    BOOL OpenSourceManager();
    BOOL CloseSourceManager(BOOL* closingViewer);
    BOOL AcquireEnd();
    BOOL TransferImage();
};

#endif // ENABLE_TWAIN32
