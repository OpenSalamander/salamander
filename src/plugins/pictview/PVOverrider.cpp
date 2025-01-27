// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "PVOverrider.h"
#include "pictview.h"
#include "heif.h"
#include "webp.h"
#include <assert.h>
#include <optional>
#include <variant>

namespace
{

    // original function pointers
    CPVW32DLL origPVW32DLL{};

    enum class HandleType
    {
        PictView = 0, // the original PictView handle
        Heif,
        Webp
    };

    struct StretchParams
    {
        DWORD Width;
        DWORD Height;
        DWORD Mode;
    };

    // overridden PVHandle
    struct Handle
    {
        HandleType type{HandleType::PictView};
        std::optional<COLORREF> bkColor;
        std::optional<StretchParams> stretch;
        std::variant<LPPVHandle, ImageHeif, ImageWebp> data;
    };

    PVCODE OpenImageEx(LPPVHandle* Img, LPPVOpenImageExInfo pOpenExInfo, LPPVImageInfo pImgInfo, int Size)
    {
        PVCODE result;
        assert(Img);
        assert(pOpenExInfo);
        assert(pImgInfo);

        // initialize the input handle
        *Img = nullptr;

        // create a new handle
        auto* handle = new Handle{};
        if (!handle)
            return PVC_OOM;

        // additional supported image formats can be loaded only from a file, not from a buffer
        if (pOpenExInfo->FileName && !(pOpenExInfo->Flags & PVOF_USERDEFINED_INPUT))
        {
            PVImageInfo info;

            {
                // try out to load the image as HEIF
                handle->type = HandleType::Heif;
                handle->data.emplace<ImageHeif>();
                auto& heif = std::get<ImageHeif>(handle->data);
                result = heif.Open(pOpenExInfo->FileName, info);
                if (result == PVC_OK)
                {
                    // image opened successfully
                    *Img = reinterpret_cast<LPPVHandle>(handle);
                    *pImgInfo = info;
                    return PVC_OK;
                }
            }

            {
                // try out to load the image as WebP
                handle->type = HandleType::Webp;
                handle->data.emplace<ImageWebp>();
                auto& webp = std::get<ImageWebp>(handle->data);
                result = webp.Open(pOpenExInfo->FileName, info);
                if (result == PVC_OK)
                {
                    // image opened successfully
                    *Img = reinterpret_cast<LPPVHandle>(handle);
                    *pImgInfo = info;
                    return PVC_OK;
                }
            }
        }

        // call the original function
        handle->type = HandleType::PictView;
        handle->data = LPPVHandle{nullptr};
        result = origPVW32DLL.PVOpenImageEx(&std::get<LPPVHandle>(handle->data), pOpenExInfo, pImgInfo, Size);
        if (result == PVC_OK)
        {
            // image opened successfully
            *Img = reinterpret_cast<LPPVHandle>(handle);
        }
        else
        {
            // error, delete the handle
            origPVW32DLL.PVCloseImage(std::get<LPPVHandle>(handle->data));
            delete handle;
        }

        return result;
    }

    PVCODE _ReadImage(Handle* handle, TProgressProc Progress, void* AppSpecific, int ImageIndex)
    {
        assert(handle);
        assert(handle->type != HandleType::PictView);
        // TODO: handle non primary ImageIndex
        assert(ImageIndex == 0);

        HBITMAP bmp{};
        PVCODE result;

        switch (handle->type)
        {
        case HandleType::Heif:
            result = std::get<ImageHeif>(handle->data).Read(bmp, Progress, AppSpecific);
            break;
        case HandleType::Webp:
            result = std::get<ImageWebp>(handle->data).Read(bmp, Progress, AppSpecific);
            break;
        default:
            assert(0 || "Invalid image type");
            return PVC_INVALID_HANDLE;
        }

        if (result == PVC_OK)
        {
            LPPVHandle tmpHandle{};
            // image loaded successfully, load the bitmap by original PVW32DLL
            PVOpenImageExInfo oiei{};
            oiei.cbSize = sizeof(oiei);
            oiei.Flags = PVOF_ATTACH_TO_HANDLE;
            oiei.Handle = bmp;
            PVImageInfo pvii{};
            result = origPVW32DLL.PVOpenImageEx(&tmpHandle, &oiei, &pvii, sizeof(pvii));
            if (result == PVC_OK)
            {
                // apply background color and stretching if it was set
                if (handle->bkColor.has_value())
                {
                    origPVW32DLL.PVSetBkHandle(tmpHandle, handle->bkColor.value());
                    handle->bkColor.reset();
                }
                if (handle->stretch.has_value())
                {
                    const auto& params = handle->stretch.value();
                    origPVW32DLL.PVSetStretchParameters(tmpHandle, params.Width, params.Height, params.Mode);
                    handle->stretch.reset();
                }
                // read the image content
                result = origPVW32DLL.PVReadImage2(tmpHandle, 0, NULL, Progress, AppSpecific, 0);
            }

            if (result == PVC_OK)
            {
                // success, swap the handles
                handle->type = HandleType::PictView;
                handle->data = tmpHandle;
            }
            else
            {
                origPVW32DLL.PVCloseImage(tmpHandle);
            }
        }
        if (bmp)
            DeleteObject(bmp);
        return result;
    }

