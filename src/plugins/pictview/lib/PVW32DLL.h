// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*****************************************************************************
 * PVW32DLL.H - C Interface to PVW32.DLL and PVW32Cnv.DLL
 * 
 * Version 1.56
 *
 * Copyright (c) 1999-2008, Jan Patera
 *****************************************************************************/

#pragma once

#pragma pack(push, 4)

#define PV_VERSION_100 0x10000
#define PV_VERSION_101 0x10001
#define PV_VERSION_102 0x10002
#define PV_VERSION_103 0x10003
#define PV_VERSION_104 0x10004
#define PV_VERSION_105 0x10005
#define PV_VERSION_106 0x10006
#define PV_VERSION_107 0x10007
#define PV_VERSION_108 0x10008
#define PV_VERSION_110 0x1000a
#define PV_VERSION_111 0x1000b
#define PV_VERSION_112 0x1000c
#define PV_VERSION_113 0x1000d
#define PV_VERSION_114 0x1000e
#define PV_VERSION_120 0x10014
#define PV_VERSION_121 0x10015
#define PV_VERSION_122 0x10016
#define PV_VERSION_150 0x10032
#define PV_VERSION_151 0x10033
#define PV_VERSION_152 0x10034
#define PV_VERSION_153 0x10035
#define PV_VERSION_154 0x10036
#define PV_VERSION_155 0x10037
#define PV_VERSION_156 0x10038
#define PV_VERSION_MAJOR(x) ((x) >> 16)
#define PV_VERSION_MINOR(x) ((x)&0xFFFF)

#define PV_MAXIMUM_BITMAP_SIZE 0xFFFFFFFF

/* Error codes: */

#define PVC_EXPIRED ((PVCODE)-8)        /* Demo version has expired */
#define PVC_GDI_ERROR ((PVCODE)-7)      /* GDI function like SelectObject failed */
#define PVC_EXCEPTION ((PVCODE)-6)      /* Internal error */
#define PVC_CANCELED ((PVCODE)-5)       /* Read canceled - not yet used */
#define PVC_NO_MORE_IMAGES ((PVCODE)-4) /* No more images in the file */
#define PVC_OOM ((PVCODE)-3)            /* Out of memory */
#define PVC_SYNC_ERROR ((PVCODE)-2)
#define PVC_INVALID_HANDLE ((PVCODE)-1)
#define PVC_OK 0
#define PVC_UNKNOWN_COMPRESSION 6
#define PVC_OUT_OF_MEMORY 7
#define PVC_TOO_MANY_BITS_PER_PLANE 9
#define PVC_LOSSLESS_JPEG_UNSUP 17
#define PVC_TOO_MUCH_FRAGMENTED_FILE 70
#define PVC_CMYK_UNSUP 74
#define PVC_YIQ_UNSUP 75
#define PVC_UNKNOWN_COLOR_MODEL 76
#define PVC_ARITHMETIC_COMP_UNSUP 77
#define PVC_UNEXPECTED_EOF 78
#define PVC_UNEXPECTED_MARKER_IN_FILE 79
#define PVC_UNEXPECTED_MARKER_IN_DATA 80
#define PVC_TOO_MANY_HUFFMAN_TABLES 81
#define PVC_TOO_MANY_COL_COMPONENTS 82
#define PVC_NON_INTERLEAVING_UNSUP 83
#define PVC_BAD_NUM_OF_COL_COMPONENTS 84
#define PVC_BOGUS_DRI_MARKER_SIZE 85
#define PVC_HEIGHT_UNKNOWN_PRIOR_DECOMP 86
#define PVC_UNKNOWN_MARKER_FOUND 87
#define PVC_UNSUP_SUBSAMPLING 88
#define PVC_TOO_MANY_QUANT_TABLES 89
#define PVC_INVALID_DIMENSIONS 91
#define PVC_UNSUP_FILE_TYPE 94
#define PVC_COMPED_TC_IMAGES_UNSUP 96
#define PVC_UNKNOWN_FILE_STRUCT 97
#define PVC_CANNOT_OPEN_FILE 99
#define PVC_NO_IMAGES_IN_AMIPRO_DOC 100
#define PVC_NO_HUFFMAN_TABLE_DEFINED 111
#define PVC_UNSUP_CONTROL_BYTE 112
#define PVC_WRITING_ERROR 115
#define PVC_READING_ERROR 117
#define PVC_UNSUP_NUM_OF_COLORS 126
#define PVC_UNSUP_DIMENSIONS 130
#define PVC_NO_FILES_FOUND 180
#define PVC_NOT_YET_SUP_COMP 195
#define PVC_BAD_FILE_HEADER_SIZE 207
#define PVC_OVERFLOW_OF_HUFFMAN_TABLE 209
#define PVC_UNSUP_NUM_OF_COL_COMPONENTS 223
#define PVC_DIFFERENT_BPS_UNSUP 224
#define PVC_UNSUP_TYPE_OF_INTERLEAVING 232
#define PVC_TOO_BIG_DECOMP_BUF 243
#define PVC_INCORRECT_UNCOMP_BLOCK_SIZE 244
#define PVC_INTERLEAVING_LIMITED 245
#define PVC_INCORRECT_HEADER_CRC 246
#define PVC_HSI_JPEG_UNSUP 248
#define PVC_UNKNOWN_FILTER 250
#define PVC_2D_CCITT3_UNSUP 253
#define PVC_UNCOMP_CCITT3_UNSUP 254
#define PVC_BAD_SCAN_DEFINITION 280
#define PVC_UNSUP_NUM_OF_COMPONENTS_IN_SCAN 281
#define PVC_ERROR_CREATING_FILE 287
#define PVC_ASCII_FORMAT_UNSUP 297
#define PVC_BOGUS_HUFFMAN_TABLE 310
#define PVC_INCORRECT_PARAMETER 312
#define PVC_PREVIEW_NOT_PRESENT 313
#define PVC_UNSUP_COLOR_DEPTH 319
#define PVC_TOO_WIDE_FOR_GIVEN_SUBSAMPLING 325
#define PVC_VECTOR_FILES_UNSUP 326
#define PVC_AUDIO_FILES_UNSUP 332
#define PVC_IMAGE_TOO_BIG 344
#define PVC_IMAGE_TOO_WIDE 345
#define PVC_UNSUP_OUT_PARAMS 346
#define PVC_CANNOT_FIND_TRANSPARENT_COLOR 347

