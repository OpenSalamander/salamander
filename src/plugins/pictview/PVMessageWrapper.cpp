// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef PICTVIEW_DLL_IN_SEPARATE_PROCESS

#include "pictview.h"
#include "PVMessage.h"

// PVWrapperImageHandle wraps LPPVHandle for use by the Wrapper
class PVWrapperImageHandle
{
public:
    PVWrapperImageHandle(DWORD Img);
    ~PVWrapperImageHandle();

    bool CreateSharedMemoryForHBitmap(HBITMAP hBitmap, char* pSharedMemoryName, int maxSharedMemoryName, DWORD* pDataSize);
    bool CreateSharedMemoryForMemoryBlock(LPBYTE pBuffer, DWORD dataSize, char* pSharedMemoryName, int maxSharedMemoryName);
    void FreeSharedMemory();

    HANDLE hEnvelopeProcess;
    DWORD hPVHandle;  // This is LPPVHandle passed from the Envelope to the Wrapper
    HANDLE hFileMap;  // Memory used to pass data via PVOF_ATTACH_TO_HANDLE
    DWORD iFileMapID; // ID used to determine file mapping name
    char* Comment;
    LPPVFormatSpecificInfo FSI;
    LPPVImageSequence PVImageSequence; // Allocated by the wrapper
    DWORD NumberOfFrames;
    HANDLE hImageSequenceFileMap;
};

typedef PVWrapperImageHandle* LPPVWrapperImageHandle;

PVMessage::PVMessage(ePVMSG type, size_t dataSize, LPPVHandle pvHandle)
    : pData(NULL), hFileMap(NULL), pWImg(NULL)
{
    Init(type, dataSize, pvHandle);
}

bool PVMessage::Init(ePVMSG type, size_t dataSize, LPPVHandle pvHandle)
{
    TCHAR fileMapName[48];
    LPPVMessageHeader pHdr;

    pWImg = (LPPVWrapperImageHandle)pvHandle;
    iID = PVWrapper.MessageID++;
    _sntprintf(fileMapName, SizeOf(fileMapName), _T("%s_%d"), PVWrapper.MutexName, iID);
    hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 0, (DWORD)(dataSize + sizeof(PVMessageHeader)), fileMapName);
    if (!hFileMap)
    {
        return false;
    }
    pData = (LPPVMessageHeader)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, dataSize + sizeof(PVMessageHeader));
    if (!pData)
    {
        return false;
    }
    memset(pData, 0, dataSize + sizeof(PVMessageHeader));
    pHdr = (LPPVMessageHeader)pData;
    pHdr->cbSize = (DWORD)(dataSize + sizeof(PVMessageHeader));
    pHdr->Type = type;
    pHdr->PVHandle = pWImg ? pWImg->hPVHandle : NULL;
    pHdr->ResultCode = PVC_ENVELOPE_NOT_LOADED;
    return true;
}

bool PVMessage::IsInited()
{
    if (!hFileMap || !pData)
        return false;
    if (!LoadEnvelope())
        return false;
    return true;
}

bool PVMessage::Exec()
{
    TCHAR eventName[32];
    HANDLE hEvent;

    _sntprintf(eventName, SizeOf(eventName), _T("%s_ev%x"), PVWrapper.MutexName, iID);
    hEvent = CreateEvent(NULL, FALSE, FALSE, eventName);
    if (!hEvent)
    {
        return false;
    }
    PostThreadMessage(PVWrapper.pi.dwThreadId, WM_USER, pData->cbSize, iID);
    HANDLE handles[2] = {hEvent, PVWrapper.pi.hProcess};
    DWORD ret = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    CloseHandle(hEvent);
    if (ret == 1)
    {
        // The remote process died
        return false;
    }
    return ret == 0;
}

LPPVHandle PVMessage::GetPVHandle()
{
    return pWImg;
}

