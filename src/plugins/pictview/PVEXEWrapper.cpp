// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/* This wrapper implements stubs for the functions exported by PVW32Cnv.dll
 * and found in struct CPVW32DLL. These stubs call a hidden remote (32-bit) process
 * SalPVEnv.exe that actually makes the calls to a real PVW32Cnv.dll.
 * This remote process is called the Envelope.
 * Shared memory (File mapping) is used for the inter-process communication.
 * The name of the file mapping is based on the AS.exe ProcessID and a unique call ID.
 * The calls are signalled by a unique Event object whose name is based on the same ID's.
 * Each call uses its own shared memory and event to enable proper multi-threading.
 *
 * This wrapper is enabled by the PICTVIEW_DLL_IN_SEPARATE_PROCESS macro.
 * It supports both 32-bit and 64-bit builds of AS.
 *
 * NOTE: Work in progress.
 */
#include "precomp.h"

#ifdef PICTVIEW_DLL_IN_SEPARATE_PROCESS

#include "PVEXEWrapper.h"
#include "PVMessage.h"
#include "PixelAccess.h"
#include "Thumbnailer.h"

CPVWrapper PVWrapper;

static PVCODE WINAPI PVReadImage2Stub(LPPVHandle Img, HDC PaintDC, RECT* pDRect, TProgressProc Progress, void* AppSpecific, int ImageIndex);
static PVCODE WINAPI PVCloseImageStub(LPPVHandle Img);
static PVCODE WINAPI PVDrawImageStub(LPPVHandle Img, HDC PaintDC, int X, int Y, LPRECT rect);
static const char* WINAPI PVGetErrorTextStub(DWORD ErrorCode);
static PVCODE WINAPI PVOpenImageExStub(LPPVHandle* Img, LPPVOpenImageExInfo pOpenExInfo, LPPVImageInfo pImgInfo, int Size);
static PVCODE WINAPI PVSetBkHandleStub(LPPVHandle Img, COLORREF BkColor);
static DWORD WINAPI PVGetDLLVersionStub(void);
static PVCODE WINAPI PVSetStretchParametersStub(LPPVHandle Img, DWORD Width, DWORD Height, DWORD Mode);
static PVCODE WINAPI PVLoadFromClipboardStub(LPPVHandle* Img, LPPVImageInfo pImgInfo, int Size);
static PVCODE WINAPI PVGetImageInfoStub(LPPVHandle Img, LPPVImageInfo pImgInfo, int Size, int ImageIndex);
static PVCODE WINAPI PVSetParamStub(LPPVHandle Img);
static PVCODE WINAPI PVGetHandles2Stub(LPPVHandle Img, LPPVImageHandles* pHandles);
static PVCODE WINAPI PVSaveImageStub(LPPVHandle Img, const char* OutFName, LPPVSaveImageInfo pSii, TProgressProc Progress, void* AppSpecific, int ImageIndex);
static PVCODE WINAPI PVChangeImageStub(LPPVHandle Img, DWORD Flags);
static DWORD WINAPI PVIsOutCombSupportedStub(int Fmt, int Compr, int Colors, int ColorModel);
static PVCODE WINAPI PVReadImageSequenceStub(LPPVHandle Img, LPPVImageSequence* ppSeq);
static PVCODE WINAPI PVCropImageStub(LPPVHandle Img, int Left, int Top, int Width, int Height);
static bool GetRGBAtCursorStub(LPPVHandle Img, DWORD Colors, int x, int y, RGBQUAD* pRGB, int* pIndex);
static PVCODE CalculateHistogramStub(LPPVHandle Img, const LPPVImageInfo pvii, LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb);
static PVCODE CreateThumbnailStub(LPPVHandle Img, LPPVSaveImageInfo pSii, int imageIndex, DWORD imgWidth, DWORD imgHeight,
                                  int thumbWidth, int thumbHeight, CSalamanderThumbnailMakerAbstract* thumbMaker, DWORD thumbFlags, TProgressProc progressProc, void* progressProcArg);
static PVCODE SimplifyImageSequenceStub(LPPVHandle hPVImage, HDC dc, int ScreenWidth, int ScreenHeight, LPPVImageSequence& pSeq, const COLORREF& bgColor);

struct CPVW32DLL PVEXEProcStubs = {
    PVReadImage2Stub,
    PVCloseImageStub,
    PVDrawImageStub,
    PVGetErrorTextStub,
    PVOpenImageExStub,
    PVSetBkHandleStub,
    PVGetDLLVersionStub,
    PVSetStretchParametersStub,
    PVLoadFromClipboardStub,
    PVGetImageInfoStub,
    PVSetParamStub,
    PVGetHandles2Stub,
    PVSaveImageStub,
    PVChangeImageStub,
    PVIsOutCombSupportedStub,
    PVReadImageSequenceStub,
    PVCropImageStub,
    GetRGBAtCursorStub,
    CalculateHistogramStub,
    CreateThumbnailStub,
    SimplifyImageSequenceStub};