#define PV_MAX_INFO_LEN 30

#define PV_MAX_OUT_FORMATS 20

/* Color models */
#define PV_COLOR_HC15 65532
#define PV_COLOR_HC16 65533
#define PV_COLOR_TC24 65534
#define PV_COLOR_TC32 65535

/* File formats */
#define PVF_BMP 1
#define PVF_PCX 2
#define PVF_GIF 3
#define PVF_TGA 4
#define PVF_ICO 5
#define PVF_CUT 6
#define PVF_IMG 7
#define PVF_TIFF 8
#define PVF_RLE 9
#define PVF_PSD 10
#define PVF_RAS 12
#define PVF_PCT 13
#define PVF_LBM 14
#define PVF_JPG 15
#define PVF_MSP 16
#define PVF_ICN 17
#define PVF_JMX 18
#define PVF_HRZ 19
#define PVF_FLI 20
#define PVF_PIC 34
#define PVF_RIX 35
#define PVF_CALS 21
#define PVF_SGI 22
#define PVF_PNG 23
#define PVF_HSIJPG 24
#define PVF_PC2 25
#define PVF_MAC 26
#define PVF_CUR 27
#define PVF_CDR 28
#define PVF_X 29
#define PVF_Vivid 30
#define PVF_PNM 31
#define PVF_DCX 32
#define PVF_WPG 33
#define PVF_SAM 36
#define PVF_CEL 37
#define PVF_ZBR 38
#define PVF_UDI 39
#define PVF_CLP 40
#define PVF_IRF 41
#define PVF_OFX 42
#define PVF_PCD 43
#define PVF_PYX 44
#define PVF_BMI 45
#define PVF_ZMF 46
#define PVF_AWD 47
#define PVF_QFX 48
#define PVF_XAR 49
#define PVF_TI 50
#define PVF_SKA 51
#define PVF_RAW 52
#define PVF_WBMP 53
#define PVF_EPS 54
#define PVF_MBM 55
#define PVF_ZNO 56
#define PVF_DTX 57
#define PVF_DDS 58
#define PVF_ANI 59
#define PVF_CRW 60
#define PVF_HPI 61
#define PVF_Sapphire 62