bool PVMessage::UnmarshalImageInfo(LPPVImageInfoStruct pInInfo, LPPVImageInfo pOutInfo)
{
    pOutInfo->cbSize = sizeof(PVImageInfo);
    pOutInfo->Width = pInInfo->Width;
    pOutInfo->Height = pInInfo->Height;
    pOutInfo->BytesPerLine = pInInfo->BytesPerLine;
    pOutInfo->FileSize = pInInfo->FileSize;
    pOutInfo->Colors = pInInfo->Colors;
    pOutInfo->Format = pInInfo->Format;
    pOutInfo->Flags = pInInfo->Flags;
    pOutInfo->ColorModel = pInInfo->ColorModel;
    pOutInfo->NumOfImages = pInInfo->NumOfImages;
    pOutInfo->CurrentImage = pInInfo->CurrentImage;
    strcpy(pOutInfo->Info1, pInInfo->Info1);
    strcpy(pOutInfo->Info2, pInInfo->Info2);
    strcpy(pOutInfo->Info3, pInInfo->Info3);
    pOutInfo->StretchedWidth = pInInfo->StretchedWidth;
    pOutInfo->StretchedHeight = pInInfo->StretchedHeight;
    pOutInfo->StretchMode = pInInfo->StretchMode;
    pOutInfo->HorDPI = pInInfo->HorDPI;
    pOutInfo->VerDPI = pInInfo->VerDPI;
    pOutInfo->FSI = NULL;
    if (pInInfo->bFSI)
    {
        pOutInfo->FSI = pWImg->FSI = (LPPVFormatSpecificInfo)malloc(sizeof(PVFormatSpecificInfo));
        if (pOutInfo->FSI)
        {
            memcpy(pOutInfo->FSI, &pInInfo->FSI, sizeof(PVFormatSpecificInfo));
        }
    }
    pOutInfo->Compression = pInInfo->Compression;
    pOutInfo->TotalBitDepth = pInInfo->TotalBitDepth;
    pOutInfo->CommentSize = 0;
    pOutInfo->Comment = NULL;
    if (pInInfo->CommentSize)
    {
        pOutInfo->Comment = pWImg->Comment = (char*)malloc(pInInfo->CommentSize);
        if (pOutInfo->Comment)
        {
            pOutInfo->CommentSize = pInInfo->CommentSize;
            memcpy(pOutInfo->Comment, pInInfo->Comment, pInInfo->CommentSize);
        }
    }
    return true;
}

// ========================= PVMessageWithProgress =========================
PVMessageWithProgress::PVMessageWithProgress(ePVMSG type, size_t dataSize, LPPVHandle pvHandle) : PVMessage(type, dataSize, pvHandle)
{
    LPPVMessageWithProgressHeader pHdr = (LPPVMessageWithProgressHeader)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->State = PVState_Started;
    pHdr->ProgressValue = 0;
}

bool PVMessageWithProgress::Exec(TProgressProc Progress, void* AppSpecific)
{
    if (!Progress)
    {
        // No progress function specified -> process quickly
        return PVMessage::Exec();
    }
    TCHAR eventName[32];
    HANDLE hEvt;

    _sntprintf(eventName, SizeOf(eventName), _T("%s_ev%x"), PVWrapper.MutexName, iID);
    hEvt = CreateEvent(NULL, FALSE, FALSE, eventName);
    if (!hEvt)
    {
        return false;
    }
    PostThreadMessage(PVWrapper.pi.dwThreadId, WM_USER, pData->cbSize, iID);
    HANDLE handles[2] = {hEvt, PVWrapper.pi.hProcess};
    LPPVMessageWithProgressHeader pHdr = (LPPVMessageWithProgressHeader)pData;
    DWORD ret = -1;
    // Receive progress messages as long as State is not PVState_Finished
    for (;;)
    {
        ret = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (ret == 0)
        {
            if (pHdr->State == PVState_Progressing)
            {
                // Forward progress update and check for cancellatiom
                if (Progress(pHdr->ProgressValue, AppSpecific))
                {
                    LONG state = InterlockedExchange(&pHdr->State, PVState_Cancelled);
                    if (state == PVState_Finished)
                    {
                        // Envelope finished in the meantime -> restore the Finished state to avoid infinite loop
                        pHdr->State = PVState_Finished;
                    }
                }
                continue;
            }
            if (pHdr->State == PVState_Cancelled)
            {
                continue;
            }
        }
        break;
    }
    CloseHandle(hEvt);
    if (ret == 1)
    {
        // The remote process died
        return false;
    }
    return ret == 0;
}

// ========================= PVMessage_GetErrorText =========================
PVMessage_GetErrorText::PVMessage_GetErrorText(DWORD ErrorCode)
    : PVMessage(PVMSG_GetErrorText, sizeof(PVMessageGetErrorText))
{
    LPPVMessageGetErrorText pHdr = (LPPVMessageGetErrorText)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->ErrorCode = ErrorCode;
}

const char* PVMessage_GetErrorText::GetErrorText()
{
    LPPVMessageGetErrorText pHdr = (LPPVMessageGetErrorText)pData;

    if (!pHdr)
    {
        return "";
    }
    return pHdr->ErrorText;
}

