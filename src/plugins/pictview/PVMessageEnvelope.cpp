// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "PVMessage.h"
#include "PixelAccess.h"
#include "Thumbnailer.h"

class PVWrapperImageHandle
{
public:
    PVWrapperImageHandle();
    PVWrapperImageHandle(LPBYTE aBuffer, DWORD aSize);
    ~PVWrapperImageHandle();
    void FreeSharedMemory();
    void FreeBuffer();

    LPPVHandle PVHandle;
    LPPVImageSequence PVImageSequence;
    // The following 4 items are used when transfering file via shared memory
    HANDLE hFileMap;
    LPBYTE pBuffer; // Buffer with file content
    LPBYTE pCur;    // Current position inside pBuffer
    DWORD Size;     // Size of pBuffer
};

struct PVThumbnailerInfo
{
    CSalamanderThumbnailMaker* pThumbnailer;
    DWORD BytesPerLine;
    PVMessageWithProgress* pMsg;
};

typedef PVWrapperImageHandle* LPPVWrapperImageHandle;

extern TCHAR SalamanderMutex[64];
extern DWORD MainThreadID;

const char* Strings[450];
char* StringsBuf;
CSalamanderThumbnailMaker Thumbnailer;

static bool CreateSharedMemoryForBuffer(LPBYTE pInBuffer, DWORD dataSize, char* fileMapName, DWORD& outFileMap);

PVMessage::PVMessage(LPPVMessageHeader pHdr) : pData(pHdr), hFileMap(NULL), pWImg((LPPVWrapperImageHandle)(pHdr->PVHandle))
{
}

bool PVMessage::MarshalImageInfo(LPPVImageInfo pInInfo, LPPVImageInfoStruct pOutInfo)
{
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
    // Quick fix to avoid asserts because of unterminated (although correctly clipped) strings in strcpy below translated to strcpy_s
    pInInfo->Info1[sizeof(pInInfo->Info1) - 1] = 0;
    pInInfo->Info2[sizeof(pInInfo->Info2) - 1] = 0;
    pInInfo->Info3[sizeof(pInInfo->Info3) - 1] = 0;
    strcpy(pOutInfo->Info1, pInInfo->Info1);
    strcpy(pOutInfo->Info2, pInInfo->Info2);
    strcpy(pOutInfo->Info3, pInInfo->Info3);
    pOutInfo->StretchedWidth = pInInfo->StretchedWidth;
    pOutInfo->StretchedHeight = pInInfo->StretchedHeight;
    pOutInfo->StretchMode = pInInfo->StretchMode;
    pOutInfo->HorDPI = pInInfo->HorDPI;
    pOutInfo->VerDPI = pInInfo->VerDPI;
    if (pInInfo->FSI)
    {
        pOutInfo->bFSI = true;
        memcpy(&pOutInfo->FSI, pInInfo->FSI, sizeof(PVFormatSpecificInfo));
    }
    else
    {
        pOutInfo->bFSI = false;
    }
    pOutInfo->Compression = pInInfo->Compression;
    pOutInfo->TotalBitDepth = pInInfo->TotalBitDepth;
    pOutInfo->CommentSize = min(sizeof(pOutInfo->Comment), pInInfo->CommentSize);
    memcpy(pOutInfo->Comment, pInInfo->Comment, pOutInfo->CommentSize);
    return true;
}

PVMessage* PVMessage::GetPVMessage(LPPVMessageHeader pHdr, HANDLE hEvent)
{
    switch (pHdr->Type)
    {
    case PVMSG_GetDLLVersion:
        return new PVMessage_GetDLLVersion(pHdr);
    case PVMSG_CloseImage:
        return new PVMessage_CloseImage(pHdr);
    case PVMSG_DrawImage:
        return new PVMessage_DrawImage(pHdr);
    case PVMSG_GetErrorText:
        return new PVMessage_GetErrorText(pHdr);
    case PVMSG_SetBkHandle:
        return new PVMessage_SetBkHandle(pHdr);
    case PVMSG_SetStretchParameters:
        return new PVMessage_SetStretchParameters(pHdr);
    case PVMSG_ChangeImage:
        return new PVMessage_ChangeImage(pHdr);
    case PVMSG_IsOutCombSupported:
        return new PVMessage_IsOutCombSupported(pHdr);
    case PVMSG_CropImage:
        return new PVMessage_CropImage(pHdr);
    case PVMSG_OpenImageEx:
        return new PVMessage_OpenImageEx(pHdr);
    case PVMSG_GetImageInfo:
        return new PVMessage_GetImageInfo(pHdr);
    case PVMSG_ReadImage:
    case PVMSG_ReadImageWithoutProgress:
        return new PVMessage_ReadImage(pHdr, hEvent);
    case PVMSG_SaveImage:
    case PVMSG_SaveImageWithoutProgress:
        return new PVMessage_SaveImage(pHdr, hEvent);
    case PVMSG_LoadFromClipboard:
        return new PVMessage_LoadFromClipboard(pHdr);
    case PVMSG_ReadImageSequence:
        return new PVMessage_ReadImageSequence(pHdr);
    case PVMSG_GetRGBAtCursor:
        return new PVMessage_GetRGBAtCursor(pHdr);
    case PVMSG_CalculateHistogram:
        return new PVMessage_CalculateHistogram(pHdr);
    case PVMSG_CreateThumbnail:
        return new PVMessage_CreateThumbnail(pHdr, hEvent);
    case PVMSG_SimplifyImageSequence:
        return new PVMessage_SimplifyImageSequence(pHdr);
    case PVMSG_InitTexts:
        return new PVMessage_InitTexts(pHdr);
    default:
        // Unsupported/unknown message
        _ASSERTE(pHdr->Type == PVMSG_InitTexts);
        return NULL;
    }
}