/* Bit flags used in PVImageInfo.Flags */
#define PVFF_BOTTOMTOTOP 1 /* Informational only */
#define PVFF_SINGLEPASS 2
#define PVFF_THUMBNAIL 8
#define PVFF_THUMBNAILPRESENT 16
#define PVFF_FLIP_HOR 32 /* Image needs horizontal flip for correct display */
#define PVFF_FAST 0x01000
#define PVFF_ROTATE90 0x10000 /* Image needs 90CW deg rotation (possibly after mirror) for correct display */
#define PVFF_IMAGESEQUENCE 0x400000
#define PVFF_EXIF 0x800000
#define PVFF_IPTC 0x1000000

/* Color modes used in PVImageInfo.ColorModel */
#define PVCM_RGB 0
#define PVCM_GRAYS 1
#define PVCM_YIQ 119 /* Unsupported */
#define PVCM_YCBCR 120
#define PVCM_CMYK 121
#define PVCM_UNKNOWN 122 /* Unsupported */
#define PVCM_CIELAB 257  /* Unsupported */

/* Compression schemes - used in PVImageInfo.Compression */
#define PVCS_DEFAULT 0     /* Used for conversion only */
#define PVCS_RLE 19        /* Some variation of RunLength Encoding */
#define PVCS_COMPRESSED 39 /* Proprietary compression */
#define PVCS_PACKBITS 49
#define PVCS_NO_COMPRESSION 57 /* Raw uncompressed data */
#define PVCS_HUFFMAN 58
#define PVCS_LZW 59
#define PVCS_JPEG_LOSSLESS 67 /* Loss-less JPEG, unsupported */
#define PVCS_JPEG_HUFFMAN 123 /* Lossy JPEG */
#define PVCS_JPEG_ARITH 124   /* Lossy JPEG, unsupported */
#define PVCS_CCITT_3 203
#define PVCS_CCITT_4 204
#define PVCS_UNKNOWN 205 /* Unknown compression, unsupported */
#define PVCS_CFA 206     /* Color Filter Array, used by digital cameras, unsupported */
#define PVCS_DEFLATE 236
#define PVCS_ASCII 296 /* ASCII PNM, unsupported */

/* Image stretching  - used in PVSetStretchParameters */
#define PV_STRETCH_NO 0 /* do not stretch */

/* Transparent color for PVSetBWColors and PVSetTransparentColor */
#define PV_TRANSPARENT_COLOR -1

/* Disposal methods for image sequences, used by PVImageSequence.Flags */
#define PVDM_UNDEFINED 0  // No disposal specified
#define PVDM_UNMODIFIED 1 // Do not dispose. The graphic is to be left in place
#define PVDM_BACKGROUND 2 // Restore to background color
#define PVDM_PREVIOUS 3   // Restore to previous: what was there prior to rendering the graphic

/* Which GeoTIFF info is present - used in PVFormatSpecificInfo.GeoTIFF.Flags */
#define PVFS_GEOTIFF_TRM 1
#define PVFS_GEOTIFF_PIXELSIZE 2
#define PVFS_GEOTIFF_TIEPOINT 4

typedef HANDLE LPPVHandle;
typedef DWORD PVCODE;
typedef BOOL(WINAPI* TProgressProc)(int Done, void* AppSpecific);

/* Prototypes of user specified file input/output functions */
typedef DWORD(WINAPI* TPVSeekFunc)(void* AppSpecific, LONG NewPos, int Origin);
typedef DWORD(WINAPI* TPVReadFunc)(void* AppSpecific, void* pData, DWORD Size);
typedef DWORD(WINAPI* TPVWriteFunc)(void* AppSpecific, void* pData, DWORD Size);