// ========================= PVMessage_OpenImageEx =========================
PVMessage_OpenImageEx::PVMessage_OpenImageEx(LPPVOpenImageExInfo pOpenExInfo)
    : PVMessage()
{
    LPPVMessageOpenImageEx pHdr;

    pWImg = new PVWrapperImageHandle(NULL);
    if (!pWImg)
    {
        return;
    }

    Init(PVMSG_OpenImageEx, sizeof(PVMessageOpenImageEx), pWImg);
    pHdr = (LPPVMessageOpenImageEx)pData;
    if (!pHdr)
    {
        return;
    }

    pHdr->Flags = pOpenExInfo->Flags;
    if (pOpenExInfo->Flags & PVOF_ATTACH_TO_HANDLE)
    {
        pWImg->CreateSharedMemoryForHBitmap((HBITMAP)pOpenExInfo->Handle, pHdr->FileName, sizeof(pHdr->FileName), &pHdr->DataSize);
    }
    else if (pOpenExInfo->Flags & PVOF_USERDEFINED_INPUT)
    {
        psReadMemFuncData rmfd = (psReadMemFuncData)pOpenExInfo->Handle;
        _ASSERTE(rmfd && !rmfd->Pos);
        pHdr->DataSize = rmfd->Size;
        pWImg->CreateSharedMemoryForMemoryBlock((LPBYTE)rmfd->Buffer, rmfd->Size, pHdr->FileName, sizeof(pHdr->FileName));
    }
    else
    {
        strcpy(pHdr->FileName, pOpenExInfo->FileName);
    }
}

bool PVMessage_OpenImageEx::IsInited()
{
    // We do not support custom Read and Seek functions for now
    LPPVMessageOpenImageEx pHdr = (LPPVMessageOpenImageEx)pData;

    if (!pData || (!*pHdr->FileName && !(pHdr->Flags & PVOF_ATTACH_TO_HANDLE)))
    {
        return false;
    }
    return PVMessage::IsInited();
}

bool PVMessage_OpenImageEx::Exec(LPPVImageInfo pImgInfo)
{
    if (!PVMessage::Exec())
    {
        return false;
    }
    if (GetResultCode() == PVC_OK)
    {
        pWImg->hPVHandle = (((LPPVMessageHeader)pData)->PVHandle);
        return UnmarshalImageInfo(&((LPPVMessageOpenImageEx)pData)->ImageInfo, pImgInfo);
    }
    return true;
}

// ========================= PVMessage_GetImageInfo =========================
PVMessage_GetImageInfo::PVMessage_GetImageInfo(LPPVHandle pvHandle, int ImageIndex)
    : PVMessage(PVMSG_GetImageInfo, sizeof(PVMessageGetImageInfo), pvHandle)
{
    LPPVMessageGetImageInfo pHdr = (LPPVMessageGetImageInfo)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->ImageIndex = ImageIndex;
}

bool PVMessage_GetImageInfo::Exec(LPPVImageInfo pImgInfo)
{
    if (!PVMessage::Exec())
    {
        return false;
    }
    return UnmarshalImageInfo(&((LPPVMessageGetImageInfo)pData)->ImageInfo, pImgInfo);
}

// ========================= PVMessage_ReadImage =========================
PVMessage_ReadImage::PVMessage_ReadImage(LPPVHandle pvHandle, int ImageIndex, bool bProgress)
    : PVMessageWithProgress(bProgress ? PVMSG_ReadImage : PVMSG_ReadImageWithoutProgress, sizeof(PVMessageReadImage), pvHandle)
{
    LPPVMessageReadImage pHdr = (LPPVMessageReadImage)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->ImageIndex = ImageIndex;
}

bool PVMessage_ReadImage::Exec(HDC PaintDC, RECT* pDRect, TProgressProc Progress, void* AppSpecific)
{
    // FIXME: Paint during loading
    return PVMessageWithProgress::Exec(Progress, AppSpecific);
}

// ========================= PVMessage_CloseImage =========================
PVMessage_CloseImage::PVMessage_CloseImage(LPPVHandle pvHandle)
    : PVMessage(PVMSG_CloseImage, sizeof(PVMessage), pvHandle)
{
}

PVMessage_CloseImage::~PVMessage_CloseImage()
{
    delete pWImg;
}

// ========================= PVMessage_DrawImage =========================
PVMessage_DrawImage::PVMessage_DrawImage(LPPVHandle pvHandle, HDC PaintDC, int X, int Y, LPRECT pDrawRect)
    : PVMessage(PVMSG_DrawImage, sizeof(PVMessage), pvHandle)
{
    LPPVMessageDrawImage pHdr = (LPPVMessageDrawImage)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->X = X;
    pHdr->Y = Y;
    pHdr->DrawRect = *pDrawRect;
    if (PaintDC)
    {
        pHdr->BitsPerPixel = GetDeviceCaps(PaintDC, BITSPIXEL);
        if (pHdr->BitsPerPixel < 15)
        {
            // 8-bit displays no longer supported...
            pHdr->BitsPerPixel = 24;
        }
    }
    else
    {
        pHdr->BitsPerPixel = 24;
    }
}