bool PVMessage::HandleMessage(DWORD dataSize, DWORD id, HANDLE hEvent)
{
    TCHAR fileMapName[48];
    LPPVMessageHeader pHdr;
    HANDLE hFileMap;

    _sntprintf(fileMapName, SizeOf(fileMapName), _T("%s_%d"), SalamanderMutex, id);
    hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 0, dataSize, fileMapName);
    if (!hFileMap)
    {
        return false;
    }
    pHdr = (LPPVMessageHeader)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, dataSize);
    if (!pHdr)
    {
        CloseHandle(hFileMap);
        return false;
    }
    _ASSERTE(pHdr->cbSize == dataSize);
    PVMessage* pMessage = GetPVMessage(pHdr, hEvent);
    if (pMessage)
    {
        pMessage->HandleRequest();
        delete pMessage;
    }

    UnmapViewOfFile(pHdr);
    CloseHandle(hFileMap);

    return true;
}

bool PVMessage::HandlePostedMessage(DWORD message, DWORD data)
{
    switch (message)
    {
    case PVMSG_CloseHandle:
        CloseHandle((HANDLE)data);
        break;
    default:
        return false;
    }
    return true;
}

BOOL PVMessageWithProgress::HandleProgress(int done)
{
    LPPVMessageWithProgressHeader pHdr = (LPPVMessageWithProgressHeader)pData;
    pHdr->ProgressValue = done;
    PulseEvent(hEvent);
    return pHdr->State == PVState_Cancelled;
}

bool PVMessage_InitTexts::HandleRequest()
{
    LPPVMessageInitTexts pHdr = (LPPVMessageInitTexts)pData;

    if (!pHdr)
    {
        return false;
    }
    // This message should be sent just once....
    _ASSERTE(!StringsBuf);
    if (StringsBuf)
    {
        return false;
    }
    // Duplioate the entire buffer
    int size = pHdr->cbSize - sizeof(PVMessage) - sizeof(pHdr->TextsCount);
    StringsBuf = (char*)malloc(size);
    if (!StringsBuf)
    {
        return false;
    }
    memcpy(StringsBuf, pHdr->Texts, size);
    // Init strings pointers to point inside the buffer
    const char* str = StringsBuf;
    for (int i = 0; i < min((int)(SizeOf(Strings)), pHdr->TextsCount); i++)
    {
        Strings[i] = str;
        str += strlen(str) + 1;
    }
    pHdr->ResultCode = PVC_OK;
    return true;
}

bool PVMessage_GetErrorText::HandleRequest()
{
    LPPVMessageGetErrorText pHdr = (LPPVMessageGetErrorText)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->ResultCode = PVC_OK;
    strncpy_s(pHdr->ErrorText, PVGetErrorText(pHdr->ErrorCode), _TRUNCATE);
    return true;
}

DWORD WINAPI MySeekFunc(void* AppSpecific, LONG NewPos, int Origin)
{
    LPPVWrapperImageHandle pReadInfo = (LPPVWrapperImageHandle)AppSpecific;

    switch (Origin)
    {
    case FILE_BEGIN:
        pReadInfo->pCur = pReadInfo->pBuffer + min((DWORD)NewPos, pReadInfo->Size);
        break;
    case FILE_CURRENT:
        pReadInfo->pCur += min((DWORD)NewPos, pReadInfo->Size - (pReadInfo->pCur - pReadInfo->pBuffer));
        break;
    case FILE_END:
        pReadInfo->pCur = pReadInfo->pBuffer + pReadInfo->Size - min((DWORD)NewPos, pReadInfo->Size);
        break;
    }
    return pReadInfo->pCur - pReadInfo->pBuffer;
}