typedef struct genPVFormatSpecificInfo
{
    DWORD cbSize;
    union
    {
        struct
        {
            double XOR;        /* X View Origin */
            double YOR;        /* Y View Origin */
            double ZOR;        /* Z View Origin */
            double XDL;        /* X View Extent */
            double YDL;        /* Y View Extent */
            double ZDL;        /* Z View Extent */
            double TRM[16];    /* Transformation Matrix 4x4 */
            unsigned char SLO; /* Scanline Orientation */
            double ROT;        /* Rotation Angle */
            double SKW;        /* Skew Angle */
        } IRF;
        struct
        {
            unsigned ScreenWidth;
            unsigned ScreenHeight;
            unsigned XPosition;
            unsigned YPosition;
            unsigned Delay;
            unsigned TranspIndex;
            COLORREF BgColor;        /* Background color */
            unsigned DisposalMethod; /* see PVDM_xxx flags */
        } GIF;
        struct
        {
            DWORD Flags;         /* Combination of PVFS_GEOTIFF_XXX */
            double TRM[16];      /* Transformation Matrix 4x4 */
            double PixelSize[3]; /* ModelPixelScaleTag */
            DWORD TiePointCount; /* Real number of tie point items in the file */
            double TiePoint[6];  /* First 6 items of ModelTiepointTag */
        } GeoTIFF;
        struct
        {
            DWORD cbSize;             /* Num bytes in AniHeader (36 bytes) */
            DWORD cFrames;            /* Number of unique Icons in this cursor */
            DWORD cSteps;             /* Number of Blits before the animation cycles */
            DWORD cx, cy;             /* reserved, must be zero */
            DWORD cBitCount, cPlanes; /* reserved, must be zero */
            DWORD JifRate;            /* Default Jiffies (1/60th of a second) if rate chunk not present */
            DWORD flags;              /* Animation Flag (see AF_ constants) */
        } ANI;
    };
} PVFormatSpecificInfo, *LPPVFormatSpecificInfo;

typedef struct genPVImageInfo
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
    /* Added in version 1.02: */
    DWORD StretchedWidth;
    DWORD StretchedHeight;
    DWORD StretchMode;
    /* Added in version 1.03: */
    DWORD HorDPI;
    DWORD VerDPI;
    LPPVFormatSpecificInfo FSI;
    /* Added in version 1.07: */
    DWORD Compression;
    /* Added in version 1.14: */
    DWORD CommentSize;
    char* Comment;
    /* Added in version 1.53: */
    DWORD TotalBitDepth;
} PVImageInfo, *LPPVImageInfo;

typedef struct genPVImageHandles
{
    HBITMAP TransparentHandle;
    HBITMAP TransparentBackgroundHandle;
    HPALETTE HPal;
    /* Added in version 1.02: */
    HBITMAP StretchedHandle;
    /* Added in version 1.03: */
    HBITMAP StretchedTransparentHandle;
    /* Added in version 1.08: */
    RGBQUAD* Palette; /* Non-NULL for paletted images only */
                      /* Added in version 1.50: */
    BYTE** pLines;    /* Array of pointers to image lines */
} PVImageHandles, *LPPVImageHandles;

typedef struct genPVMemoryOptions2
{
    DWORD cbSize;
    DWORD MaxZoomedDDB;
} PVMemoryOptions2, *LPPVMemoryOptions2;

typedef struct genPVSupFormat
{
    WORD Format;
    WORD Flags;
    WORD Schemes[10];  /* 2, 4, 8, Grays, 15, 16, 24, 32 bits, CMYK, reserved */
    char* pExtensions; /* NUL-separated extensions for this format; 2 NUL-chars at the end */
    void* Reserved;
} PVSupFormat, *LPPVSupFormat;

typedef struct genPVSupOutFormats
{
    DWORD NumOfItems;
    PVSupFormat Formats[PV_MAX_OUT_FORMATS];
} PVSupOutFormats, *LPPVSupOutFormats;

/* Flags for PVSupFormat.Schemes */
#define PVSOCS_NO_COMPRESSION 1
#define PVSOCS_RLE 2
#define PVSOCS_PACKBITS 4
#define PVSOCS_HUFFMAN 8
#define PVSOCS_CCITT_3 16
#define PVSOCS_CCITT_4 32
#define PVSOCS_LZW 64
#define PVSOCS_JPEG 128
#define PVSOCS_JPEG_LOSSLESS 256 /* Not supported yet */
#define PVSOCS_DEFLATE 512
#define PVSOCS_ASCII 1024
#define PVSOCS_INTERLACE 16384 /* Not supported yet */
#define PVSOCS_TILE 32768      /* Not supported yet */