bool PVMessage_DrawImage::Exec(HDC PaintDC)
{
    if (!PVMessage::Exec())
    {
        return false;
    }
    if (GetResultCode() == PVC_OK)
    {
        LPPVMessageDrawImage pHdr = (LPPVMessageDrawImage)pData;
        DWORD paintWidth = pHdr->DrawRect.right - pHdr->DrawRect.left;
        DWORD paintHeight = pHdr->DrawRect.bottom - pHdr->DrawRect.top;
        DWORD frameSize = (((paintWidth * pHdr->BitsPerPixel + 31) >> 3) & ~3) * paintHeight;
        HANDLE hFMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, frameSize, pHdr->SharedMemoryName);

        // The envelope no longer needs to keep the shared memory object -> free it
        PVMessage_CloseHandle(pHdr->InternalFileMapHandle).Exec();

        if (!hFMap)
        {
            pHdr->ResultCode = PVC_OOM;
            return false;
        }

        BITMAPINFO bi;
        LPVOID pvBits = NULL;
        HBITMAP hDIB;

        memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = paintWidth;
        bi.bmiHeader.biHeight = paintHeight;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = (WORD)pHdr->BitsPerPixel;

        hDIB = CreateDIBSection(PaintDC, &bi, DIB_RGB_COLORS, &pvBits, hFMap, 0);
        if (hDIB)
        {
            HDC hMemDC = CreateCompatibleDC(PaintDC);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hDIB);

            BitBlt(PaintDC, pHdr->DrawRect.left, pHdr->DrawRect.top,
                   paintWidth, paintHeight, hMemDC, 0, 0, SRCCOPY);
            SelectObject(hMemDC, hOldBmp);
            DeleteDC(hMemDC);
            DeleteObject(hDIB);
        }
        CloseHandle(hFMap);
    }
    return true;
}

// ========================= PVMessage_SaveImage =========================
PVMessage_SaveImage::PVMessage_SaveImage(LPPVHandle pvHandle, const char* FileName, LPPVSaveImageInfo pSii, int ImageIndex, bool bProgress)
    : PVMessageWithProgress(bProgress ? PVMSG_SaveImage : PVMSG_SaveImageWithoutProgress, sizeof(PVMessageSaveImage), pvHandle)
{
    LPPVMessageSaveImage pHdr = (LPPVMessageSaveImage)pData;

    if (!pHdr)
    {
        return;
    }

    pHdr->cbSize = pSii->cbSize;
    pHdr->Format = pSii->Format;
    pHdr->Compression = pSii->Compression;
    pHdr->Colors = pSii->Colors;
    pHdr->ColorModel = pSii->ColorModel;
    pHdr->Flags = pSii->Flags;
    pHdr->Width = pSii->Width;
    pHdr->Height = pSii->Height;
    pHdr->HorDPI = pSii->HorDPI;
    pHdr->VerDPI = pSii->VerDPI;
    _ASSERTE(sizeof(pHdr->FormatSpecificInfo) == sizeof(pSii->Misc));
    memcpy(pHdr->FormatSpecificInfo, &pSii->Misc, sizeof(pSii->Misc));
    pHdr->CropLeft = pSii->CropLeft;
    pHdr->CropTop = pSii->CropTop;
    pHdr->CropWidth = pSii->CropWidth;
    pHdr->CropHeight = pSii->CropHeight;
    _ASSERTE(sizeof(pHdr->Transp) == sizeof(pSii->Transp));
    memcpy(&pHdr->Transp, &pSii->Transp, sizeof(pSii->Transp));
    pHdr->ImageIndex = ImageIndex;
    strcpy(pHdr->FileName, FileName ? FileName : "");
    _ASSERTE(pHdr->CommentSize <= sizeof(pHdr->Comment));
    pHdr->CommentSize = min(pSii->CommentSize, sizeof(pHdr->Comment));
    memcpy(pHdr->Comment, pSii->Comment, pHdr->CommentSize);
}

// ========================= PVMessage_LoadFromClipboard =========================
PVMessage_LoadFromClipboard::PVMessage_LoadFromClipboard()
    : PVMessage(PVMSG_LoadFromClipboard, sizeof(PVMessageLoadFromClipboard))
{
    pWImg = new PVWrapperImageHandle(NULL);
}