DWORD WINAPI MyMessageSeekFunc(void* AppSpecific, LONG NewPos, int Origin)
{
    return MySeekFunc(((PVMessage*)AppSpecific)->GetWImg(), NewPos, Origin);
}

DWORD WINAPI MyDummySeekFunc(void* AppSpecific, LONG NewPos, int Origin)
{
    return 0;
}

DWORD WINAPI MyReadFunc(void* AppSpecific, void* pData, DWORD Size)
{
    LPPVWrapperImageHandle pReadInfo = (LPPVWrapperImageHandle)AppSpecific;

    DWORD ret = min(Size, pReadInfo->Size - (pReadInfo->pCur - pReadInfo->pBuffer));
    memcpy(pData, pReadInfo->pCur, ret);
    pReadInfo->pCur += ret;
    return ret;
}

DWORD WINAPI MyMessageReadFunc(void* AppSpecific, void* pData, DWORD Size)
{
    return MyReadFunc(((PVMessage*)AppSpecific)->GetWImg(), pData, Size);
}

DWORD WINAPI MyWriteFunc(void* AppSpecific, void* pData, DWORD Size)
{
    LPPVWrapperImageHandle pWriteInfo = (LPPVWrapperImageHandle)AppSpecific;

    DWORD ret = min(Size, pWriteInfo->Size - (pWriteInfo->pCur - pWriteInfo->pBuffer));
    memcpy(pWriteInfo->pCur, pData, ret);
    pWriteInfo->pCur += ret;
    return ret;
}

DWORD WINAPI MyAcumulateWriteFunc(void* AppSpecific, void* pData, DWORD Size)
{
    PVMessage_SaveImage* pSIMessage = (PVMessage_SaveImage*)AppSpecific;
    LPPVWrapperImageHandle pWriteInfo = pSIMessage->GetWImg();
    DWORD pos = pWriteInfo->pCur - pWriteInfo->pBuffer;

    if (pos + Size > pWriteInfo->Size)
    {
        pWriteInfo->pBuffer = (LPBYTE)realloc(pWriteInfo->pBuffer, pos + Size);
        pWriteInfo->pCur = pWriteInfo->pBuffer + pos;
    }
    memcpy(pWriteInfo->pCur, pData, Size);
    pWriteInfo->pCur += Size;
    if ((int)pWriteInfo->Size < pWriteInfo->pCur - pWriteInfo->pBuffer)
    {
        pWriteInfo->Size = pWriteInfo->pCur - pWriteInfo->pBuffer;
    }
    return Size;
}

DWORD WINAPI MyThumbWriteFunc(void* AppSpecific, void* pData, DWORD Size)
{
    PVThumbnailerInfo* pThumbInfo = (PVThumbnailerInfo*)AppSpecific;

    if (pThumbInfo->pMsg->HandleProgress(0))
        return 0;
    return pThumbInfo->pThumbnailer->ProcessBuffer(pData, Size / pThumbInfo->BytesPerLine) * Size;
}

BOOL WINAPI ThumbProgressProc(int done, void* data)
{
    PVThumbnailerInfo* pThumbInfo = (PVThumbnailerInfo*)data;

    if (pThumbInfo)
    {
        return pThumbInfo->pMsg->HandleProgress(done);
    }
    return FALSE;
}

bool PVMessage_OpenImageEx::HandleRequest()
{
    PVOpenImageExInfo OpenImageInfo;
    PVImageInfo ImageInfo;
    LPPVMessageOpenImageEx pHdr = (LPPVMessageOpenImageEx)pData;

    if (!pHdr)
    {
        return false;
    }

    pWImg = new PVWrapperImageHandle();
    if (!pWImg)
    {
        pHdr->ResultCode = PVC_OOM;
        return true; // Message handled
    }

    memset(&OpenImageInfo, 0, sizeof(OpenImageInfo));
    OpenImageInfo.cbSize = sizeof(OpenImageInfo);
    OpenImageInfo.Flags = pHdr->Flags & ~(PVOF_ATTACH_TO_HANDLE | PVOF_USERDEFINED_INPUT);
    if (pHdr->Flags & (PVOF_ATTACH_TO_HANDLE | PVOF_USERDEFINED_INPUT))
    {
        pWImg->hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, pHdr->DataSize, pHdr->FileName);
        if (!pWImg->hFileMap)
        {
            pHdr->ResultCode = PVC_NO_FILES_FOUND;
            delete pWImg;
            pWImg = NULL;
            return true; // Message handled
        }
        pWImg->pBuffer = pWImg->pCur = (LPBYTE)MapViewOfFile(pWImg->hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, pHdr->DataSize);
        OpenImageInfo.DataSize = pWImg->Size = pHdr->DataSize;
        OpenImageInfo.Handle = pWImg;
        OpenImageInfo.ReadFunc = MyReadFunc;
        OpenImageInfo.SeekFunc = MySeekFunc;
        OpenImageInfo.Flags |= PVOF_USERDEFINED_INPUT;
    }
    else
    {
        OpenImageInfo.FileName = pHdr->FileName;
    }
    pHdr->ResultCode = PVOpenImageEx(&pWImg->PVHandle, &OpenImageInfo, &ImageInfo, sizeof(ImageInfo));
    if (pHdr->ResultCode == PVC_OK)
    {
        MarshalImageInfo(&ImageInfo, &pHdr->ImageInfo);
        pHdr->PVHandle = (DWORD)pWImg;
    }
    else
    {
        delete pWImg;
        pWImg = NULL;
    }
    return true;
}

