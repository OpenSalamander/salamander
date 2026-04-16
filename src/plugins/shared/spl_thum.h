// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_thum) // so the structures are independent of the current packing alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//
// ****************************************************************************
// CSalamanderThumbnailMakerAbstract
//

// information about the image from which the thumbnail is generated; these flags are used
// in CSalamanderThumbnailMakerAbstract::SetParameters():
#define SSTHUMB_MIRROR_HOR 1                                            // image must be mirrored horizontally
#define SSTHUMB_MIRROR_VERT 2                                           // image must be mirrored vertically
#define SSTHUMB_ROTATE_90CW 4                                           // image must be rotated 90 degrees clockwise
#define SSTHUMB_ROTATE_180 (SSTHUMB_MIRROR_VERT | SSTHUMB_MIRROR_HOR)   // image must be rotated 180 degrees
#define SSTHUMB_ROTATE_90CCW (SSTHUMB_ROTATE_90CW | SSTHUMB_ROTATE_180) // image must be rotated 90 degrees counterclockwise
// the image is lower quality or smaller than required; after the first pass of obtaining
// fast thumbnails, Salamander will try to obtain a quality thumbnail for this image
#define SSTHUMB_ONLY_PREVIEW 8

class CSalamanderThumbnailMakerAbstract
{
public:
    // sets the image-processing parameters for thumbnail creation; this must be the first method called on this interface; 'picWidth' and 'picHeight' are the dimensions of the processed image (in pixels); 'flags' is a combination of SSTHUMB_XXX flags describing the image passed in the 'buffer' parameter to ProcessBuffer; returns TRUE if the downscaling buffers were allocated successfully and ProcessBuffer can then be called; if it returns FALSE, an error occurred and thumbnail loading must be terminated
    virtual BOOL WINAPI SetParameters(int picWidth, int picHeight, DWORD flags) = 0;

    // processes part of the image in 'buffer' (the processed part of the image is stored row by row from top to bottom, pixels within each row are stored left to right, and each pixel is represented by a 32-bit value composed of three R+G+B color bytes plus a fourth byte that is ignored); there are two processing modes: copying the image to the resulting thumbnail (if the processed image does not exceed the thumbnail size) and scaling the image down to the thumbnail (if the image is larger than the thumbnail); 'buffer' is read-only; 'rowsCount' specifies how many image rows are in the buffer;
    // if 'buffer' is NULL, the data is taken from the internal buffer (the plugin obtains it via GetBuffer);
    // returns TRUE if the plugin should continue loading the image; if it returns FALSE, thumbnail generation is finished (the whole image has been processed) or should be aborted as soon as possible (for example, the user changed the panel path, so the thumbnail is no longer needed)
    //
    // NOTE: while CPluginInterfaceForThumbLoader::LoadThumbnail is running, changing the panel path is blocked. Therefore larger images should be read and passed in chunks, and the return value of ProcessBuffer should be checked to determine whether loading should be aborted.
    // If time-consuming operations must be performed before calling SetParameters or before calling ProcessBuffer, GetCancelProcessing must be called occasionally during that time.
    virtual BOOL WINAPI ProcessBuffer(void* buffer, int rowsCount) = 0;

    // returns an internal buffer large enough to store 'rowsCount' rows of the image
    // (4 * 'rowsCount' * 'picWidth' bytes); if the object is in the error state (after calling
    // SetError), returns NULL;
    // the plugin must not deallocate the returned buffer (Salamander deallocates it automatically)
    virtual void* WINAPI GetBuffer(int rowsCount) = 0;

    // reports an error while obtaining the image (the thumbnail is considered invalid
    // and will not be used); after SetError is called, the other methods of this interface
    // only return errors (GetBuffer and SetParameters) or indicate that processing must stop
    // (ProcessBuffer)
    virtual void WINAPI SetError() = 0;

    // returns TRUE if the plugin should abort thumbnail loading
    // returns FALSE if the plugin should continue loading the image
    //
    // this method can be called before and after SetParameters
    //
    // it is used to detect abort requests when the plugin
    // needs to perform time-consuming operations before calling SetParameters
    // or when the plugin needs to prerender the image, that is, after calling
    // SetParameters but before calling ProcessBuffer
    virtual BOOL WINAPI GetCancelProcessing() = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForThumbLoaderAbstract
//

class CPluginInterfaceForThumbLoaderAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calls to methods (see CPluginInterfaceForThumbLoaderEncapsulation)
    friend class CPluginInterfaceForThumbLoaderEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // loads a thumbnail for file 'filename'; 'thumbWidth' and 'thumbHeight' are
    // the dimensions of the requested thumbnail; 'thumbMaker' is the interface of the
    // thumbnail-generation algorithm (it can accept a finished thumbnail or create it by scaling the
    // image down); returns TRUE if the format of 'filename' is recognized; if it returns
    // FALSE, Salamander zkusi nacist thumbnail pomoci jineho pluginu; chybu pri
    // errors while obtaining the thumbnail (for example, a file read error) are reported through
    // rozhrani 'thumbMaker' - viz metoda SetError; 'fastThumbnail' je TRUE v prvnim
    // thumbnail-loading pass, the goal is to return the thumbnail as quickly as possible (even
    // at lower quality or smaller than required); in the second thumbnail-loading pass
    // (only if the SSTHUMB_ONLY_PREVIEW flag is set in the first pass),
    // 'fastThumbnail' FALSE - cilem je vratit kvalitni thumbnail
    // limitation: because this is called from the icon-loading thread (not the main thread), only
    // methods of CSalamanderGeneralAbstract that are safe to call from any thread may be used
    //
    // Recommended implementation outline:
    //   - try to open the image
    //   - if that fails, return FALSE
    //   - extract the image dimensions
    //   - predat je do Salamandera pres thumbMaker->SetParameters
    //   - if it returns FALSE, clean up and exit (buffer allocation failed)
    //   - SMYCKA
    //     - load part of the image data
    //     - poslat je do Salamandera pres thumbMaker->ProcessBuffer
    //     - if it returns FALSE, clean up and exit (interrupted due to a panel path change)
    //     - continue in SMYCCE until the entire image has been passed
    //   - clean up and exit
    virtual BOOL WINAPI LoadThumbnail(const char* filename, int thumbWidth, int thumbHeight,
                                      CSalamanderThumbnailMakerAbstract* thumbMaker,
                                      BOOL fastThumbnail) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_thum)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