bool PVMessage_LoadFromClipboard::Exec(LPPVImageInfo pImgInfo)
{
    if (!PVMessage::Exec())
    {
        return false;
    }
    pWImg->hPVHandle = (((LPPVMessageHeader)pData)->PVHandle);
    return UnmarshalImageInfo(&((LPPVMessageLoadFromClipboard)pData)->ImageInfo, pImgInfo);
}

// ========================= PVMessage_ReadImageSequence =========================
PVMessage_ReadImageSequence::PVMessage_ReadImageSequence(LPPVHandle pvHandle)
    : PVMessage(PVMSG_ReadImageSequence, sizeof(PVMessageReadImageSequence), pvHandle)
{
}

bool PVMessage_ReadImageSequence::Exec()
{
    if (!PVMessage::Exec())
    {
        return false;
    }
    LPPVMessageReadImageSequence pHdr = (LPPVMessageReadImageSequence)pData;
    if (PVC_OK == GetResultCode())
    {
        pWImg->NumberOfFrames = pHdr->NumberOfFrames;
    }
    return true;
}

// ========================= PVMessage_SetBkHandle =========================
PVMessage_SetBkHandle::PVMessage_SetBkHandle(LPPVHandle pvHandle, COLORREF bkColor)
    : PVMessage(PVMSG_SetBkHandle, sizeof(PVMessageSetBkHandle), pvHandle)
{
    LPPVMessageSetBkHandle pHdr = (LPPVMessageSetBkHandle)pData;

    if (!pHdr)
    {
        return;
    }

    pHdr->BackgroundColor = bkColor;
}

// ========================= PVMessage_GetDLLVersion =========================
PVMessage_GetDLLVersion::PVMessage_GetDLLVersion()
    : PVMessage(PVMSG_GetDLLVersion, sizeof(PVMessageGetDLLVersion))
{
}

bool PVMessage_GetDLLVersion::Exec(DWORD& Version)
{
    if (!PVMessage::Exec())
    {
        return false;
    }
    Version = ((LPPVMessageGetDLLVersion)pData)->DLLVersion;

    return true;
}

// ========================= PVMessage_ChangeImage =========================
PVMessage_SetStretchParameters::PVMessage_SetStretchParameters(LPPVHandle pvHandle, DWORD Width, DWORD Height, DWORD Mode)
    : PVMessage(PVMSG_SetStretchParameters, sizeof(PVMessageSetStretchParameters), pvHandle)
{
    LPPVMessageSetStretchParameters pHdr = (LPPVMessageSetStretchParameters)pData;

    if (!pHdr)
    {
        return;
    }

    pHdr->Width = Width;
    pHdr->Height = Height;
    pHdr->Mode = Mode;
}

// ========================= PVMessage_ChangeImage =========================
PVMessage_ChangeImage::PVMessage_ChangeImage(LPPVHandle pvHandle, DWORD Flags)
    : PVMessage(PVMSG_ChangeImage, sizeof(PVMessageChangeImage), pvHandle)
{
    LPPVMessageChangeImage pHdr = (LPPVMessageChangeImage)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->Flags = Flags;
}

// ========================= PVMessage_IsOutCombSupported =========================
PVMessage_IsOutCombSupported::PVMessage_IsOutCombSupported(int Fmt, int Compr, int Colors, int ColorModel)
    : PVMessage(PVMSG_IsOutCombSupported, sizeof(PVMessageIsOutCombSupported))
{
    LPPVMessageIsOutCombSupported pHdr = (LPPVMessageIsOutCombSupported)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->Fmt = Fmt;
    pHdr->Compr = Compr;
    pHdr->Colors = Colors;
    pHdr->ColorModel = ColorModel;
}

// ========================= PVMessage_CropImage =========================
PVMessage_CropImage::PVMessage_CropImage(LPPVHandle pvHandle, int Left, int Top, int Width, int Height)
    : PVMessage(PVMSG_CropImage, sizeof(PVMessageCropImage), pvHandle)
{
    LPPVMessageCropImage pHdr = (LPPVMessageCropImage)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->Left = Left;
    pHdr->Top = Top;
    pHdr->Width = Width;
    pHdr->Height = Height;
}

// ========================= PVMessage_GetRGBAtCursor =========================
PVMessage_GetRGBAtCursor::PVMessage_GetRGBAtCursor(LPPVHandle pvHandle, DWORD Colors, int x, int y)
    : PVMessage(PVMSG_GetRGBAtCursor, sizeof(PVMessageGetRGBAtCursor), pvHandle)
{
    LPPVMessageGetRGBAtCursor pHdr = (LPPVMessageGetRGBAtCursor)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->Colors = Colors;
    pHdr->X = x;
    pHdr->Y = y;
}

