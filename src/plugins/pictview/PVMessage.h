// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/pvw32dll.h"

#define PVC_ENVELOPE_NOT_LOADED ((PVCODE) - 123)

struct CPVWrapper
{
    TCHAR EnvelopePath[_MAX_PATH];
    TCHAR CommandLine[128];
    PROCESS_INFORMATION pi; // Remote PV Envelope process
    TCHAR MutexName[32];    // Name of the mutex hMutex
    HANDLE hMutex;          // Used to signal to the Envelope we are still alive to prevent zombies when AS dies
    DWORD MessageID;        // Each message sent to the Envelope has its unique ID
};

extern CPVWrapper PVWrapper;

// PVMessage sends a single message to the PV Envelope process and waits for the answer.
// It is for one time use, instead of using single instance for the entire lifetime of AS.
// of AS because most PV functions are thread-safe.
// FIXME: ensure each PVMessage request or LPPVHandle indeed creates extra thread in the remote process.
class PVMessage
{
protected:
    enum ePVMSG
    {
        PVMSG_GetDLLVersion,
        PVMSG_CloseImage,
        PVMSG_DrawImage,
        PVMSG_GetErrorText,
        PVMSG_SetBkHandle,
        PVMSG_SetStretchParameters,
        PVMSG_ChangeImage,
        PVMSG_IsOutCombSupported,
        PVMSG_CropImage,
        PVMSG_OpenImageEx,
        PVMSG_GetImageInfo,
        PVMSG_ReadImage,
        PVMSG_ReadImageWithoutProgress,
        PVMSG_SaveImage,
        PVMSG_SaveImageWithoutProgress,
        PVMSG_LoadFromClipboard,
        PVMSG_ReadImageSequence,
        PVMSG_GetRGBAtCursor,
        PVMSG_CalculateHistogram,
        PVMSG_CreateThumbnail,
        PVMSG_SimplifyImageSequence,
        // Internal messages for communication SPL -> Envelope, not stubbing PVW32Cnv.dll
        PVMSG_InitTexts,
        PVMSG_CloseHandle
    };

    typedef struct PVMessageHeader
    {                 // Header at the beginning of pData
        DWORD cbSize; // Including this header
        DWORD Type;
        DWORD PVHandle;
        PVCODE ResultCode;
    } PVMessageHeader, *LPPVMessageHeader;

public:
    PVMessage(ePVMSG type, size_t dataSize, LPPVHandle pvHandle = NULL);
    PVMessage();
    PVMessage(LPPVMessageHeader pHdr);

    virtual ~PVMessage();

    static bool HandleMessage(DWORD dataSize, DWORD id, HANDLE hEvent);
    static bool HandlePostedMessage(DWORD message, DWORD data);

    bool IsInited();
    bool Exec();
    virtual bool HandleRequest() { return false; }
    LPBYTE GetData(); // Returns pointer to data shared with the Envelope process
    PVCODE GetResultCode();
    LPPVHandle GetPVHandle();
    class PVWrapperImageHandle* GetWImg() { return pWImg; }

protected:
    HANDLE hFileMap;
    struct PVMessageHeader* pData; // Obtained via MapViewOfFile(hFileMap)
    DWORD iID;
    class PVWrapperImageHandle* pWImg;

    typedef struct PVImageInfoStruct
    {
        DWORD cbSize;
        DWORD Width, Height;
        DWORD BytesPerLine;
        DWORD FileSize;
        DWORD Colors;
        DWORD Format;
        DWORD Flags;
        DWORD ColorModel;
        DWORD NumOfImages;
        DWORD CurrentImage;
        char Info1[PV_MAX_INFO_LEN], Info2[PV_MAX_INFO_LEN], Info3[PV_MAX_INFO_LEN];
        DWORD StretchedWidth;
        DWORD StretchedHeight;
        DWORD StretchMode;
        DWORD HorDPI;
        DWORD VerDPI;
        bool bFSI; // True when FSI is filled and valid
        PVFormatSpecificInfo FSI;
        DWORD Compression;
        DWORD TotalBitDepth;
        DWORD CommentSize;
        char Comment[256];
    } PVImageInfoStruct, *LPPVImageInfoStruct;

    bool Init(ePVMSG type, size_t dataSize, LPPVHandle pvHandle);

    bool MarshalImageInfo(LPPVImageInfo pInInfo, LPPVImageInfoStruct pOutInfo);
    bool UnmarshalImageInfo(LPPVImageInfoStruct pInInfo, LPPVImageInfo pOutInfo);

    static PVMessage* GetPVMessage(LPPVMessageHeader pHdr, HANDLE hEvent);
};

