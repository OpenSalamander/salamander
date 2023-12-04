// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef ENABLE_WIA

// I have used source code from GetImage sample from MSVC 2008
// (C:\Program Files (x86)\Microsoft Visual Studio 9.0\Samples\1033\
//  AllVCLanguageSamples.zip\C++\OS\WindowsXP\GetImage)
// GetImage sample copy: pictview\doc\GetImage-sample-WIA.7z

//****************************************************************************
//
// CWiaWrap
//

class CWiaWrap
{
protected:
    DWORD MyThreadID;                     // this object can be used only in thread with this ID
    BOOL WiaWrapUsed;                     // object was used = COM is already initialized
    BOOL WiaWrapOK;                       // FALSE = COM was not initialized, object cannot be used
    BOOL AcquiringImage;                  // TRUE = just acquiring image (AcquireImage() is in progress)
    BOOL ShouldCloseParentAfterAcquiring; // TRUE = zavrit parenta po dokonceni AcquireImage()

public:
    CWiaWrap();
    ~CWiaWrap();

    BOOL IsAcquiringImage() { return AcquiringImage; }
    void CloseParentAfterAcquiring() { ShouldCloseParentAfterAcquiring = TRUE; }

    // let user to acquire image from scanner (or other device) using WIA (Windows Image Acquisition);
    // returns bitmap handle on success, do not forget to destroy it when it is no longer needed,
    // returns NULL when cancelled or failed, error message is displayed in messagebox;
    // if scanner returns more images, all images except the first one are added to 'extraScanImages'
    // array (if '*extraScanImages==NULL', array will be allocated if needed), do not forget to
    // destroy it and delete all contained bitmaps when it is no longer needed; if you don't want
    // to receive more images, set 'extraScanImages' to NULL (images will be deleted after scanning)
    HBITMAP AcquireImage(HWND parent, TDirectArray<HBITMAP>** extraScanImages, BOOL* closeParent);
};

#endif // ENABLE_WIA