BOOL InitPVEXEWrapper(HWND hParentWnd, LPCTSTR pPluginFolder)
{
    PVW32DLL = PVEXEProcStubs;

    _tcscpy(PVWrapper.EnvelopePath, pPluginFolder);
    _tcscat(PVWrapper.EnvelopePath, _T("\\SalPVEnv.exe"));
    _sntprintf(PVWrapper.MutexName, SizeOf(PVWrapper.MutexName), _T("PVEXE_%08X"), GetCurrentProcessId());
    PVWrapper.hMutex = CreateMutex(NULL, TRUE, PVWrapper.MutexName);
    _ASSERTE(GetLastError() != ERROR_ALREADY_EXISTS);
    if (!PVWrapper.hMutex)
    {
        return FALSE;
    }

    // NOTE: the name of the exe in the Command line is not important. It just must be something.
    _sntprintf(PVWrapper.CommandLine, SizeOf(PVWrapper.CommandLine), _T("SalPVEnv.exe %s"), PVWrapper.MutexName);
    if (!LoadEnvelope())
    {
        CloseHandle(PVWrapper.hMutex);
        PVWrapper.hMutex = NULL;
        return FALSE;
    }

    return TRUE;
}

void ReleasePVEXEWrapper()
{
    if (PVWrapper.hMutex != NULL)
    {
        CloseHandle(PVWrapper.hMutex); // This will make the Envelope make suicide
        PVWrapper.hMutex = NULL;
    }
}

// ========================= Stubs to PVW32Cnv.dll =========================