RGBQUAD* PVMessage_GetRGBAtCursor::GetRGB()
{
    return &((LPPVMessageGetRGBAtCursor)pData)->RGB;
}

int PVMessage_GetRGBAtCursor::GetIndex()
{
    return ((LPPVMessageGetRGBAtCursor)pData)->Index;
}

// ========================= PVMessage_CalculateHistogram =========================
PVMessage_CalculateHistogram::PVMessage_CalculateHistogram(LPPVHandle pvHandle, const PVImageInfo* pvii)
    : PVMessage(PVMSG_CalculateHistogram, sizeof(PVMessageCalculateHistogram), pvHandle)
{
    LPPVMessageCalculateHistogram pHdr = (LPPVMessageCalculateHistogram)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->Colors = pvii->Colors;
    pHdr->Width = pvii->Width;
    pHdr->Height = pvii->Height;
    pHdr->BytesPerLine = pvii->BytesPerLine;
}

void PVMessage_CalculateHistogram::GetResults(LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb)
{
    LPPVMessageCalculateHistogram pHdr = (LPPVMessageCalculateHistogram)pData;

    memcpy(luminosity, pHdr->luminosity, sizeof(pHdr->luminosity));
    memcpy(red, pHdr->red, sizeof(pHdr->red));
    memcpy(green, pHdr->green, sizeof(pHdr->green));
    memcpy(blue, pHdr->blue, sizeof(pHdr->blue));
    memcpy(rgb, pHdr->rgb, sizeof(pHdr->rgb));
}

// ========================= PVMessage_CreateThumbnail =========================
PVMessage_CreateThumbnail::PVMessage_CreateThumbnail(LPPVHandle pvHandle, LPPVSaveImageInfo pSii,
                                                     int ImageIndex, DWORD imgWidth, DWORD imgHeight, DWORD thumbWidth, DWORD thumbHeight)
    : PVMessageWithProgress(PVMSG_CreateThumbnail, sizeof(PVMessageCreateThumbnail) + thumbWidth * thumbHeight * 4, pvHandle)
{
    LPPVMessageCreateThumbnail pHdr = (LPPVMessageCreateThumbnail)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->Flags = pSii->Flags;
    pHdr->ImageWidth = imgWidth;
    pHdr->ImageHeight = imgHeight;
    pHdr->ThumbWidth = thumbWidth;
    pHdr->ThumbHeight = thumbHeight;
    pHdr->ImageIndex = ImageIndex;
}

LPBYTE PVMessage_CreateThumbnail::GetPixelData()
{
    return ((LPPVMessageCreateThumbnail)pData)->Buffer;
}

// ========================= PVMessage_SimplifyImageSequence =========================
PVMessage_SimplifyImageSequence::PVMessage_SimplifyImageSequence(LPPVHandle pvHandle, DWORD ScreenWidth, DWORD ScreenHeight, const COLORREF& bgColor)
    : PVMessage(PVMSG_SimplifyImageSequence, sizeof(PVMessageSimplifyImageSequence) + sizeof(DWORD) * ((LPPVWrapperImageHandle)pvHandle)->NumberOfFrames, pvHandle)
{
    LPPVMessageSimplifyImageSequence pHdr = (LPPVMessageSimplifyImageSequence)pData;

    if (!pHdr)
    {
        return;
    }
    pHdr->ScreenWidth = ScreenWidth;
    pHdr->ScreenHeight = ScreenHeight;
    pHdr->BackgroundColor = bgColor;
}

bool PVMessage_SimplifyImageSequence::Exec()
{
    if (!PVMessage::Exec())
    {
        return false;
    }
    if (GetResultCode() == PVC_OK)
    {
        LPPVMessageSimplifyImageSequence pHdr = (LPPVMessageSimplifyImageSequence)pData;
        DWORD frameSize = (((pHdr->ScreenWidth * pHdr->BitsPerPixel + 31) >> 3) & ~3) * pHdr->ScreenHeight;

        pWImg->hImageSequenceFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, pWImg->NumberOfFrames * frameSize, pHdr->SharedMemoryName);
        // The envelope no longer needs to keep the shared memory object -> free it
        PVMessage_CloseHandle(pHdr->InternalFileMapHandle).Exec();

        if (!pWImg->hImageSequenceFileMap)
        {
            return false;
        }

        LPPVImageSequence* pCurFrame = &pWImg->PVImageSequence;
        BITMAPINFO bi;
        LPVOID pvBits = NULL;

        memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = pHdr->ScreenWidth;
        bi.bmiHeader.biHeight = pHdr->ScreenHeight;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = (WORD)pHdr->BitsPerPixel;

        for (DWORD nFrame = 0; nFrame < pWImg->NumberOfFrames; nFrame++)
        {
            LPPVImageSequence pSeq = (LPPVImageSequence)calloc(1, sizeof(PVImageSequence));
            if (pSeq)
            {
                pSeq->Rect.right = pHdr->ScreenWidth;
                pSeq->Rect.bottom = pHdr->ScreenHeight;
                pSeq->Delay = pHdr->Delay[nFrame];
                pSeq->ImgHandle = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits,
                                                   pWImg->hImageSequenceFileMap, nFrame * frameSize);
                *pCurFrame = pSeq;
                pCurFrame = &(*pCurFrame)->pNext;
            }
        }
    }
    return true;
}

