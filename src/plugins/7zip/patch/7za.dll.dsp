# Microsoft Developer Studio Project File - Name="7za.dll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=7za.dll - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "7za.dll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "7za.dll.mak" CFG="7za.dll - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "7za.dll - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "7za.dll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Gz /MT /W3 /GX /O1 /I "$(CURRENT_LOCAL)\SALAMAND\PLUGINS\7ZIP\DLL\7ZA" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /D "EXCLUDE_COM" /D "NO_REGISTRY" /D "FORMAT_7Z" /D "COMPRESS_LZMA" /D "COMPRESS_BCJ_X86" /D "COMPRESS_BCJ2" /D "COMPRESS_COPY" /D "COMPRESS_MF_PAT" /D "COMPRESS_MF_BT" /D "COMPRESS_MF_HC" /D "COMPRESS_MF_MT" /D "COMPRESS_PPMD" /D "CRYPTO_7ZAES" /D "CRYPTO_AES" /Yu"StdAfx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"Release\7za.dll" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none /debug

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Gz /MTd /W3 /Gm /GX /ZI /Od /I "$(CURRENT_LOCAL)\SALAMAND\PLUGINS\7ZIP\7ZA" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /D "EXCLUDE_COM" /D "NO_REGISTRY" /D "FORMAT_7Z" /D "COMPRESS_LZMA" /D "COMPRESS_BCJ_X86" /D "COMPRESS_BCJ2" /D "COMPRESS_COPY" /D "COMPRESS_MF_PAT" /D "COMPRESS_MF_BT" /D "COMPRESS_MF_HC" /D "COMPRESS_MF_MT" /D "COMPRESS_PPMD" /D "CRYPTO_7ZAES" /D "CRYPTO_AES" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"c:\zumpa\salamand\Debug\plugins\7zip\7za.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "7za.dll - Win32 Release"
# Name "7za.dll - Win32 Debug"
# Begin Group "Spec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Archive\7z\7z.def
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7z.ico
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\DllExports.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\resource.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\resource.rc
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\StdAfx.cpp
# ADD CPP /Yc"StdAfx.h"
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\StdAfx.h
# End Source File
# End Group
# Begin Group "Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Common\AlignedBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\AlignedBuffer.h
# End Source File
# Begin Source File

SOURCE=..\Common\CRC.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\CRC.h
# End Source File
# Begin Source File

SOURCE=..\Common\IntToString.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\IntToString.h
# End Source File
# Begin Source File

SOURCE=..\Common\String.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\String.h
# End Source File
# Begin Source File

SOURCE=..\Common\StringConvert.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\StringConvert.h
# End Source File
# Begin Source File

SOURCE=..\Common\StringToInt.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\StringToInt.h
# End Source File
# Begin Source File

SOURCE=..\Common\Vector.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\Vector.h
# End Source File
# Begin Source File

SOURCE=..\Common\Wildcard.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\Wildcard.h
# End Source File
# End Group
# Begin Group "Windows"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Windows\FileDir.cpp
# End Source File
# Begin Source File

SOURCE=..\Windows\FileDir.h
# End Source File
# Begin Source File

SOURCE=..\Windows\FileFind.cpp
# End Source File
# Begin Source File

SOURCE=..\Windows\FileFind.h
# End Source File
# Begin Source File

SOURCE=..\Windows\FileIO.cpp
# End Source File
# Begin Source File

SOURCE=..\Windows\FileIO.h
# End Source File
# Begin Source File

SOURCE=..\Windows\PropVariant.cpp
# End Source File
# Begin Source File

SOURCE=..\Windows\PropVariant.h
# End Source File
# Begin Source File

SOURCE=..\Windows\Synchronization.cpp
# End Source File
# Begin Source File

SOURCE=..\Windows\Synchronization.h
# End Source File
# Begin Source File

SOURCE=..\Windows\System.cpp
# End Source File
# Begin Source File