bool PVMessage_GetImageInfo::HandleRequest()
{
    PVImageInfo ImageInfo;
    LPPVMessageGetImageInfo pHdr = (LPPVMessageGetImageInfo)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->ResultCode = PVGetImageInfo(pWImg ? pWImg->PVHandle : NULL, &ImageInfo, sizeof(ImageInfo), pHdr->ImageIndex);
    if (pHdr->ResultCode == PVC_OK)
    {
        MarshalImageInfo(&ImageInfo, &pHdr->ImageInfo);
    }
    return true;
}

PVMessage_CloseImage::~PVMessage_CloseImage()
{
    delete pWImg;
}

bool PVMessage_CloseImage::HandleRequest()
{
    LPPVMessageHeader pHdr = (LPPVMessageHeader)pData;

    if (!pHdr)
    {
        return false;
    }

    pHdr->ResultCode = PVCloseImage(pWImg ? pWImg->PVHandle : NULL);
    return true;
}

bool PVMessage_DrawImage::HandleRequest()
{
    LPPVMessageDrawImage pHdr = (LPPVMessageDrawImage)pData;

    if (!pHdr)
    {
        return false;
    }

    HDC hDC = GetDC(NULL), hMemDC;
    HBITMAP hOldBmp, hDIB;
    BITMAPINFO bi;
    LPVOID pvBits = NULL;
    DWORD hFMap;
    DWORD paintWidth = pHdr->DrawRect.right - pHdr->DrawRect.left;
    DWORD paintHeight = pHdr->DrawRect.bottom - pHdr->DrawRect.top;
    DWORD frameSize = (((paintWidth * pHdr->BitsPerPixel + 31) >> 3) & ~3) * paintHeight;

    if (!CreateSharedMemoryForBuffer(NULL, frameSize, pHdr->SharedMemoryName, hFMap))
    {
        pHdr->ResultCode = PVC_OOM;
        return true;
    }
    pHdr->InternalFileMapHandle = hFMap;

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = paintWidth;
    bi.bmiHeader.biHeight = paintHeight;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = (WORD)pHdr->BitsPerPixel;

    hDIB = CreateDIBSection(hDC, &bi, DIB_RGB_COLORS, &pvBits, (HANDLE)hFMap, 0);

    hMemDC = CreateCompatibleDC(hDC);
    hOldBmp = (HBITMAP)SelectObject(hMemDC, hDIB);
    int X = (int)pHdr->X - pHdr->DrawRect.left;
    int Y = (int)pHdr->Y - pHdr->DrawRect.top;
    RECT rect;
    rect.right = paintWidth;
    rect.bottom = paintHeight;
    rect.left = rect.top = 0;
    pHdr->ResultCode = PVDrawImage(pWImg ? pWImg->PVHandle : NULL, hMemDC, X, Y, &rect);
    SelectObject(hMemDC, hOldBmp);
    DeleteObject(hDIB);
    DeleteDC(hMemDC);

    ReleaseDC(NULL, hDC);
    return true;
}

BOOL WINAPI ProgressProc(int done, void* data)
{
    PVMessageWithProgress* pMsg = (PVMessageWithProgress*)data;

    if (pMsg)
    {
        return pMsg->HandleProgress(done);
    }
    return FALSE;
}

bool PVMessage_ReadImage::HandleRequest()
{
    LPPVMessageReadImage pHdr = (LPPVMessageReadImage)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->State = PVState_Progressing;
    pHdr->ProgressValue = 0;
    // FIXME: support drawing on-the-fly
    pHdr->ResultCode = PVReadImage2(pWImg ? pWImg->PVHandle : NULL, NULL, NULL,
                                    pHdr->Type == PVMSG_ReadImage ? ProgressProc : NULL, this, pHdr->ImageIndex);
    // Image in shared memory no longer needed -> free it
    if (pWImg)
        pWImg->FreeSharedMemory();
    InterlockedExchange(&pHdr->State, PVState_Finished);
    return true;
}