    PVCODE ReadImage(LPPVHandle Img, HDC PaintDC, RECT* pDRect, TProgressProc Progress, void* AppSpecific, int ImageIndex)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
        {
            // read the image content
            auto result = _ReadImage(handle, Progress, AppSpecific, ImageIndex);
            if (result == PVC_OK)
            {
                // ensure that the image handle is converted to the PictView type
                assert(handle->type == HandleType::PictView);
                // announce reading done
                if (Progress)
                    Progress(100, AppSpecific);
                // draw the loaded image
                origPVW32DLL.PVDrawImage(std::get<LPPVHandle>(handle->data), PaintDC, pDRect->left, pDRect->top, pDRect);
            }
            return result;
        }
        // call the original function
        return origPVW32DLL.PVReadImage2(std::get<LPPVHandle>(handle->data), PaintDC, pDRect, Progress, AppSpecific, ImageIndex);
    }

    PVCODE DrawImage(LPPVHandle Img, HDC PaintDC, int X, int Y, LPRECT rect)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
        {
            // TODO: draw background during loading
            return PVC_INVALID_HANDLE;
        }
        // call the original function
        return origPVW32DLL.PVDrawImage(std::get<LPPVHandle>(handle->data), PaintDC, X, Y, rect);
    }

    PVCODE CloseImage(LPPVHandle Img)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
        {
            delete handle;
            return PVC_OK;
        }
        // call the original function
        PVCODE result = origPVW32DLL.PVCloseImage(std::get<LPPVHandle>(handle->data));
        delete handle;
        return result;
    }

    PVCODE SetBkHandle(LPPVHandle Img, COLORREF BkColor)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
        {
            // cache the background color
            handle->bkColor = BkColor;
            return PVC_OK;
        }
        // call the original function
        return origPVW32DLL.PVSetBkHandle(std::get<LPPVHandle>(handle->data), BkColor);
    }

    PVCODE GetHandles2(LPPVHandle Img, LPPVImageHandles* pHandles)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.PVGetHandles2(std::get<LPPVHandle>(handle->data), pHandles);
    }

    PVCODE GetImageInfo(LPPVHandle Img, LPPVImageInfo pImgInfo, int cbSize, int ImageIndex)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.PVGetImageInfo(std::get<LPPVHandle>(handle->data), pImgInfo, cbSize, ImageIndex);
    }

    PVCODE SetStretchParameters(LPPVHandle Img, DWORD Width, DWORD Height, DWORD Mode)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
        {
            // cache the parameters
            handle->stretch = StretchParams{Width, Height, Mode};
            return PVC_OK;
        }
        // call the original function
        return origPVW32DLL.PVSetStretchParameters(std::get<LPPVHandle>(handle->data), Width, Height, Mode);
    }

    PVCODE LoadFromClipboard(LPPVHandle* Img, LPPVImageInfo pImgInfo, int cbSize)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.PVLoadFromClipboard(Img, pImgInfo, cbSize);
    }

    PVCODE SaveImage(LPPVHandle Img, const char* OutFName, LPPVSaveImageInfo pSii, TProgressProc Progress, void* AppSpecific, int ImageIndex)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.PVSaveImage(std::get<LPPVHandle>(handle->data), OutFName, pSii, Progress, AppSpecific, ImageIndex);
    }

    PVCODE ChangeImage(LPPVHandle Img, DWORD Flags)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.PVChangeImage(std::get<LPPVHandle>(handle->data), Flags);
    }

    PVCODE ReadImageSequence(LPPVHandle Img, LPPVImageSequence* ppSeq)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.PVReadImageSequence(Img, ppSeq);
    }

    PVCODE CropImage(LPPVHandle Img, int Left, int Top, int Width, int Height)
    {
        if (!Img)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.PVCropImage(std::get<LPPVHandle>(handle->data), Left, Top, Width, Height);
    }

    bool _GetRGBAtCursor(LPPVHandle Img, DWORD Colors, int x, int y, RGBQUAD* pRGB, int* pIndex)
    {
        if (!Img)
            return false;
        auto* handle = reinterpret_cast<Handle*>(Img);
        if (handle->type != HandleType::PictView)
            return false;
        // call the original function
        return origPVW32DLL.GetRGBAtCursor(std::get<LPPVHandle>(handle->data), Colors, x, y, pRGB, pIndex);
    }

    PVCODE _CalculateHistogram(LPPVHandle PVHandle, const LPPVImageInfo pvii, LPDWORD luminosity,
                               LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb)
    {
        if (!PVHandle)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(PVHandle);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.CalculateHistogram(std::get<LPPVHandle>(handle->data), pvii, luminosity, red, green, blue, rgb);
    }

    PVCODE _CreateThumbnail(LPPVHandle hPVImage, LPPVSaveImageInfo pSii, int imageIndex, DWORD imgWidth, DWORD imgHeight,
                            int thumbWidth, int thumbHeight, CSalamanderThumbnailMakerAbstract* thumbMaker,
                            DWORD thumbFlags, TProgressProc progressProc, void* progressProcArg)
    {
        if (!hPVImage)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(hPVImage);
        if (handle->type != HandleType::PictView)
        {
            // set the thumbnail size
            thumbMaker->SetParameters(thumbWidth, thumbHeight, 0);

            if (_ReadImage(handle, nullptr, nullptr, imageIndex) != PVC_OK)
            {
                thumbMaker->SetError();
                return PVC_INVALID_HANDLE;
            }
            if (progressProc)
                progressProc(0, progressProcArg);
        }
        // call the original function
        return origPVW32DLL.CreateThumbnail(std::get<LPPVHandle>(handle->data), pSii, imageIndex, imgWidth, imgHeight, thumbWidth,
                                            thumbHeight, thumbMaker, thumbFlags, progressProc, progressProcArg);
    }

    PVCODE _SimplifyImageSequence(LPPVHandle hPVImage, HDC dc, int ScreenWidth, int ScreenHeight, LPPVImageSequence& pSeq,
                                  const COLORREF& bgColor)
    {
        if (!hPVImage)
            return PVC_INVALID_HANDLE;
        auto* handle = reinterpret_cast<Handle*>(hPVImage);
        if (handle->type != HandleType::PictView)
            return PVC_INVALID_HANDLE;
        // call the original function
        return origPVW32DLL.SimplifyImageSequence(std::get<LPPVHandle>(handle->data), dc, ScreenWidth, ScreenHeight, pSeq, bgColor);
    }

} // namespace