/* Flags for PVSaveImageInfo.Flags */
#define PVSF_UNIFORM_PALETTE 0x0020 /* Create uniform 216-color palette for web */
#define PVSF_INVERT 0x0040          /* Invert bilevel image */
/*#define  PVSF_TRANSPARENT         0x0080   Obsolete, please use PVSaveImageInfo.Transp */
#define PVSF_FLIP_VERT 0x0100           /* Flip image vertically */
#define PVSF_FLIP_HOR 0x0200            /* Flip image horizontally */
#define PVSF_GIF89 0x0400               /* Create GIF89a instead of GIF87a */
#define PVSF_APPEND_PAGE 0x0800         /* Append page to an existing file (output format must be multipage) */
#define PVSF_DO_NOT_STRIP 0x1000        /* Do not split TIFFs into strips */
#define PVSF_PREDICT 0x2000             /* Predict TIFFs compressed with LZW */
#define PVSF_INTERLACE 0x4000           /* Create interlaced GIF */
#define PVSF_TILE 0x8000                /* Not supported yet */
#define PVSF_USERDEFINED_OUTPUT 0x10000 /* WriteFunc & SeekFunc shall be used */
#define PVSF_ROTATE90 0x20000           /* Rotate 90deg clockwise */

/* Transparency flags, used by PVSaveImageInfo.Transp.Flags */
#define PVTF_NONE 0     /* No transparent color; default */
#define PVTF_ORIGINAL 1 /* Preserve transparent color from original input file */
#define PVTF_INDEX 2    /* Palette index specified in PVSaveImageInfo.Transp.Value.Index */
#define PVTF_RGB 3      /* Color specified in PVSaveImageInfo.Transp.Value.RGB */

typedef struct genPVSaveImageInfo
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
    char* Comment;
    union
    {
        /*     struct
     {
       DWORD  TranspIndex;  // Obsolete, please use PVSaveImageInfo.Transp
     } GIF;*/
        struct
        {
            DWORD StripSize;
            DWORD JPEGQuality;
            DWORD JPEGSubSampling;
        } TIFF;
        struct
        {
            DWORD Quality;
            DWORD SubSampling;
        } JPEG;
        struct
        {                      /* Intergraph raster format */
            double XOR;        /* X View Origin */
            double YOR;        /* Y View Origin */
            double ZOR;        /* Z View Origin */
            double XDL;        /* X View Extent */
            double YDL;        /* Y View Extent */
            double ZDL;        /* Z View Extent */
            double TRM[16];    /* Transformation Matrix 4x4 */
            unsigned char SLO; /* Scanline Orientation */
            double ROT;        /* Rotation Angle */
            double SKW;        /* Skew Angle */
        } IRF;
        struct
        {
            char Reserved[256];
        } RESERVED;
    } Misc;
    TPVWriteFunc WriteFunc; /* Used if PVSF_USERDEFINED_OUTPUT set in Flags */
    TPVSeekFunc SeekFunc;   /* Used if PVSF_USERDEFINED_OUTPUT set in Flags */
                            /* Added in version 1.20: */
    TPVReadFunc ReadFunc;   /* Used if PVSF_USERDEFINED_OUTPUT+PVSF_APPEND_PAGE set in Flags */
                            /* Added in version 1.52: */
    DWORD CropLeft;         /* Cropping applied before scaling */
    DWORD CropTop;
    DWORD CropWidth;  /* Crop size */
    DWORD CropHeight; /* No cropping applied if CropWidth or CropHeight is zero */
                      /* Added in version 1.56: */
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
} PVSaveImageInfo, *LPPVSaveImageInfo;

/* Flags for PVOpenImageExInfo.Flags */
#define PVOF_ATTACH_TO_HANDLE 0x0001  /* Attach DDB or DIB handle */
#define PVOF_USERDEFINED_INPUT 0x0002 /* Use user-defined Read and Seek functions */
#define PVOF_THUMBNAIL 0x0004         /* Look for a thumbnail */

typedef struct genPVOpenImageExInfo
{
    DWORD cbSize;
    DWORD Flags;
    HANDLE Handle;
    TPVReadFunc ReadFunc;
    TPVSeekFunc SeekFunc;
    DWORD DataSize;
    const char* FileName;
} PVOpenImageExInfo, *LPPVOpenImageExInfo;

