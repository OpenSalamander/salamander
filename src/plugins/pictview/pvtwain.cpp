// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef ENABLE_TWAIN32

#include "lib/pvw32dll.h"
#include "renderer.h"
#include "dialogs.h"
#include "pictview.h"
#include "twain/twain.h"
#include "pvtwain.h"

//****************************************************************************
//
// CTwain
//

CTwain::CTwain()
{
    HTwainDLL = NULL;
    DSMEntry = NULL;
    Viewer = NULL;
    TwainState = 1;
    ShouldCloseViewerAfterClosingSM = FALSE;

    memset(&AppIdentity, 0, sizeof(AppIdentity));
    AppIdentity.Version.MajorNum = 1;
    AppIdentity.Version.MinorNum = 0;
    AppIdentity.Version.Language = TWLG_ENGLISH_USA;
    AppIdentity.Version.Country = TWCY_USA;
#ifdef _UNICODE
    WideCharToMultiByte(CP_ACP, 0, VERSINFO_VERSION, -1, AppIdentity.Version.Info,
                        sizeof(AppIdentity.Version.Info), NULL, NULL);
    AppIdentity.Version.Info[sizeof(AppIdentity.Version.Info) - 1] = 0;
#else
    strcpy(AppIdentity.Version.Info, VERSINFO_VERSION);
#endif
    AppIdentity.ProtocolMajor = TWON_PROTOCOLMAJOR;
    AppIdentity.ProtocolMinor = TWON_PROTOCOLMINOR;
    AppIdentity.SupportedGroups = DG_IMAGE | DG_CONTROL;
    strcpy(AppIdentity.Manufacturer, "Open Salamander");
    strcpy(AppIdentity.ProductFamily, "Open Salamander plugin");
    strcpy(AppIdentity.ProductName, PLUGIN_NAME_EN);

    memset(&SrcIdentity, 0, sizeof(SrcIdentity));
}

CTwain::~CTwain()
{
    ReleaseTwain();
}

BOOL CTwain::IsTwainLoaded()
{
    return (HTwainDLL != NULL && DSMEntry != NULL);
}