PVCODE WINAPI PVReadImage2Stub(LPPVHandle Img, HDC PaintDC, RECT* pDRect, TProgressProc Progress, void* AppSpecific, int ImageIndex)
{
    PVMessage_ReadImage msg(Img, ImageIndex, Progress != NULL);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec(PaintDC, pDRect, Progress, AppSpecific))
    {
        PVCODE ret = msg.GetResultCode();
        if (((PVC_OK == ret) || (PVC_CANCELED == ret)) && PaintDC)
        {
            PVDrawImageStub(Img, PaintDC, pDRect->left, pDRect->top, pDRect);
        }
        return ret;
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVCloseImageStub(LPPVHandle Img)
{
    if (!Img)
    {
        return PVC_OK;
    }
    PVMessage_CloseImage msg(Img);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVDrawImageStub(LPPVHandle Img, HDC PaintDC, int X, int Y, LPRECT rect)
{
    PVMessage_DrawImage msg(Img, PaintDC, X, Y, rect);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec(PaintDC))
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

const char* WINAPI PVGetErrorTextStub(DWORD ErrorCode)
{
    // FIXME: make this thread-safe.
    // FIXME: implement providing the string from Salamander, when needed
    //  (PVW32Cnv.dll doesn't contain most strings anymore)
    // FIXME: consult the default error text with Altap
    static char errorStr[512];

    if (ErrorCode == PVC_ENVELOPE_NOT_LOADED)
        return "Could not load PictView process.";

    PVMessage_GetErrorText msg(ErrorCode);

    if (!msg.IsInited())
        return "Could not load PictView process.";

    if (msg.Exec())
    {
        strcpy(errorStr, msg.GetErrorText());
        return errorStr;
    }

    return "Could not load PictView process.";
}

PVCODE WINAPI PVOpenImageExStub(LPPVHandle* Img, LPPVOpenImageExInfo pOpenExInfo, LPPVImageInfo pImgInfo, int Size)
{
    PVMessage_OpenImageEx msg(pOpenExInfo);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec(pImgInfo))
    {
        *Img = msg.GetPVHandle();
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVSetBkHandleStub(LPPVHandle Img, COLORREF BkColor)
{
    PVMessage_SetBkHandle msg(Img, BkColor);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

DWORD WINAPI PVGetDLLVersionStub(void)
{
    PVMessage_GetDLLVersion msg;
    DWORD version;

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec(version))
    {
        return version;
    }

    return 0;
}

PVCODE WINAPI PVSetStretchParametersStub(LPPVHandle Img, DWORD Width, DWORD Height, DWORD Mode)
{
    PVMessage_SetStretchParameters msg(Img, Width, Height, Mode);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVLoadFromClipboardStub(LPPVHandle* Img, LPPVImageInfo pImgInfo, int Size)
{
    PVMessage_LoadFromClipboard msg;

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec(pImgInfo))
    {
        *Img = msg.GetPVHandle();
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVGetImageInfoStub(LPPVHandle Img, LPPVImageInfo pImgInfo, int Size, int ImageIndex)
{
    PVMessage_GetImageInfo msg(Img, ImageIndex);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec(pImgInfo))
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVSetParamStub(LPPVHandle Img)
{
    // This is not needed in this environment
    return PVC_OK;
}

PVCODE WINAPI PVGetHandles2Stub(LPPVHandle Img, LPPVImageHandles* pHandles)
{
    // This is not needed in this environment
    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVSaveImageStub(LPPVHandle Img, const char* OutFName, LPPVSaveImageInfo pSii, TProgressProc Progress, void* AppSpecific, int ImageIndex)
{
    PVMessage_SaveImage msg(Img, OutFName, pSii, ImageIndex, Progress != NULL);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec(Progress, AppSpecific))
    {
        PVCODE ret = msg.GetResultCode();

        if (ret != PVC_OK)
            return ret;

        // Send the output to user-defined output funcs, if specified
        if (pSii->Flags & PVSF_USERDEFINED_OUTPUT)
        {
            PVMessage_SaveImage::LPPVMessageSaveImage pHdr = (PVMessage_SaveImage::LPPVMessageSaveImage)msg.GetData();
            HANDLE hFileMap;
            LPBYTE pBuffer;

            hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                          0, pHdr->OutFileMapSize, pHdr->FileName);
            if (!hFileMap)
            {
                return PVC_OOM;
            }
            pBuffer = (LPBYTE)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, pHdr->OutFileMapSize);
            if (!pBuffer)
            {
                CloseHandle(hFileMap);
                return PVC_OOM;
            }
            ret = (pSii->WriteFunc(AppSpecific, pBuffer, pHdr->OutFileMapSize) == pHdr->OutFileMapSize) ? PVC_OK : PVC_WRITING_ERROR;
            UnmapViewOfFile(pBuffer);
            CloseHandle(hFileMap);
            PVMessage_CloseHandle(pHdr->InternalFileMapHandle).Exec();
        }
        return ret;
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVChangeImageStub(LPPVHandle Img, DWORD Flags)
{
    PVMessage_ChangeImage msg(Img, Flags);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

DWORD WINAPI PVIsOutCombSupportedStub(int Fmt, int Compr, int Colors, int ColorModel)
{
    PVMessage_IsOutCombSupported msg(Fmt, Compr, Colors, ColorModel);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVReadImageSequenceStub(LPPVHandle Img, LPPVImageSequence* ppSeq)
{
    PVMessage_ReadImageSequence msg(Img);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE WINAPI PVCropImageStub(LPPVHandle Img, int Left, int Top, int Width, int Height)
{
    PVMessage_CropImage msg(Img, Left, Top, Width, Height);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

bool GetRGBAtCursorStub(LPPVHandle Img, DWORD Colors, int x, int y, RGBQUAD* pRGB, int* pIndex)
{
    PVMessage_GetRGBAtCursor msg(Img, Colors, x, y);

    if (!msg.IsInited())
        return false;

    if (msg.Exec())
    {
        *pRGB = *msg.GetRGB();
        *pIndex = msg.GetIndex();
        return msg.GetResultCode() == PVC_OK;
    }

    return false;
}

PVCODE CalculateHistogramStub(LPPVHandle Img, const LPPVImageInfo pvii, LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb)
{
    PVMessage_CalculateHistogram msg(Img, pvii);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        msg.GetResults(luminosity, red, green, blue, rgb);
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE CreateThumbnailStub(LPPVHandle Img, LPPVSaveImageInfo pSii, int imageIndex, DWORD imgWidth, DWORD imgHeight,
                           int thumbWidth, int thumbHeight, CSalamanderThumbnailMakerAbstract* thumbMaker, DWORD thumbFlags,
                           TProgressProc progressProc, void* progressProcArg)
{
    CSalamanderThumbnailMaker::CalculateThumbnailSize(imgWidth, imgHeight, thumbWidth, thumbHeight, thumbWidth, thumbHeight);
    PVMessage_CreateThumbnail msg(Img, pSii, imageIndex, imgWidth, imgHeight, thumbWidth, thumbHeight);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (thumbMaker->SetParameters(thumbWidth, thumbHeight, thumbFlags))
    {
        if (msg.Exec(progressProc, progressProcArg))
        {
            if ((msg.GetResultCode() == PVC_OK) || (msg.GetResultCode() == PVC_CANCELED))
            {
                thumbMaker->ProcessBuffer(msg.GetPixelData(), thumbHeight);
            }
        }
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

PVCODE SimplifyImageSequenceStub(LPPVHandle Img, HDC dc, int ScreenWidth, int ScreenHeight, LPPVImageSequence& pSeq, const COLORREF& bgColor)
{
    PVMessage_SimplifyImageSequence msg(Img, ScreenWidth, ScreenHeight, bgColor);

    if (!msg.IsInited())
        return PVC_ENVELOPE_NOT_LOADED;

    if (msg.Exec())
    {
        if (msg.GetResultCode() == PVC_OK)
        {
            pSeq = msg.GetImageSequence();
        }
        return msg.GetResultCode();
    }

    return PVC_ENVELOPE_NOT_LOADED;
}

// ========================= LoadEnvelope =========================
bool LoadEnvelope()
{
    STARTUPINFO si;

    _ASSERTE(PVWrapper.hMutex != NULL);
    if (!PVWrapper.hMutex)
    {
        return false;
    }

    // FIXME: The rest of this func may need synchronization via a critical section
    if (PVWrapper.pi.hProcess)
    {
        // FIXME: check that the envelope process is still alive
        return true;
    }
    memset(&si, 0, sizeof(si));
    memset(&PVWrapper.pi, 0, sizeof(PVWrapper.pi));
    si.cb = sizeof(STARTUPINFO);
    if (!CreateProcess(PVWrapper.EnvelopePath, PVWrapper.CommandLine, NULL, NULL,
                       FALSE, 0, NULL, NULL, &si, &PVWrapper.pi))
    {
        return false;
    }

    WaitForInputIdle(PVWrapper.pi.hProcess, 5000);

    // Feed the localized texts from our language dll to the envelope
    PVMessage_InitTexts msg;

    if (!msg.IsInited() || !msg.Exec())
    {
        return false;
    }

    return true;
}

PVMessage_InitTexts::PVMessage_InitTexts()
{
    char *buf = NULL, *ptr = buf;
    size_t alloced = 0;
    char str[5000];

    for (int i = 0; i <= 999; i++)
    {
        int size = LoadString(HLanguage, IDS_DLL + i, str, 5000);
        if (size == 0)
            str[0] = 0; // String not found in resources
        size_t len = size + 1;
        if (ptr - buf + len > alloced)
        {
            alloced += 8192;
            char* newbuf = (char*)realloc(buf, alloced);
            if (!newbuf)
            {
                break; // What's wrong?
            }
            ptr = newbuf + (ptr - buf);
            buf = newbuf;
        }
        strcpy(ptr, str);
        ptr += len;
    }
    if (!Init(PVMSG_InitTexts, ptr - buf + sizeof(PVMessageInitTexts) - sizeof(PVMessageHeader), NULL))
    {
        free(buf);
        return;
    }

    LPPVMessageInitTexts pHdr = (LPPVMessageInitTexts)pData;
    pHdr->TextsCount = 999;
    memcpy(pHdr->Texts, buf, ptr - buf);
    free(buf);
}

// The following is needed to keep the linker happy in debug builds
bool PVMessage_InitTexts::HandleRequest()
{
    return false;
}

bool PVMessage_GetErrorText::HandleRequest()
{
    return false;
}

bool PVMessage_OpenImageEx::HandleRequest()
{
    return false;
}

bool PVMessage_GetImageInfo::HandleRequest()
{
    return false;
}

bool PVMessage_ReadImage::HandleRequest()
{
    return false;
}

bool PVMessage_CloseImage::HandleRequest()
{
    return false;
}

bool PVMessage_DrawImage::HandleRequest()
{
    return false;
}

bool PVMessage_SaveImage::HandleRequest()
{
    return false;
}

bool PVMessage_LoadFromClipboard::HandleRequest()
{
    return false;
}

bool PVMessage_ReadImageSequence::HandleRequest()
{
    return false;
}

bool PVMessage_SetBkHandle::HandleRequest()
{
    return false;
}

bool PVMessage_GetDLLVersion::HandleRequest()
{
    return false;
}

bool PVMessage_SetStretchParameters::HandleRequest()
{
    return false;
}

bool PVMessage_ChangeImage::HandleRequest()
{
    return false;
}

bool PVMessage_IsOutCombSupported::HandleRequest()
{
    return false;
}

bool PVMessage_CropImage::HandleRequest()
{
    return false;
}

bool PVMessage_GetRGBAtCursor::HandleRequest()
{
    return false;
}

bool PVMessage_CalculateHistogram::HandleRequest()
{
    return false;
}

bool PVMessage_CreateThumbnail::HandleRequest()
{
    return false;
}

bool PVMessage_SimplifyImageSequence::HandleRequest()
{
    return false;
}

bool PVMessage_CloseHandle::HandleRequest()
{
    return false;
}

#endif