LPPVImageSequence PVMessage_SimplifyImageSequence::GetImageSequence()
{
    if (pWImg)
    {
        return pWImg->PVImageSequence;
    }
    return NULL;
}

// ========================= PVMessage_CloseHandle =========================
PVMessage_CloseHandle::PVMessage_CloseHandle(DWORD Handle) : PVMessage()
{
    HandleToBeClosed = Handle;
}

bool PVMessage_CloseHandle::Exec()
{
    PostThreadMessage(PVWrapper.pi.dwThreadId, WM_USER + 1, PVMSG_CloseHandle, HandleToBeClosed);
    return true;
}

// ========================= PVWrapperImageHandle =========================
PVWrapperImageHandle::PVWrapperImageHandle(DWORD Img)
    : hEnvelopeProcess(PVWrapper.pi.hProcess), hPVHandle(Img), hFileMap(NULL), Comment(NULL), FSI(NULL),
      PVImageSequence(NULL), hImageSequenceFileMap(NULL)
{
}

PVWrapperImageHandle::~PVWrapperImageHandle()
{
    if (Comment)
        free(Comment);
    if (FSI)
        free(FSI);
    if (PVImageSequence)
    {
        while (PVImageSequence)
        {
            LPPVImageSequence pSeq = PVImageSequence;

            PVImageSequence = pSeq->pNext;
            // Transparent bitmaps are merged by the envelope and not propagated to the wrapper
            _ASSERTE(pSeq->ImgHandle && !pSeq->TransparentHandle);
            DeleteObject(pSeq->ImgHandle);
            free(pSeq);
        }
    }
    if (hImageSequenceFileMap)
    {
        CloseHandle(hImageSequenceFileMap);
    }
    FreeSharedMemory();
}

void PVWrapperImageHandle::FreeSharedMemory()
{
    if (hFileMap)
    {
        CloseHandle(hFileMap);
        hFileMap = NULL;
    }
}