SOURCE=..\Windows\System.h
# End Source File
# Begin Source File

SOURCE=..\Windows\Thread.h
# End Source File
# End Group
# Begin Group "Archive common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Archive\Common\CoderMixer2.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\CoderMixer2.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\CrossThreadProgress.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\CrossThreadProgress.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\InStreamWithCRC.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\InStreamWithCRC.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\ItemNameUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\ItemNameUtils.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\OutStreamWithCRC.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\Common\OutStreamWithCRC.h
# End Source File
# End Group
# Begin Group "Compress"

# PROP Default_Filter ""
# Begin Group "LZ"

# PROP Default_Filter ""
# Begin Group "MT"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Compress\LZ\MT\MT.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZ\MT\MT.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\7zip\Compress\LZ\LZInWindow.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZ\LZInWindow.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZ\LZOutWindow.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZ\LZOutWindow.h
# End Source File
# End Group
# Begin Group "PPMD"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDContext.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDDecode.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDDecoder.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDDecoder.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDEncode.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDEncoder.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDEncoder.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDSubAlloc.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\PPMD\PPMDType.h
# End Source File
# End Group
# Begin Group "Branch"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Compress\Branch\x86.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\Branch\x86.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\Branch\x86_2.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\Branch\x86_2.h
# End Source File
# End Group
# Begin Group "LZMA"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMA.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMADecoder.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMADecoder.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMAEncoder.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMAEncoder.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMALen.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMALen.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMALiteral.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\LZMA\LZMALiteral.h
# End Source File
# End Group
# Begin Group "Copy"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Compress\Copy\CopyCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\Copy\CopyCoder.h
# End Source File
# End Group
# Begin Group "RangeCoder"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Compress\RangeCoder\RangeCoder.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\RangeCoder\RangeCoderBit.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\RangeCoder\RangeCoderBit.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\RangeCoder\RangeCoderBitTree.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Compress\RangeCoder\RangeCoderOpt.h
# End Source File
# End Group
# End Group
# Begin Group "Crypto"

# PROP Default_Filter ""
# Begin Group "AES"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Crypto\AES\aes.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\AES_CBC.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\aescpp.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\aescrypt.c

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\aeskey.c

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\aesopt.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\aestab.c

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\MyAES.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\AES\MyAES.h
# End Source File
# End Group
# Begin Group "7zAES"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Crypto\7zAES\7zAES.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\7zAES\7zAES.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\7zAES\MySHA256.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\7zAES\SHA256.cpp

!IF  "$(CFG)" == "7za.dll - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7za.dll - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\7zip\Crypto\7zAES\SHA256.h
# End Source File
# End Group
# End Group
# Begin Group "7z"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zCompressionMode.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zCompressionMode.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zDecode.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zDecode.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zEncode.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zEncode.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zExtract.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zFolderInStream.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zFolderInStream.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zFolderOutStream.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zFolderOutStream.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zHandler.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zHandlerOut.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zHeader.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zHeader.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zIn.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zIn.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zItem.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zMethodID.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zMethodID.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zMethods.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zOut.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zOut.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zProperties.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zProperties.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zSpecStream.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zSpecStream.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zUpdate.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zUpdate.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Archive\7z\7zUpdateItem.h
# End Source File
# End Group
# Begin Group "7zip Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\7zip\Common\InBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\InBuffer.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\InOutTempBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\InOutTempBuffer.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\LimitedStreams.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\LimitedStreams.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\MultiStream.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\MultiStream.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\OutBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\OutBuffer.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\ProgressUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\ProgressUtils.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\StreamBinder.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\StreamBinder.h
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\StreamObjects.cpp
# End Source File
# Begin Source File

SOURCE=..\7zip\Common\StreamObjects.h
# End Source File
# End Group
# Begin Group "spl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\spl\splthread.cpp
# End Source File
# End Group
# End Target
# End Project
