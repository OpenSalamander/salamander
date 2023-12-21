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
#pragma pack(push, enter_include_spl_thum) // so that structures are independent of the set alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//
// ****************************************************************************
// CSalamanderThumbnailMakerAbstract
//

// information about image from which we generate thumbnail, these flags are used
// in CSalamanderThumbnailMakerAbstract::SetParameters():
#define SSTHUMB_MIRROR_HOR 1                                            // image must be mirrored horizontally
#define SSTHUMB_MIRROR_VERT 2                                           // image must be mirrored vertically
#define SSTHUMB_ROTATE_90CW 4                                           // image must be rotated 90 degrees clockwise
#define SSTHUMB_ROTATE_180 (SSTHUMB_MIRROR_VERT | SSTHUMB_MIRROR_HOR)   // image must be rotated 180 degrees
#define SSTHUMB_ROTATE_90CCW (SSTHUMB_ROTATE_90CW | SSTHUMB_ROTATE_180) // image must be rotated 90 degrees counter-clockwise

// image is in a worse quality or smaller than needed, Salamander after finishing
// the first round of getting "fast" thumbnails tries to get "quality" thumbnail
#define SSTHUMB_ONLY_PREVIEW 8

class CSalamanderThumbnailMakerAbstract
{
public:
    // setting of parameters for processing of image when creating thumbnail; must be called
    // as the first method of this interface; 'picWidth' and 'picHeight' are dimensions
    // of processed image (in points); 'flags' is a combination of flags SSTHUMB_XXX
    // which specify information about image passed in parameter 'buffer' in method
    // ProcessBuffer; returns TRUE if buffers for resizing were allocated and it is
    // possible to call ProcessBuffer then; if returns FALSE, there was an error
    // and it is necessary to stop loading of thumbnail
    virtual BOOL WINAPI SetParameters(int picWidth, int picHeight, DWORD flags) = 0;

    // processes part of image in buffer 'buffer' (processed part of image is stored
    // row by row from top to bottom, points on rows are stored from left to right,
    // each point is represented by 32-bit value which is composed of three bytes
    // with R+G+B colors and fourth byte which is ignored); we distinguish two types
    // of processing: copying of image to final thumbnail (if size of processed image
    // does not exceed size of thumbnail) and resizing of image to thumbnail (image
    // is larger than thumbnail); 'buffer' is used only for reading; 'rowsCount'
    // specifies how many rows of image are in buffer;
    // if 'buffer' is NULL, data are taken from own buffer (plugin gets it via GetBuffer);
    // returns TRUE if plugin should continue with loading of thumbnail, if returns
    // FALSE, creation of thumbnail is finished (whole image was processed) or it is
    // necessary to stop loading of thumbnail as soon as possible (e.g. user changed
    // path in panel, thumbnail is not needed anymore)
    // 
    // WARNING: if method CPluginInterfaceForThumbLoader::LoadThumbnail is running,
    // change of path in panel is blocked. For this reason it is necessary to pass
    // and load larger images in parts and test return value of method ProcessBuffer
    // if loading should be stopped.
    // If it is necessary to perform time-consuming operations before calling method
    // SetParameters or before calling ProcessBuffer, it is necessary to call
    // GetCancelProcessing from time to time during this period.
    virtual BOOL WINAPI ProcessBuffer(void* buffer, int rowsCount) = 0;

    // returns own buffer of size needed to store 'rowsCount' rows of image
    // (4 * 'rowsCount' * 'picWidth' bytes); if object is in error state (after
    // calling SetError), returns NULL;
    // plugin must not deallocate the obtained buffer (it is deallocated automatically
    // by Salamander)
    virtual void* WINAPI GetBuffer(int rowsCount) = 0;

    // reporting of error when getting image (thumbnail is considered as invalid
    // and it is not used), other methods of this interface will return errors
    // (GetBuffer and SetParameters) or stop processing (ProcessBuffer) after
    // calling SetError
    virtual void WINAPI SetError() = 0;

    // returns TRUE if plugin should stop loading of thumbnail
    // returns FALSE if plugin should continue with loading of thumbnail
    // 
    // method can be called before or after calling method SetParameters
    // 
    // serves for detection of request for interruption in cases when plugin
    // needs to perform time-consuming operations before calling SetParameters
    // or in cases when plugin needs to pre-render image, i.e. after calling
    // SetParameters, but before calling ProcessBuffer
    virtual BOOL WINAPI GetCancelProcessing() = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForThumbLoaderAbstract
//

class CPluginInterfaceForThumbLoaderAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calling of methods (see CPluginInterfaceForThumbLoaderEncapsulation)
    friend class CPluginInterfaceForThumbLoaderEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // loads thumbnail for file 'filename'; 'thumbWidth' and 'thumbHeight' are
    // dimensions of requested thumbnail; 'thumbMaker' is an interface of algorithm
    // for creating thumbnail (it can accept already created thumbnail or create
    // it by resizing image); returns TRUE if format of file 'filename' is known,
    // if returns FALSE, Salamander tries to load thumbnail using another plugin;
    // error when getting thumbnail (e.g. error when reading file) is reported
    // via interface 'thumbMaker' - see method SetError; 'fastThumbnail' is TRUE
    // in the first round of loading thumbnail - the goal is to return thumbnail
    // as soon as possible (even in worse quality or smaller than needed), in the
    // second round of loading thumbnail (only if in the first round flag
    // SSTHUMB_ONLY_PREVIEW is set) 'fastThumbnail' is FALSE - the goal is to
    // return thumbnail in good quality
    // limitation: because it is called from thread for loading icons (it is not
    // main thread), only methods which can be called from any thread can be used
    // 
    // Recommended schema of implementation:
    //   - try to open image
    //   - if it fails, return FALSE
    //   - extract dimensions of image
    //   - pass them to Salamander via thumbMaker->SetParameters
    //   - if it returns FALSE, cleanup and exit (failed to allocate buffers)
    //   - LOOP
    //     - load part of data from image
    //     - send them to Salamander via thumbMaker->ProcessBuffer
    //     - if it returns FALSE, cleanup and exit (interruption due to change of path)
    //     - continue in LOOP until whole image is processed
    //   - cleanup and exit
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