bool PVMessage_SaveImage::HandleRequest()
{
    LPPVMessageSaveImage pHdr = (LPPVMessageSaveImage)pData;
    PVSaveImageInfo SaveImageInfo;

    if (!pHdr)
    {
        return false;
    }

    pHdr->State = PVState_Progressing;
    pHdr->ProgressValue = 0;

    memset(&SaveImageInfo, 0, sizeof(SaveImageInfo));
    SaveImageInfo.cbSize = sizeof(SaveImageInfo);
    SaveImageInfo.Format = pHdr->Format;
    SaveImageInfo.Compression = pHdr->Compression;
    SaveImageInfo.Colors = pHdr->Colors;
    SaveImageInfo.ColorModel = pHdr->ColorModel;
    SaveImageInfo.Flags = pHdr->Flags;
    SaveImageInfo.Width = pHdr->Width;
    SaveImageInfo.Height = pHdr->Height;
    SaveImageInfo.HorDPI = pHdr->HorDPI;
    SaveImageInfo.VerDPI = pHdr->VerDPI;
    _ASSERTE(sizeof(pHdr->FormatSpecificInfo) == sizeof(SaveImageInfo.Misc));
    memcpy(&SaveImageInfo.Misc, pHdr->FormatSpecificInfo, sizeof(SaveImageInfo.Misc));
    SaveImageInfo.CropLeft = pHdr->CropLeft;
    SaveImageInfo.CropTop = pHdr->CropTop;
    SaveImageInfo.CropWidth = pHdr->CropWidth;
    SaveImageInfo.CropHeight = pHdr->CropHeight;
    _ASSERTE(sizeof(pHdr->Transp) == sizeof(SaveImageInfo.Transp));
    memcpy(&SaveImageInfo.Transp, &pHdr->Transp, sizeof(SaveImageInfo.Transp));
    if (SaveImageInfo.CommentSize)
    {
        SaveImageInfo.CommentSize = pHdr->CommentSize;
        SaveImageInfo.Comment = pHdr->Comment;
    }
    if (SaveImageInfo.Flags & PVSF_USERDEFINED_OUTPUT)
    {
        SaveImageInfo.WriteFunc = MyAcumulateWriteFunc;
        SaveImageInfo.SeekFunc = MyMessageSeekFunc;
        SaveImageInfo.ReadFunc = MyMessageReadFunc;
    }

    pHdr->ResultCode = PVSaveImage(pWImg ? pWImg->PVHandle : NULL, !(SaveImageInfo.Flags & PVSF_USERDEFINED_OUTPUT) ? pHdr->FileName : NULL,
                                   &SaveImageInfo, pHdr->Type == PVMSG_SaveImage ? ProgressProc : NULL, this, pHdr->ImageIndex);
    InterlockedExchange(&pHdr->State, PVState_Finished);
    if ((pHdr->ResultCode == PVC_OK) && (SaveImageInfo.Flags & PVSF_USERDEFINED_OUTPUT))
    {
        // When the save succeeded and the are user-defined output funcs, we send the back in a shared memory
        pHdr->OutFileMapSize = pWImg->Size;
        bool ret = CreateSharedMemoryForBuffer(pWImg->pBuffer, pWImg->Size, pHdr->FileName, pHdr->InternalFileMapHandle);
        // The shared memory will be freed by the wrapper via the CloseHandle message
        pWImg->FreeBuffer();
    }
    return true;
}

bool PVMessage_LoadFromClipboard::HandleRequest()
{
    PVImageInfo ImageInfo;
    LPPVMessageLoadFromClipboard pHdr = (LPPVMessageLoadFromClipboard)pData;

    if (!pHdr)
    {
        return false;
    }
    pWImg = new PVWrapperImageHandle();
    if (!pWImg)
    {
        pHdr->ResultCode = PVC_OOM;
        return true; // Message handled
    }
    pHdr->ResultCode = PVLoadFromClipboard(&pWImg->PVHandle, &ImageInfo, sizeof(ImageInfo));
    if (pHdr->ResultCode == PVC_OK)
    {
        MarshalImageInfo(&ImageInfo, &pHdr->ImageInfo);
        pHdr->PVHandle = (DWORD)pWImg;
    }
    else
    {
        delete pWImg;
        pWImg = NULL;
    }
    return true;
}