//////////////////////////////////////////////////////////////////////////

void InitializePvOverrider()
{
    // save the original pointers
    origPVW32DLL = PVW32DLL;

    PVW32DLL.PVOpenImageEx = OpenImageEx;
    PVW32DLL.PVReadImage2 = ReadImage;
    PVW32DLL.PVDrawImage = DrawImage;
    PVW32DLL.PVCloseImage = CloseImage;
    PVW32DLL.PVSetBkHandle = SetBkHandle;
    PVW32DLL.PVGetHandles2 = GetHandles2;
    PVW32DLL.PVGetImageInfo = GetImageInfo;
    PVW32DLL.PVSetStretchParameters = SetStretchParameters;
    PVW32DLL.PVLoadFromClipboard = LoadFromClipboard;
    PVW32DLL.PVSaveImage = SaveImage;
    PVW32DLL.PVChangeImage = ChangeImage;
    PVW32DLL.PVReadImageSequence = ReadImageSequence;
    PVW32DLL.PVCropImage = CropImage;

    PVW32DLL.GetRGBAtCursor = _GetRGBAtCursor;
    PVW32DLL.CalculateHistogram = _CalculateHistogram;
    PVW32DLL.CreateThumbnail = _CreateThumbnail;
    PVW32DLL.SimplifyImageSequence = _SimplifyImageSequence;
}

void UninitializePvOverrider()
{
    // restore the original pointers
    PVW32DLL = origPVW32DLL;
}
