// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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

    // allocates the internal data for shrinking and returns TRUE on success
    // returns FALSE if the allocations fail
    BOOL Alloc(DWORD origWidth, DWORD origHeight,
               WORD newWidth, WORD newHeight,
               DWORD* outBuff, BOOL processTopDown);

    // destroys the allocated buffers and initializes the variables
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
// Used to shrink the original picture into a thumbnail.
//

class CSalamanderThumbnailMaker //: public CSalamanderThumbnailMakerAbstract
{
protected:
    //    CFilesWindow *Window;  // panel window whose icon reader we operate in

    DWORD* Buffer;  // dedicated buffer for the rows of data from the plugin
    int BufferSize; // size of the 'Buffer' buffer
    BOOL Error;     // if TRUE, an error occurred during thumbnail processing (the result is unusable)
    int NextLine;   // index of the next processed line

    DWORD* ThumbnailBuffer;    // shrunken image
    DWORD* AuxTransformBuffer; // auxiliary buffer of the same size as ThumbnailBuffer (used for transferring data during transformation + the buffers are swapped after the transformation)
    int ThumbnailMaxWidth;     // maximum theoretical thumbnail dimensions (in points)
    int ThumbnailMaxHeight;
    int ThumbnailRealWidth;  // actual dimensions of the shrunken image (in points)
    int ThumbnailRealHeight; //

    // parameters of the processed image
    int OriginalWidth;
    int OriginalHeight;
    DWORD PictureFlags;
    BOOL ProcessTopDown;

    CShrinkImage Shrinker; // handles shrinking the image
    BOOL ShrinkImage;

public:
    CSalamanderThumbnailMaker(/*CFilesWindow *window*/);
    ~CSalamanderThumbnailMaker();

    // Calculates the thumbnail size, given image and max thumbnail sizes
    static BOOL CalculateThumbnailSize(int originalWidth, int originalHeight,
                                       int maxWidth, int maxHeight,
                                       int& thumbWidth, int& thumbHeight);

    // cleans the object - called before processing another thumbnail or when a thumbnail
    // (finished or not) from this object is no longer needed
    // the 'thumbnailMaxSize' parameter specifies the maximum width and height of the thumbnail in points;
    // if it equals -1, it is ignored
    void Clear(int thumbnailMaxSize = -1);
    /*
    // returns TRUE if a complete thumbnail is ready within this object (retrieval from the plugin succeeded)
    BOOL ThumbnailReady();

    // performs the thumbnail transformation according to PictureFlags (SSTHUMB_MIRROR_VERT is already done,
    // SSTHUMB_MIRROR_HOR and SSTHUMB_ROTATE_90CW remain)
    void TransformThumbnail();

    // converts the finished thumbnail to a DDB and stores its dimensions and raw data into 'data'
    BOOL RenderToThumbnailData(CThumbnailData *data);*/

    // if the entire thumbnail was not created and no error occurred (see 'Error'),
    // fill the remainder of the thumbnail with white (so that remnants of the previous thumbnail do not appear
    // in the undefined part of the thumbnail); if even three rows of the thumbnail were not created,
    // nothing is filled in (the thumbnail would be useless anyway)
    void HandleIncompleteImages();

    //    BOOL IsOnlyPreview() {return (PictureFlags & SSTHUMB_ONLY_PREVIEW) != 0;}

    void* WINAPI GetThumbnailBuffer() { return ThumbnailBuffer; }

    // *********************************************************************************
    // methods of the CSalamanderThumbnailMakerAbstract interface
    // *********************************************************************************

    virtual BOOL WINAPI SetParameters(int picWidth, int picHeight, DWORD flags);
    virtual BOOL WINAPI ProcessBuffer(void* buffer, int rowsCount);
    virtual void* WINAPI GetBuffer(int rowsCount);
    virtual void WINAPI SetError() { Error = TRUE; }
    virtual BOOL WINAPI GetCancelProcessing();
};