bool PVWrapperImageHandle::CreateSharedMemoryForHBitmap(HBITMAP hBitmap, char* pSharedMemoryName, int maxSharedMemoryName, DWORD* pDataSize)
{
    BITMAPINFO bmpHeader;
    HDC hDC = GetDC(NULL);
#pragma pack(push, 1)
    typedef struct
    {
        WORD Magic;
        DWORD Size, Reserved;
        DWORD OfsDataInFile;
        BITMAPINFO Header;
        RGBQUAD Palette[255]; // Color 0 is already in Header
    } Bitmap, *LPBitmap;
    LPBitmap pBitmap;
#pragma pack(pop)

    *pDataSize = 2 + 4 + 4 + 4 + sizeof(bmpHeader.bmiHeader);
    bmpHeader.bmiHeader.biSize = sizeof(bmpHeader.bmiHeader);
    bmpHeader.bmiHeader.biBitCount = 0;
    if (!GetDIBits(hDC, hBitmap, 0, 0, NULL, &bmpHeader, DIB_RGB_COLORS))
    {
        ReleaseDC(NULL, hDC);
        return false;
    }
    if (bmpHeader.bmiHeader.biBitCount <= 8)
    {
        *pDataSize += 4 * (1 << bmpHeader.bmiHeader.biBitCount);
    }
    if ((bmpHeader.bmiHeader.biBitCount == 15) | (bmpHeader.bmiHeader.biBitCount == 16) || (bmpHeader.bmiHeader.biBitCount == 32))
    {
        *pDataSize += 3 * 4;
    }
    *pDataSize += bmpHeader.bmiHeader.biSizeImage;

    iFileMapID = PVWrapper.MessageID++;
    _snprintf_s(pSharedMemoryName, maxSharedMemoryName, _TRUNCATE, "%s_img%d", PVWrapper.MutexName, iFileMapID);
    hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, *pDataSize, pSharedMemoryName);
    if (!hFileMap)
    {
        ReleaseDC(NULL, hDC);
        return false;
    }
    pBitmap = (LPBitmap)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, *pDataSize);
    if (!pBitmap)
    {
        CloseHandle(hFileMap);
        hFileMap = NULL;
        ReleaseDC(NULL, hDC);
        return false;
    }

    pBitmap->Magic = 'MB';
    pBitmap->Size = pBitmap->Reserved = 0;
    pBitmap->OfsDataInFile = 2 + 4 + 4 + 4 + bmpHeader.bmiHeader.biSize;
    memcpy(&pBitmap->Header, &bmpHeader, sizeof(bmpHeader));
    if (bmpHeader.bmiHeader.biBitCount <= 8)
    {
        pBitmap->OfsDataInFile += 4 * (1 << bmpHeader.bmiHeader.biBitCount);
        // Will this work?
        GetDIBits(hDC, hBitmap, 0, 0, NULL, &pBitmap->Header, DIB_RGB_COLORS);
    }
    if ((bmpHeader.bmiHeader.biBitCount == 15) || (bmpHeader.bmiHeader.biBitCount == 16))
    {
        pBitmap->OfsDataInFile += 3 * 4;
        *(LPDWORD)(&pBitmap->Header.bmiColors[0]) /*bV4RedMask*/ = 0xf800;
        *(LPDWORD)(&pBitmap->Header.bmiColors[1]) /*bV4GreenMask*/ = 0x07e0;
        *(LPDWORD)(&pBitmap->Header.bmiColors[2]) /*bV4BlueMask*/ = 0x001f;
        HDC DC2 = CreateCompatibleDC(hDC);
        HBITMAP hBmp = (HBITMAP)SelectObject(DC2, hBitmap);
        COLORREF oldClr = GetPixel(DC2, 0, 0);
        COLORREF newClr = SetPixel(DC2, 0, 0, 0xFFF8FF);
        // NT4 15bit: newClr is FFFFFF
        // NT4 16bit: newClr is FFFBFF (?)
        // W95 15bit: newClr is FFF8FF (?)
        // W95 16bit: newClr is FFF8FF
        if (newClr == 0xFFF8FF)
        {
            // Running Win95
            newClr = SetPixel(DC2, 0, 0, 0xFFFcFF);
        }
        if (newClr == 0xFFFFFF)
        {
            // NT:  5-bit Green was converted to 8-bit green -> it's a 15bit bitmap! (proper scaling-rounding)}
            // W95: 6-bit Green was converted to 8-bit green -> it's a 15bit bitmap! (no scaling-rounding}
            *(LPDWORD)(&pBitmap->Header.bmiColors[0]) /*bV4RedMask*/ = 0x7c00;
            *(LPDWORD)(&pBitmap->Header.bmiColors[1]) /*bV4GreenMask*/ = 0x03e0;
        }
        SetPixel(DC2, 0, 0, oldClr); // restore the previous color}
        SelectObject(DC2, hBmp);
        DeleteDC(DC2);
    }
    if (bmpHeader.bmiHeader.biBitCount == 32)
    {
        pBitmap->OfsDataInFile += 3 * 4;
        *(LPDWORD)(&pBitmap->Header.bmiColors[0]) /*bV4RedMask*/ = 0xff0000;
        *(LPDWORD)(&pBitmap->Header.bmiColors[1]) /*bV4GreenMask*/ = 0xff00;
        *(LPDWORD)(&pBitmap->Header.bmiColors[2]) /*bV4BlueMask*/ = 0xff;
    }
    // Finally, get the pixels data
    GetDIBits(hDC, hBitmap, 0, bmpHeader.bmiHeader.biHeight, (LPBYTE)pBitmap + pBitmap->OfsDataInFile, &pBitmap->Header, DIB_RGB_COLORS);
    ReleaseDC(NULL, hDC);
    UnmapViewOfFile(pBitmap);
    return true;
}

bool PVWrapperImageHandle::CreateSharedMemoryForMemoryBlock(LPBYTE pBuffer, DWORD dataSize, char* pSharedMemoryName, int maxSharedMemoryName)
{
    iFileMapID = PVWrapper.MessageID++;

    _snprintf_s(pSharedMemoryName, maxSharedMemoryName, _TRUNCATE, "%s_img%d", PVWrapper.MutexName, iFileMapID);
    hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, dataSize, pSharedMemoryName);
    if (!hFileMap)
    {
        return false;
    }
    LPBYTE pData = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, dataSize);
    if (!pData)
    {
        CloseHandle(hFileMap);
        hFileMap = NULL;
        return false;
    }
    memcpy(pData, pBuffer, dataSize);
    UnmapViewOfFile(pData);
    return true;
}

#endif
