// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//******************************************************************************
//
// CShrinkImage
//

class CShrinkImage
{
protected:
    DWORD NormCoeffX, NormCoeffY;
    DWORD* RowCoeff;
    DWORD* ColCoeff;
    DWORD* YCoeff;
    DWORD NormCoeff;
    DWORD Y, YBndr;
    DWORD* OutLine;
    DWORD* Buff;
    DWORD OrigHeight;
    WORD NewWidth;
    BOOL ProcessTopDown;

public:
    CShrinkImage();
    ~CShrinkImage();

    // Allocates internal data for shrinking and returns TRUE on success.
    // Returns FALSE if the allocation fails.
    BOOL Alloc(DWORD origWidth, DWORD origHeight,
               WORD newWidth, WORD newHeight,
               DWORD* outBuff, BOOL processTopDown);

    // Destroys allocated buffers and reinitializes member variables.
    void Destroy();

    void ProcessRows(DWORD* inBuff, DWORD rowCount);

protected:
    DWORD* CreateCoeff(DWORD origLen, WORD newLen, DWORD& norm);
    void Cleanup();
};

//******************************************************************************
//
// CSalamanderThumbnailMaker
//
// Used to shrink the original image into a thumbnail.
//

class CSalamanderThumbnailMaker : public CSalamanderThumbnailMakerAbstract
{
protected:
    CFilesWindow* Window; // Panel window whose icon reader we operate within.

    DWORD* Buffer;  // Dedicated buffer for image rows supplied by the plugin.
    int BufferSize; // Size of the 'Buffer' buffer.
    BOOL Error;     // If TRUE, an error occurred while processing the thumbnail, so the result is unusable.
    int NextLine;   // Index of the next row to process.

    DWORD* ThumbnailBuffer;    // Buffer that holds the reduced image.
    DWORD* AuxTransformBuffer; // Auxiliary buffer of the same size as ThumbnailBuffer.
                               // Used to hand off data during transformations; afterwards the buffers swap roles.
    int ThumbnailMaxWidth;     // Maximum theoretical thumbnail width (in points).
    int ThumbnailMaxHeight;    // Maximum theoretical thumbnail height (in points).
    int ThumbnailRealWidth;    // Actual width of the reduced image (in points).
    int ThumbnailRealHeight;   // Actual height of the reduced image (in points).

    // Parameters of the image being processed.
    int OriginalWidth;
    int OriginalHeight;
    DWORD PictureFlags;
    BOOL ProcessTopDown;

    CShrinkImage Shrinker; // Handles image shrinking.
    BOOL ShrinkImage;

public:
    CSalamanderThumbnailMaker(CFilesWindow* window);
    ~CSalamanderThumbnailMaker();

    // Cleans up the object—call before processing another thumbnail or
    // whenever the object no longer needs one (finished or not).
    // The 'thumbnailMaxSize' parameter specifies the maximum thumbnail width
    // and height in points; if it equals -1, the limit is ignored.
    void Clear(int thumbnailMaxSize = -1);

    // Returns TRUE if the complete thumbnail is already available in this object
    // (successfully obtained from the plugin).
    BOOL ThumbnailReady();

    // Performs thumbnail transformations according to PictureFlags.
    // SSTHUMB_MIRROR_VERT is already handled; SSTHUMB_MIRROR_HOR and SSTHUMB_ROTATE_90CW remain.
    void TransformThumbnail();

    // Converts the finished thumbnail to a DDB and stores its dimensions and raw data in 'data'.
    BOOL RenderToThumbnailData(CThumbnailData* data);

    // If the full thumbnail was not created and no error occurred (see 'Error'),
    // fill the remainder of the thumbnail with white so undefined parts do not
    // show leftovers from the previous thumbnail. If fewer than three rows
    // were produced, leave it empty (the thumbnail would be useless anyway).
    void HandleIncompleteImages();

    BOOL IsOnlyPreview() { return (PictureFlags & SSTHUMB_ONLY_PREVIEW) != 0; }

    // *********************************************************************************
    // Methods of the CSalamanderThumbnailMakerAbstract interface.
    // *********************************************************************************

    virtual BOOL WINAPI SetParameters(int picWidth, int picHeight, DWORD flags);
    virtual BOOL WINAPI ProcessBuffer(void* buffer, int rowsCount);
    virtual void* WINAPI GetBuffer(int rowsCount);
    virtual void WINAPI SetError() { Error = TRUE; }
    virtual BOOL WINAPI GetCancelProcessing();
};