BOOL CTwain::InitTwain(CViewerWindow* viewer)
{
    if (IsTwainLoaded())
        return TRUE;

    if (TwainState != 1)
        TRACE_E("CTwain::InitTwain(): TwainState != 1!");

    Viewer = viewer;
    HParent = Viewer->HWindow;

    TCHAR buff[MAX_PATH];
    GetWindowsDirectory(buff, SizeOf(buff));
    if (buff[(_tcslen(buff) - 1)] != '\\')
        _tcscat(buff, _T("\\"));
    _tcscat(buff, _T("TWAIN_32.DLL"));

    HTwainDLL = LoadLibrary(buff);

    if (HTwainDLL == NULL)
    {
        SalamanderGeneral->SalMessageBox(HParent,
                                         LoadStr(IDS_TWAIN_OPEN), LoadStr(IDS_PLUGINNAME),
                                         MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }

    DSMEntry = (DSMENTRYPROC)GetProcAddress(HTwainDLL, MAKEINTRESOURCEA(1));
    if (DSMEntry == NULL)
    {
        FreeLibrary(HTwainDLL);
        HTwainDLL = NULL;
        SalamanderGeneral->SalMessageBox(HParent,
                                         LoadStr(IDS_TWAIN_OPEN), LoadStr(IDS_PLUGINNAME),
                                         MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }

    TwainState = 2;

    return TRUE;
}

void CTwain::ReleaseTwain()
{
    if (IsTwainLoaded())
    {
        if (TwainState != 2)
            TRACE_E("CTwain::ReleaseTwain(): TwainState != 2!");

        FreeLibrary(HTwainDLL);
        HTwainDLL = NULL;
        DSMEntry = NULL;
        TwainState = 1;
    }
}

TW_UINT16
CTwain::CallTwainProc(TW_IDENTITY* origin,
                      TW_IDENTITY* dest,
                      TW_UINT32 dg,
                      TW_UINT16 dat,
                      TW_UINT16 msg,
                      TW_MEMREF data)
{
    if (!IsTwainLoaded())
    {
        TRACE_E("Twain is not loaded");
        return TWCC_BUMMER;
    }

    TW_UINT16 ret_val;
    ret_val = (*DSMEntry)(origin, dest, dg, dat, msg, data);
    //  m_returnCode = ret_val;
    if (ret_val == TWRC_FAILURE)
    {
        TW_STATUS twStatus;
        (*DSMEntry)(origin, dest, DG_CONTROL, DAT_STATUS, MSG_GET, &twStatus);
        int resID = IDS_TWCC_BUMMER; // uknown error
        switch (twStatus.ConditionCode)
        {
        case TWCC_LOWMEMORY:
            resID = IDS_TWCC_LOWMEMORY;
            break;
        case TWCC_NODS:
            resID = IDS_TWCC_NODS;
            break;
        case TWCC_MAXCONNECTIONS:
            resID = IDS_TWCC_MAXCONNECTIONS;
            break;
        case TWCC_OPERATIONERROR:
            resID = IDS_TWCC_OPERATIONERROR;
            break;
        case TWCC_BADCAP:
            resID = IDS_TWCC_BADCAP;
            break;
        case TWCC_BADPROTOCOL:
            resID = IDS_TWCC_BADPROTOCOL;
            break;
        case TWCC_BADVALUE:
            resID = IDS_TWCC_BADVALUE;
            break;
        case TWCC_SEQERROR:
            resID = IDS_TWCC_SEQERROR;
            break;
        case TWCC_BADDEST:
            resID = IDS_TWCC_BADDEST;
            break;
        case TWCC_CAPUNSUPPORTED:
            resID = IDS_TWCC_CAPUNSUPPORTED;
            break;
        case TWCC_CAPBADOPERATION:
            resID = IDS_TWCC_CAPBADOPERATION;
            break;
        case TWCC_CAPSEQERROR:
            resID = IDS_TWCC_CAPSEQERROR;
            break;
        case TWCC_DENIED:
            resID = IDS_TWCC_DENIED;
            break;
        case TWCC_FILEEXISTS:
            resID = IDS_TWCC_FILEEXISTS;
            break;
        case TWCC_FILENOTFOUND:
            resID = IDS_TWCC_FILENOTFOUND;
            break;
        case TWCC_NOTEMPTY:
            resID = IDS_TWCC_NOTEMPTY;
            break;
        case TWCC_PAPERJAM:
            resID = IDS_TWCC_PAPERJAM;
            break;
        case TWCC_PAPERDOUBLEFEED:
            resID = IDS_TWCC_PAPERDOUBLEFEED;
            break;
        case TWCC_FILEWRITEERROR:
            resID = IDS_TWCC_FILEWRITEERROR;
            break;
        case TWCC_CHECKDEVICEONLINE:
            resID = IDS_TWCC_CHECKDEVICEONLINE;
            break;
        }
        SalamanderGeneral->SalMessageBox(HParent,
                                         LoadStr(resID), LoadStr(IDS_TWAIN_ERROR),
                                         MB_ICONEXCLAMATION | MB_OK);
    }
    return ret_val;
}

BOOL CTwain::OpenSourceManager()
{
    if (!IsTwainLoaded())
    {
        TRACE_E("Twain is not loaded");
        return FALSE;
    }
    if (TwainState != 2)
    {
        TRACE_E("CTwain::OpenSourceManager(): TwainState != 2");
        return FALSE;
    }
    ShouldCloseViewerAfterClosingSM = FALSE; // prevence proti necekanemu zavreni vieweru (zatim o to neslo pozadat)

    TW_UINT16 ret_val = CallTwainProc(&AppIdentity, NULL, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, (TW_MEMREF)&HParent);
    if (ret_val == TWRC_SUCCESS)
    {
        TwainState = 3;
        return TRUE;
    }
    return FALSE;
}

BOOL CTwain::CloseSourceManager(BOOL* closingViewer)
{
    if (!IsTwainLoaded())
    {
        TRACE_E("Twain is not loaded");
        return FALSE;
    }
    if (TwainState != 3)
    {
        TRACE_E("CTwain::CloseSourceManager(): TwainState != 3");
        return FALSE;
    }

    TW_UINT16 ret_val = CallTwainProc(&AppIdentity, NULL, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, (TW_MEMREF)&HParent);
    if (ret_val == TWRC_SUCCESS)
    {
        TwainState = 2;
        if (ShouldCloseViewerAfterClosingSM)
        {
            if (closingViewer != NULL)
                *closingViewer = TRUE; // nepocitat enablery, nezdrzovat
            ShouldCloseViewerAfterClosingSM = FALSE;
            PostMessage(Viewer->HWindow, WM_CLOSE, 0, 0);
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CTwain::SelectSource(BOOL* closingViewer)
{
    BOOL ret = FALSE;
    if (OpenSourceManager())
    {
        ret = CallTwainProc(&AppIdentity, NULL, DG_CONTROL, DAT_IDENTITY, MSG_USERSELECT, &SrcIdentity) == TWCC_SUCCESS;
        CloseSourceManager(closingViewer);
    }
    return ret;
}

BOOL CTwain::Acquire()
{
    if (TwainState != 2)
        TRACE_E("CTwain::Acquire(): TwainState != 2");
    if (OpenSourceManager())
    {
        if (CallTwainProc(&AppIdentity, NULL, DG_CONTROL, DAT_IDENTITY, MSG_GETDEFAULT, &SrcIdentity) == TWCC_SUCCESS)
        {
            if (CallTwainProc(&AppIdentity, NULL, DG_CONTROL, DAT_IDENTITY, MSG_OPENDS, &SrcIdentity) == TWCC_SUCCESS)
            {
                TwainState = 4;
                TW_USERINTERFACE twUI;
                twUI.ShowUI = TRUE;
                twUI.ModalUI = TRUE;
                twUI.hParent = (TW_HANDLE)HParent;
                if (CallTwainProc(&AppIdentity, &SrcIdentity, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, (TW_MEMREF)&twUI) == TWCC_SUCCESS)
                {
                    TwainState = 5;
                    EnableWindow(HParent, FALSE); // modal UI, disable parent window
                }
            }
        }
        if (TwainState < 5)
            AcquireEnd();
    }
    return (TwainState == 5);
}

BOOL CTwain::AcquireEnd()
{
    if (TwainState > 5)
        TRACE_E("CTwain::AcquireEnd(): TwainState > 5");
    if (TwainState == 5)
    {
        EnableWindow(HParent, TRUE); // end of modal UI, enable parent window
        SetForegroundWindow(HParent);
        TW_USERINTERFACE twUI;
        CallTwainProc(&AppIdentity, &SrcIdentity, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, &twUI);
        TwainState = 4;
    }
    if (TwainState == 4)
    {
        CallTwainProc(&AppIdentity, NULL, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, (TW_MEMREF)&SrcIdentity);
        TwainState = 3;
    }
    if (Viewer->ExtraScanImages != NULL)
        PostMessage(Viewer->HWindow, WM_USER_SCAN_EXTRA_IMAGES, 0, 0);
    CloseSourceManager(NULL);
    return TRUE;
}

BOOL CTwain::IsTwainMessage(const MSG* msg)
{
    if (TwainState >= 5)
    {
        TW_EVENT twEvent;
        twEvent.pEvent = (TW_MEMREF)msg;
        twEvent.TWMessage = MSG_NULL;
        TW_INT16 rc = CallTwainProc(&AppIdentity, &SrcIdentity, DG_CONTROL, DAT_EVENT, MSG_PROCESSEVENT, (TW_MEMREF)&twEvent);
        switch (twEvent.TWMessage)
        {
        case MSG_XFERREADY:
        {
            TwainState = 6;
            TransferImage();
            break;
        }

        case MSG_CLOSEDSREQ:
        {
            AcquireEnd();
            break;
        }
        }
        if (rc != TWRC_NOTDSEVENT)
            return TRUE;
    }
    return FALSE;
}

BOOL CTwain::TransferImage()
{
    if (TwainState != 6)
        TRACE_E("CTwain::TransferImage(): TwainState != 6");

    TW_IMAGEINFO twImageInfo;
    TW_UINT16 rc;
    TW_UINT32 hBitmap;
    BOOL transferred = FALSE;
    BOOL pendingXfers = TRUE;
    BOOL firstScan = TRUE;

    while (pendingXfers)
    {
        rc = CallTwainProc(&AppIdentity, &SrcIdentity, DG_IMAGE, DAT_IMAGEINFO, MSG_GET, (TW_MEMREF)&twImageInfo);
        if (rc == TWRC_SUCCESS)
        {
            hBitmap = NULL;
            rc = CallTwainProc(&AppIdentity, &SrcIdentity, DG_IMAGE, DAT_IMAGENATIVEXFER, MSG_GET, (TW_MEMREF)&hBitmap);
            switch (rc)
            {
            case TWRC_XFERDONE:
            {
                TwainState = 7;

                LPBITMAPINFOHEADER lpdib = (LPBITMAPINFOHEADER)GlobalLock((HBITMAP)(UINT_PTR)hBitmap);
                HDC hDC = GetDC(NULL);
                void* bits;
                HBITMAP hBmp = CreateDIBSection(hDC, (BITMAPINFO*)lpdib, DIB_RGB_COLORS, &bits, NULL, 0);
                int paletteSize = 0;
                switch (lpdib->biBitCount)
                {
                case 1:
                    paletteSize = 2;
                    break;
                case 4:
                    paletteSize = 16;
                    break;
                case 8:
                    paletteSize = 256;
                    break;
                }
                DWORD bytes = ((lpdib->biBitCount * lpdib->biWidth + 31) >> 3) & ~3;

                // preneseme data
                memcpy(bits, (BYTE*)lpdib + sizeof(BITMAPINFOHEADER) + paletteSize * sizeof(RGBQUAD), lpdib->biHeight * bytes);

                // original uz nepotrebujeme, staci kopie, uvolnime ho pred predanim 'hBmp' do
                // vieweru (tam se dela dalsi kopie)
                GlobalUnlock((HBITMAP)(UINT_PTR)hBitmap);
                GlobalFree((void*)(UINT_PTR)hBitmap);
                hBitmap = NULL;
                ReleaseDC(NULL, hDC);

                if (firstScan)
                {
                    transferred = Viewer->OpenScannedImage(hBmp);
                    firstScan = FALSE;
                }
                else
                {
                    // SCAN MULTIPAGE - all images except the first one is stored in 'Viewer->ExtraScanImages' for later handling
                    if (Viewer->ExtraScanImages == NULL)
                        Viewer->ExtraScanImages = new TDirectArray<HBITMAP>(10, 30);
                    Viewer->ExtraScanImages->Add(hBmp);
                }

                break;
            }

            case TWRC_CANCEL:
            {
                TwainState = 7;
                break;
            }

            case TWRC_FAILURE:
            {
                // Petr: bez nasledujiciho volani se mi skenovani pri Paper Jam kouslo, skenovaci okna
                // zustala otevrena, musel jsem to cely killnout; na netu jsem nasel sample TWAIN, kde
                // pri TWRC_FAILURE volaji nasledujici a i u me to resi situaci, uz se to nekouse...

                // abort all pending images
                TW_PENDINGXFERS twPendingXfers2;
                TW_UINT16 rc2 = CallTwainProc(&AppIdentity, &SrcIdentity, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, (TW_MEMREF)&twPendingXfers2);
                TwainState = 5;
                pendingXfers = FALSE; // vratime rizeni do message loop

                break;
            }
            }

            if (rc == TWRC_XFERDONE || rc == TWRC_CANCEL)
            {
                if (hBitmap != NULL)
                {
                    // GlobalUnlock((HBITMAP)hBitmap);  // Petr: neni zamcene, tedy neodemykame
                    GlobalFree((void*)(UINT_PTR)hBitmap);
                    hBitmap = NULL;
                }

                //end transfer
                TW_PENDINGXFERS twPendingXfers;
                rc = CallTwainProc(&AppIdentity, &SrcIdentity, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, (TW_MEMREF)&twPendingXfers);
                if (rc == TWRC_SUCCESS)
                {
                    // Patera 2009.03.12: FIXME: this prevents multipage scan, we don't support it in the PictView engine
                    // twPendingXfers.Count = 0;  // Petr: toto vynulovani vede k zablokovani skeneru a PV, pomuze az restart, takze to resime jinak...
                    if (twPendingXfers.Count == 0)
                    {
                        TwainState = 5;
                        pendingXfers = FALSE; // vratime rizeni do message loop
                    }
                    if (twPendingXfers.Count == (TW_UINT16)-1 || twPendingXfers.Count > 0)
                    {
                        TwainState = 6; // pokracujeme dalsim obrazkem

                        // nasledujici kod bohuzel na mem skeneru (Canon i-SENSYS MF8030Cn) nefunguje = kousne se to
                        // a musi se sestrelit driver skeneru nez to zase jde pouzivat
                        //            // abort all pending images
                        //            rc = CallTwainProc(&AppIdentity, &SrcIdentity, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, (TW_MEMREF)&twPendingXfers);
                        //            if (rc == TWRC_SUCCESS)
                        //            {
                        //              TwainState = 5;
                        //              pendingXfers = FALSE; // vratime rizeni do message loop
                        //            }
                    }
                }
            }
        }
    }
    return transferred;
}

#endif // ENABLE_TWAIN32