bool PVMessage_ReadImageSequence::HandleRequest()
{
    LPPVMessageReadImageSequence pHdr = (LPPVMessageReadImageSequence)pData;

    if (!pHdr)
    {
        return false;
    }

    if (!pWImg)
        pHdr->ResultCode = PVC_INVALID_HANDLE;
    else
    {
        pHdr->ResultCode = PVReadImageSequence(pWImg->PVHandle, &pWImg->PVImageSequence);
        // Image in shared memory no longer needed -> free it
        pWImg->FreeSharedMemory();
    }
    if (PVC_OK == pHdr->ResultCode)
    {
        LPPVImageSequence pSeq = pWImg->PVImageSequence;
        pHdr->NumberOfFrames = 0;
        while (pSeq)
        {
            pSeq = pSeq->pNext;
            pHdr->NumberOfFrames++;
        }
    }
    return true;
}

bool PVMessage_SetBkHandle::HandleRequest()
{
    LPPVMessageSetBkHandle pHdr = (LPPVMessageSetBkHandle)pData;

    if (!pHdr)
    {
        return false;
    }
    // NOTE: The Salamander version of PVW32Cnv.dll expects COLORREF, not HGDIOBJ
    pHdr->ResultCode = PVSetBkHandle(pWImg ? pWImg->PVHandle : NULL, (HGDIOBJ)pHdr->BackgroundColor);
    return true;
}

bool PVMessage_GetDLLVersion::HandleRequest()
{
    LPPVMessageGetDLLVersion pHdr = (LPPVMessageGetDLLVersion)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->DLLVersion = PVGetDLLVersion();
    pHdr->ResultCode = PVC_OK;
    return true;
}

bool PVMessage_SetStretchParameters::HandleRequest()
{
    LPPVMessageSetStretchParameters pHdr = (LPPVMessageSetStretchParameters)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->ResultCode = PVSetStretchParameters(pWImg ? pWImg->PVHandle : NULL, pHdr->Width, pHdr->Height, pHdr->Mode);
    return true;
}

bool PVMessage_ChangeImage::HandleRequest()
{
    LPPVMessageChangeImage pHdr = (LPPVMessageChangeImage)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->ResultCode = PVChangeImage(pWImg ? pWImg->PVHandle : NULL, pHdr->Flags);
    return true;
}

bool PVMessage_IsOutCombSupported::HandleRequest()
{
    LPPVMessageIsOutCombSupported pHdr = (LPPVMessageIsOutCombSupported)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->ResultCode = PVIsOutCombSupported(pHdr->Fmt, pHdr->Compr, pHdr->Colors, pHdr->ColorModel);
    return true;
}

bool PVMessage_CropImage::HandleRequest()
{
    LPPVMessageCropImage pHdr = (LPPVMessageCropImage)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->ResultCode = PVCropImage(pWImg ? pWImg->PVHandle : NULL, pHdr->Left, pHdr->Top, pHdr->Width, pHdr->Height);
    return true;
}

bool PVMessage_GetRGBAtCursor::HandleRequest()
{
    LPPVMessageGetRGBAtCursor pHdr = (LPPVMessageGetRGBAtCursor)pData;

    if (!pHdr)
    {
        return false;
    }
    pHdr->ResultCode = GetRGBAtCursor(pWImg ? pWImg->PVHandle : NULL, pHdr->Colors, pHdr->X, pHdr->Y, &pHdr->RGB, &pHdr->Index) ? PVC_OK : PVC_UNSUP_FILE_TYPE;
    return true;
}

bool PVMessage_CalculateHistogram::HandleRequest()
{
    LPPVMessageCalculateHistogram pHdr = (LPPVMessageCalculateHistogram)pData;
    PVImageInfo pvii;

    if (!pHdr)
    {
        return false;
    }
    // Provide just the information needed/used, nothing more
    memset(&pvii, 0, sizeof(pvii));
    pvii.Colors = pHdr->Colors;
    pvii.Width = pHdr->Width;
    pvii.Height = pHdr->Height;
    pvii.BytesPerLine = pHdr->BytesPerLine;
    pHdr->ResultCode = CalculateHistogram(pWImg ? pWImg->PVHandle : NULL, &pvii, pHdr->luminosity, pHdr->red, pHdr->green, pHdr->blue, pHdr->rgb);
    return true;
}