// Internal message used to feed translated strings from Salamander.exe to Enveloper
class PVMessage_InitTexts : public PVMessage
{
public:
    PVMessage_InitTexts();
    PVMessage_InitTexts(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

private:
    typedef struct PVMessageInitTexts : PVMessageHeader
    {
        int TextsCount;
        char Texts[1];
    } PVMessageInitTexts, *LPPVMessageInitTexts;
};

class PVMessage_GetErrorText : public PVMessage
{
public:
    PVMessage_GetErrorText(DWORD ErrorCode);
    PVMessage_GetErrorText(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();
    const char* GetErrorText();

private:
    typedef struct PVMessageGetErrorText : PVMessageHeader
    {
        DWORD ErrorCode;
        char ErrorText[512];
    } PVMessageGetErrorText, *LPPVMessageGetErrorText;
};

class PVMessage_OpenImageEx : public PVMessage
{
public:
    PVMessage_OpenImageEx(LPPVOpenImageExInfo pOpenExInfo);
    PVMessage_OpenImageEx(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool IsInited();
    bool Exec(LPPVImageInfo pImgInfo);
    bool HandleRequest();

private:
    typedef struct PVMessageOpenImageEx : PVMessageHeader
    {
        // PVOpenImageExInfo
        DWORD Flags;
        char FileName[260]; // Contains FileMap name when Flags contains PVOF_ATTACH_TO_HANDLE
        DWORD DataSize;     // Contains size of FileMap data when Flags contains PVOF_ATTACH_TO_HANDLE
        // PVImageInfo
        PVImageInfoStruct ImageInfo;
    } PVMessageOpenImageEx, *LPPVMessageOpenImageEx;
};

class PVMessage_GetImageInfo : public PVMessage
{
public:
    PVMessage_GetImageInfo(LPPVHandle pvHandle, int ImageIndex);
    PVMessage_GetImageInfo(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool Exec(LPPVImageInfo pImgInfo);
    bool HandleRequest();

private:
    typedef struct PVMessageGetImageInfo : PVMessageHeader
    {
        int ImageIndex;
        PVImageInfoStruct ImageInfo;
    } PVMessageGetImageInfo, *LPPVMessageGetImageInfo;
};

class PVMessageWithProgress : public PVMessage
{
public:
    PVMessageWithProgress(ePVMSG type, size_t dataSize, LPPVHandle pvHandle = NULL);
    PVMessageWithProgress(LPPVMessageHeader pHdr, HANDLE Event) : PVMessage(pHdr), hEvent(Event) {};
    bool Exec(TProgressProc Progress, void* AppSpecific);
    BOOL HandleProgress(int done);

protected:
    typedef enum PVState_ReadImage
    {
        PVState_Started,
        PVState_Progressing,
        PVState_Cancelled,
        PVState_Finished
    } PVState_ReadImage;
    typedef struct PVMessageWithProgressHeader : PVMessageHeader
    {
        LONG State; // typecast to PVState_ReadImage; LONG used for InterlockedExchange
        int ProgressValue;
    } PVMessageWithProgressHeader, *LPPVMessageWithProgressHeader;
    HANDLE hEvent;
};

class PVMessage_ReadImage : public PVMessageWithProgress
{
public:
    PVMessage_ReadImage(LPPVHandle pvHandle, int ImageIndex, bool bProgress);
    PVMessage_ReadImage(LPPVMessageHeader pHdr, HANDLE hEvent) : PVMessageWithProgress(pHdr, hEvent) {};
    bool Exec(HDC PaintDC, RECT* pDRect, TProgressProc Progress, void* AppSpecific);
    bool HandleRequest();

private:
    typedef struct PVMessageReadImage : PVMessageWithProgressHeader
    {
        int ImageIndex;
    } PVMessageReadImage, *LPPVMessageReadImage;
};

class PVMessage_CloseImage : public PVMessage
{
public:
    PVMessage_CloseImage(LPPVHandle pvHandle);
    PVMessage_CloseImage(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    ~PVMessage_CloseImage();
    bool HandleRequest();
};

class PVMessage_DrawImage : public PVMessage
{
public:
    PVMessage_DrawImage(LPPVHandle pvHandle, HDC PaintDC, int X, int Y, LPRECT pDrawRect);
    PVMessage_DrawImage(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool Exec(HDC PaintDC);
    bool HandleRequest();

private:
    typedef struct PVMessageDrawImage : PVMessageHeader
    {
        DWORD BitsPerPixel;
        DWORD X, Y;
        RECT DrawRect;
        char SharedMemoryName[16];
        DWORD InternalFileMapHandle; // Envelope's handle to the shared memory object
    } PVMessageDrawImage, *LPPVMessageDrawImage;
};

class PVMessage_SaveImage : public PVMessageWithProgress
{
public:
    PVMessage_SaveImage(LPPVHandle pvHandle, const char* FileName, LPPVSaveImageInfo pSii, int ImageIndex, bool bProgress);
    PVMessage_SaveImage(LPPVMessageHeader pHdr, HANDLE hEvent) : PVMessageWithProgress(pHdr, hEvent) {};
    bool HandleRequest();

private:
    typedef struct PVMessageSaveImage : PVMessageWithProgressHeader
    {
        DWORD cbSize;
        DWORD Format;
        DWORD Compression;
        DWORD Colors;
        DWORD ColorModel;
        DWORD Flags;
        DWORD Width; /* No scaling applied if Width or Height is zero */
        DWORD Height;
        DWORD HorDPI;
        DWORD VerDPI;
        DWORD CommentSize;
        char Comment[256];
        char FormatSpecificInfo[256];
        DWORD CropLeft; /* Cropping applied before scaling */
        DWORD CropTop;
        DWORD CropWidth;  /* Crop size */
        DWORD CropHeight; /* No cropping applied if CropWidth or CropHeight is zero */
        struct
        {
            BYTE Flags; /* PVTF_XXX flags */
            union
            {
                BYTE Index; /* Used only when Flags is PVTF_INDEX */
                struct
                {
                    BYTE Red, Green, Blue; /* Used only when Flags is PVTF_RGB */
                } RGB;
            } Value;
        } Transp;
        int ImageIndex;
        // When Flags contains PVSF_USERDEFINED_OUTPUT, FileName contains shared memory name,
        // its size is in OutFileMapSize and InternalFileMapHandle contains Envelope's HANDLE to it.
        char FileName[260];
        DWORD OutFileMapSize;
        DWORD InternalFileMapHandle;
    } PVMessageSaveImage, *LPPVMessageSaveImage;
    friend PVCODE WINAPI PVSaveImageStub(LPPVHandle, const char*, LPPVSaveImageInfo, TProgressProc, void*, int);
};

class PVMessage_LoadFromClipboard : public PVMessage
{
public:
    PVMessage_LoadFromClipboard();
    PVMessage_LoadFromClipboard(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool Exec(LPPVImageInfo pImgInfo);
    bool HandleRequest();

private:
    typedef struct PVMessageLoadFromClipboard : PVMessageHeader
    {
        PVImageInfoStruct ImageInfo;
    } PVMessageLoadFromClipboard, *LPPVMessageLoadFromClipboard;
};

class PVMessage_ReadImageSequence : public PVMessage
{
public:
    PVMessage_ReadImageSequence(LPPVHandle pvHandle);
    PVMessage_ReadImageSequence(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool Exec();
    bool HandleRequest();

private:
    typedef struct PVMessageReadImageSequence : PVMessageHeader
    {
        DWORD NumberOfFrames;
    } PVMessageReadImageSequence, *LPPVMessageReadImageSequence;
};

class PVMessage_SetBkHandle : public PVMessage
{
public:
    PVMessage_SetBkHandle(LPPVHandle pvHandle, COLORREF bkColor);
    PVMessage_SetBkHandle(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

private:
    typedef struct PVMessageSetBkHandle : PVMessageHeader
    {
        COLORREF BackgroundColor;
    } PVMessageSetBkHandle, *LPPVMessageSetBkHandle;
};

class PVMessage_GetDLLVersion : public PVMessage
{
public:
    PVMessage_GetDLLVersion();
    PVMessage_GetDLLVersion(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool Exec(DWORD& Version);
    bool HandleRequest();

private:
    typedef struct PVMessageGetDLLVersion : PVMessageHeader
    {
        DWORD DLLVersion;
    } PVMessageGetDLLVersion, *LPPVMessageGetDLLVersion;
};

class PVMessage_SetStretchParameters : public PVMessage
{
public:
    PVMessage_SetStretchParameters(LPPVHandle pvHandle, DWORD Width, DWORD Height, DWORD Mode);
    PVMessage_SetStretchParameters(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

private:
    typedef struct PVMessageSetStretchParameters : PVMessageHeader
    {
        DWORD Width;
        DWORD Height;
        DWORD Mode;
    } PVMessageSetStretchParameters, *LPPVMessageSetStretchParameters;
};

class PVMessage_ChangeImage : public PVMessage
{
public:
    PVMessage_ChangeImage(LPPVHandle pvHandle, DWORD Flags);
    PVMessage_ChangeImage(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

private:
    typedef struct PVMessageChangeImage : PVMessageHeader
    {
        DWORD Flags;
    } PVMessageChangeImage, *LPPVMessageChangeImage;
};

class PVMessage_IsOutCombSupported : public PVMessage
{
public:
    PVMessage_IsOutCombSupported(int Fmt, int Compr, int Colors, int ColorModel);
    PVMessage_IsOutCombSupported(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

private:
    typedef struct PVMessageIsOutCombSupported : PVMessageHeader
    {
        int Fmt;
        int Compr;
        int Colors;
        int ColorModel;
    } PVMessageIsOutCombSupported, *LPPVMessageIsOutCombSupported;
};

class PVMessage_CropImage : public PVMessage
{
public:
    PVMessage_CropImage(LPPVHandle pvHandle, int Left, int Top, int Width, int Height);
    PVMessage_CropImage(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

private:
    typedef struct PVMessageCropImage : PVMessageHeader
    {
        int Left;
        int Top;
        int Width;
        int Height;
    } PVMessageCropImage, *LPPVMessageCropImage;
};

class PVMessage_GetRGBAtCursor : public PVMessage
{
public:
    PVMessage_GetRGBAtCursor(LPPVHandle pvHandle, DWORD Colors, int x, int y);
    PVMessage_GetRGBAtCursor(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

    RGBQUAD* GetRGB();
    int GetIndex();

private:
    typedef struct PVMessageGetRGBAtCursor : PVMessageHeader
    {
        DWORD Colors; // Helper to speed things a little up
        int X;
        int Y;
        RGBQUAD RGB;
        int Index;
    } PVMessageGetRGBAtCursor, *LPPVMessageGetRGBAtCursor;
};

class PVMessage_CalculateHistogram : public PVMessage
{
public:
    PVMessage_CalculateHistogram(LPPVHandle pvHandle, const PVImageInfo* pvii);
    PVMessage_CalculateHistogram(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool HandleRequest();

    void GetResults(LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb);

private:
    typedef struct PVMessageCalculateHistogram : PVMessageHeader
    {
        DWORD Colors; // Helper to speed things a little up
        DWORD Width;
        DWORD Height;
        DWORD BytesPerLine;
        DWORD luminosity[256];
        DWORD red[256];
        DWORD green[256];
        DWORD blue[256];
        DWORD rgb[256];
    } PVMessageCalculateHistogram, *LPPVMessageCalculateHistogram;
};

class PVMessage_CreateThumbnail : public PVMessageWithProgress
{
public:
    PVMessage_CreateThumbnail(LPPVHandle pvHandle, LPPVSaveImageInfo pSii, int ImageIndex, DWORD imgWidth, DWORD imgHeight, DWORD thumbWidth, DWORD thumbHeight);
    PVMessage_CreateThumbnail(LPPVMessageHeader pHdr, HANDLE hEvent) : PVMessageWithProgress(pHdr, hEvent) {};
    bool HandleRequest();
    LPBYTE GetPixelData();

private:
    typedef struct PVMessageCreateThumbnail : PVMessageWithProgressHeader
    {
        DWORD ThumbWidth;  // Target thumbnail width
        DWORD ThumbHeight; // Target thumbnail height
        DWORD ImageWidth;  // Original image width
        DWORD ImageHeight; // Original image height
        DWORD Flags;
        int ImageIndex;
        BYTE Buffer[1];
    } PVMessageCreateThumbnail, *LPPVMessageCreateThumbnail;
};

class PVMessage_SimplifyImageSequence : public PVMessage
{
public:
    PVMessage_SimplifyImageSequence(LPPVHandle pvHandle, DWORD ScreenWidth, DWORD ScreenHeight, const COLORREF& bgColor);
    PVMessage_SimplifyImageSequence(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool Exec();
    bool HandleRequest();
    LPPVImageSequence GetImageSequence();

private:
    typedef struct PVMessageSimplifyImageSequence : PVMessageHeader
    {
        DWORD ScreenWidth;
        DWORD ScreenHeight;
        DWORD BitsPerPixel;
        COLORREF BackgroundColor;
        char SharedMemoryName[16];
        DWORD InternalFileMapHandle; // Envelope's handle to the shared memory object
        DWORD Delay[1];              // Size of the array is NumberOfFrames
    } PVMessageSimplifyImageSequence, *LPPVMessageSimplifyImageSequence;
};

// Internal message, used to return Win32 HANDLE to Envelope to be closed as no longer used
class PVMessage_CloseHandle : public PVMessage
{
public:
    PVMessage_CloseHandle(DWORD Handle);
    PVMessage_CloseHandle(LPPVMessageHeader pHdr) : PVMessage(pHdr) {};
    bool Exec();
    bool HandleRequest();

private:
    DWORD HandleToBeClosed;
};

bool LoadEnvelope();