/* Flags for PVChangeImage */
#define PVCF_ROTATE90CW 1
#define PVCF_ROTATE90CCW 2

typedef struct tagPVImageSequence
{
    struct tagPVImageSequence* pNext;
    RECT Rect;
    HBITMAP TransparentHandle;
    HBITMAP ImgHandle;
    DWORD Delay;
    DWORD DisposalMethod; // see PVDM_xxx flags
} PVImageSequence, *LPPVImageSequence;

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(WINVER) || (WINVER < 0x0400)
// 16-bit Windows
#define WINUSERAPI
#undef WINAPI
#define WINAPI far pascal

    typedef DWORD LPPVHandle;
#else
typedef HANDLE LPPVHandle;
#endif

    WINUSERAPI PVCODE WINAPI PVCloseImage(LPPVHandle Img);
    WINUSERAPI PVCODE WINAPI PVDrawImage(LPPVHandle Img, HDC PaintDC, int X, int Y, LPRECT rect);
    WINUSERAPI DWORD WINAPI PVGetDLLVersion(void);
    WINUSERAPI char* WINAPI PVGetErrorText(DWORD ErrorCode);
    WINUSERAPI PVCODE WINAPI PVGetHandles2(LPPVHandle Img, LPPVImageHandles* pHandles);
    WINUSERAPI PVCODE WINAPI PVGetImageInfo(LPPVHandle Img, LPPVImageInfo pImgInfo, int cbSize, int ImageIndex);
    WINUSERAPI char* WINAPI PVGetLicenseString(void);
    WINUSERAPI PVCODE WINAPI PVGetMemoryOptions2(LPPVMemoryOptions2 pMemoryOptions, int cbSize);
    WINUSERAPI DWORD WINAPI PVIsOutCombSupported(int Fmt, int Compr, int Colors, int ColorModel);
    WINUSERAPI PVCODE WINAPI PVLoadFromClipboard(LPPVHandle* Img, LPPVImageInfo pImgInfo, int cbSize);
    WINUSERAPI PVCODE WINAPI PVOpenImage(LPPVHandle* Img, const char* FileName, LPPVImageInfo pImgInfo, int cbSize);
    WINUSERAPI PVCODE WINAPI PVOpenImageEx(LPPVHandle* Img, const LPPVOpenImageExInfo pOpenExInfo, LPPVImageInfo pImgInfo, int cbSize);
    WINUSERAPI PVCODE WINAPI PVReadImage2(LPPVHandle Img, HDC PaintDC, RECT* pDRect, TProgressProc Progress, void* AppSpecific, int ImageIndex);
    WINUSERAPI PVCODE WINAPI PVSetBkHandle(LPPVHandle Img, HGDIOBJ BkHandle);
    WINUSERAPI PVCODE WINAPI PVSetMaxDIBSize(LPPVHandle Img, DWORD Size);
    WINUSERAPI PVCODE WINAPI PVSetMemoryOptions2(LPPVMemoryOptions2 MemoryOptions);
    WINUSERAPI PVCODE WINAPI PVSetStretchParameters(LPPVHandle Img, DWORD Width, DWORD Height, DWORD Mode);
    WINUSERAPI PVCODE WINAPI PVSetTransparentColor(LPPVHandle Img, COLORREF TranspColor);
    WINUSERAPI PVCODE WINAPI PVSetBWColors(LPPVHandle Img, COLORREF Black, COLORREF White);
    WINUSERAPI PVCODE WINAPI PVSaveImage(LPPVHandle Img, const char* OutFName, const LPPVSaveImageInfo pSii, TProgressProc Progress, void* AppSpecific, int ImageIndex);
    WINUSERAPI LPPVSupOutFormats WINAPI PVGetSupOutFormats(void);
    WINUSERAPI PVCODE WINAPI PVChangeImage(LPPVHandle Img, DWORD Flags);
    WINUSERAPI PVCODE WINAPI PVReadImageSequence(LPPVHandle Img, LPPVImageSequence* ppSeq);
    WINUSERAPI PVCODE WINAPI PVCropImage(LPPVHandle Img, int Left, int Top, int Width, int Height);

#ifdef __cplusplus
};
#endif

#pragma pack(pop)