bool PVMessage_CreateThumbnail::HandleRequest()
{
    LPPVMessageCreateThumbnail pHdr = (LPPVMessageCreateThumbnail)pData;
    PVSaveImageInfo SaveImageInfo;
    PVThumbnailerInfo ti;

    if (!pHdr)
    {
        return false;
    }

    pHdr->State = PVState_Progressing;
    pHdr->ProgressValue = 0;

    Thumbnailer.Clear(max(pHdr->ThumbWidth, pHdr->ThumbHeight));

    if (!Thumbnailer.SetParameters(pHdr->ImageWidth, pHdr->ImageHeight, 0))
    {
        pHdr->ResultCode = PVC_OOM;
        return true;
    }

    // Provide just the information needed/used, nothing more
    memset(&SaveImageInfo, 0, sizeof(SaveImageInfo));
    SaveImageInfo.cbSize = sizeof(SaveImageInfo);
    SaveImageInfo.Format = PVF_RAW;
    SaveImageInfo.Compression = PVCS_NO_COMPRESSION;
    SaveImageInfo.Colors = PV_COLOR_TC32;
    SaveImageInfo.ColorModel = PVCM_RGB;
    SaveImageInfo.Flags = pHdr->Flags | PVSF_USERDEFINED_OUTPUT;
    SaveImageInfo.WriteFunc = MyThumbWriteFunc;
    SaveImageInfo.SeekFunc = MyDummySeekFunc;
    ti.pThumbnailer = &Thumbnailer;
    ti.BytesPerLine = pHdr->ImageWidth * 4;
    ti.pMsg = this;
    pHdr->ResultCode = PVSaveImage(pWImg ? pWImg->PVHandle : NULL, NULL, &SaveImageInfo, ThumbProgressProc, &ti, pHdr->ImageIndex);
    if ((pHdr->ResultCode == PVC_OK) || ((pHdr->ResultCode == PVC_WRITING_ERROR) && (pHdr->State != PVState_Cancelled)))
    {
        pHdr->ResultCode = PVC_OK;
        Thumbnailer.HandleIncompleteImages();
        memcpy(pHdr->Buffer, Thumbnailer.GetThumbnailBuffer(), pHdr->ThumbWidth * pHdr->ThumbHeight * 4);
    }
    InterlockedExchange(&pHdr->State, PVState_Finished);
    return true;
}

bool PVMessage_SimplifyImageSequence::HandleRequest()
{
    LPPVMessageSimplifyImageSequence pHdr = (LPPVMessageSimplifyImageSequence)pData;
    LPPVImageSequence pSeq = pWImg->PVImageSequence;
    HDC hMemDC, hMemDC2, hTempDC = GetDC(NULL);
    HBITMAP hMemBmp, hOldBmp, hTargetBmp;
    DWORD PutType;
    RECT rct, rctFull;
    HBRUSH hBr;
    int dispMethod = PVDM_UNDEFINED;
    BITMAPINFO bi;
    LPVOID pvBits = NULL;
    int nFrames, nFrame;
    int nFrameSize = ((pHdr->ScreenWidth * 3 + 3) & ~3) * pHdr->ScreenHeight;
    DWORD hFMap;

    // Get the number of frames and create shared memory object accomodating all pixels of all frames
    nFrames = 0;
    while (pSeq)
    {
        pSeq = pSeq->pNext;
        nFrames++;
    }

    if (!CreateSharedMemoryForBuffer(NULL, nFrameSize * nFrames, pHdr->SharedMemoryName, hFMap))
    {
        pHdr->ResultCode = PVC_OOM;
        return true;
    }
    pHdr->InternalFileMapHandle = hFMap;

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = pHdr->ScreenWidth;
    bi.bmiHeader.biHeight = pHdr->ScreenHeight;
    bi.bmiHeader.biPlanes = 1;
    pHdr->BitsPerPixel = bi.bmiHeader.biBitCount = 24;

    hMemBmp = CreateCompatibleBitmap(hTempDC, pHdr->ScreenWidth, pHdr->ScreenHeight);
    hMemDC = CreateCompatibleDC(hTempDC);
    SelectObject(hMemDC, hMemBmp);

    rct.left = rct.top = 0;
    rct.right = pHdr->ScreenWidth;
    rct.bottom = pHdr->ScreenHeight;
    rctFull = rct;

    //   hBr = CreateSolidBrush(pvii.FSI->GIF.BgColor);
    hBr = CreateSolidBrush(pHdr->BackgroundColor);
    FillRect(hMemDC, &rct, hBr); // image area

    hMemDC2 = CreateCompatibleDC(hMemDC);
    pSeq = pWImg->PVImageSequence;
    nFrame = 0;
    while (pSeq)
    {
        // The GIF89a spec doesn't say anything
        // GIF Construction Set help say dispMethod should be applied AFTER
        // MSIE, NN, Irfan do apply it AFTER
        // XnView (and PictView until 2004.05.24) applies it BEFORE
        // 2009.12.08: odrazka.gif: fill only the area of the subimage
        if ((dispMethod == PVDM_BACKGROUND) || (dispMethod == PVDM_PREVIOUS) /*|| (dispMethod == PVDM_UNDEFINED)*/)
        {
            FillRect(hMemDC, &rct, hBr);
        }
        dispMethod = pSeq->DisposalMethod;
        rct = pSeq->Rect;
        rct.right -= rct.left;
        rct.bottom -= rct.top;
        if (pSeq->TransparentHandle)
        {
            hOldBmp = (HBITMAP)SelectObject(hMemDC2, pSeq->TransparentHandle);
            BitBlt(hMemDC, rct.left, rct.top, rct.right, rct.bottom, hMemDC2, 0, 0, SRCAND);
            SelectObject(hMemDC2, hOldBmp);
            PutType = SRCINVERT;
            DeleteObject(pSeq->TransparentHandle);
            pSeq->TransparentHandle = NULL;
        }
        else
        {
            PutType = SRCCOPY;
        }
        hOldBmp = (HBITMAP)SelectObject(hMemDC2, pSeq->ImgHandle);
        BitBlt(hMemDC, rct.left, rct.top, rct.right, rct.bottom, hMemDC2, 0, 0, PutType);

        //    hTargetBmp = CreateCompatibleBitmap(hTempDC, pHdr->ScreenWidth, pHdr->ScreenHeight);
        hTargetBmp = CreateDIBSection(hTempDC, &bi, DIB_RGB_COLORS, &pvBits, (HANDLE)hFMap, nFrame * nFrameSize);

        SelectObject(hMemDC2, hTargetBmp);
        BitBlt(hMemDC2, 0, 0, pHdr->ScreenWidth, pHdr->ScreenHeight, hMemDC, 0, 0, SRCCOPY);
        rct = pSeq->Rect;
        pSeq->Rect.left = pSeq->Rect.top = 0;
        pSeq->Rect.right = pHdr->ScreenWidth;
        pSeq->Rect.bottom = pHdr->ScreenHeight;
        pHdr->Delay[nFrame] = pSeq->Delay;
        /*     hTargetBmp = CreateCompatibleBitmap(dc, rct.right, rct.bottom);
    SelectObject(hMemDC2, hTargetBmp);
    BitBlt(hMemDC2, 0, 0, rct.right, rct.bottom, hMemDC, rct.left, rct.top, SRCCOPY);*/
        SelectObject(hMemDC2, hOldBmp);
        // Delete the DIB Section, we no longer need it, only the pixels in the hFMap object
        DeleteObject(hTargetBmp);

        pSeq = pSeq->pNext;
        nFrame++;
    }
    DeleteDC(hMemDC2);
    DeleteObject(hBr);
    DeleteDC(hMemDC);
    DeleteObject(hMemBmp);

    ReleaseDC(NULL, hTempDC);
    pHdr->ResultCode = PVC_OK;
    return true;
}

// ====================== PVWrapperImageHandle ======================
PVWrapperImageHandle::PVWrapperImageHandle()
{
    PVHandle = NULL;
    PVImageSequence = NULL;
    hFileMap = NULL;
    pBuffer = pCur = NULL;
    Size = 0;
}

PVWrapperImageHandle::PVWrapperImageHandle(LPBYTE aBuffer, DWORD aSize)
{
    PVHandle = NULL;
    PVImageSequence = NULL;
    hFileMap = NULL;
    pBuffer = pCur = aBuffer;
    Size = aSize;
}

PVWrapperImageHandle::~PVWrapperImageHandle()
{
    FreeSharedMemory();
    // pBuffer is not NULL when hFileMap was NULL, can occur after wrong use of PVMessage_SaveImage
    if (pBuffer)
    {
        free(pBuffer);
    }
}

void PVWrapperImageHandle::FreeSharedMemory()
{
    if (hFileMap)
    {
        UnmapViewOfFile(pBuffer);
        CloseHandle(hFileMap);
        hFileMap = NULL;
        pBuffer = pCur = NULL;
    }
}

void PVWrapperImageHandle::FreeBuffer()
{
    if (pBuffer)
    {
        free(pBuffer);
        pBuffer = pCur = NULL;
        Size = 0;
    }
}

// Creates a file mapping for the given buffer
bool CreateSharedMemoryForBuffer(LPBYTE pInBuffer, DWORD dataSize, char* fileMapName, DWORD& outFileMap)
{
    HANDLE hFileMap;
    static int id = 0;

    sprintf(fileMapName, "%d_env%d", MainThreadID, id++);
    hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                  0, dataSize, fileMapName);
    if (!hFileMap)
    {
        return false;
    }
    if (pInBuffer)
    {
        LPBYTE pBuffer = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, dataSize);

        if (!pBuffer)
        {
            CloseHandle(hFileMap);
            return false;
        }
        memcpy(pBuffer, pInBuffer, dataSize);
        UnmapViewOfFile(pBuffer);
    }
    outFileMap = (DWORD)hFileMap;

    return true;
}
