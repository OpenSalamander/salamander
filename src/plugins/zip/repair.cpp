// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
/*
#include <crtdbg.h>
#include <ostream>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "selfextr\\comdefs.h"
#include "config.h"
#include "typecons.h"
#include "resource.h"
#include "common.h"
#include "extract.h"
#include "repair.h"
#include "dialogs.h"


CZipRepair::CZipRepair(const char * archive, CSalamanderForOperationsAbstract *salamander) :
                       CZipUnpack(archive, "", salamander, NULL)
{
  CALL_STACK_MESSAGE1("CZipRepair::CZipRepair");
  Test = true;
  DiskNum = 0;
  MultiVol = false;

  fixed_tl = NULL;
  fixed_td = NULL;
  SkipAllIOErrors = 0;
  SkipAllLongNames = 0;
  SkipAllEncrypted = 0;
  SkipAllDataErr = true;
  SkipAllBadMathods = true;
  Silent = 0;
  DialogFlags = PE_NOSKIP;
  InputBuffer = NULL;
  SlideWindow = NULL;

  TargetFile = NULL;
  CentrDir = NULL;
}

CZipRepair::~CZipRepair()
{
  CALL_STACK_MESSAGE1("CZipRepair::~CZipRepair");
  if (TargetFile) CloseCFile(TargetFile);
  InflateFreeFixedHufman();
  if (InputBuffer) free(InputBuffer);
  if (SlideWindow) free(SlideWindow);
  if (LocalHeader) free(LocalHeader);
  if (CentrDir) free(CentrDir);
}

int 
CZipRepair::RepeairArchive()
{
  CALL_STACK_MESSAGE1("CZipRepair::RepeairArchive()");

  char targetName[MAX_PATH];

  lstrcpy(targetName, ZipName);
  PathRemoveExtension(targetName);
  lstrcat(targetName, "_.zip");
  CCreateSFXDialog dlg(SalCalls->GetMainWindowHWND(), ZipName, targetName, NULL, NULL);
  if (dlg.Proceed() != IDOK) return ErrorID = IDS_NODISPLAY;

  int ret = CreateCFile(&ZipFile, ZipName, GENERIC_READ, FILE_SHARE_READ,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL);
  if (ret)
  {
    if (ret == ERR_LOWMEM) ErrorID = IDS_LOWMEM;
    else ErrorID = IDS_NODISPLAY;
  }

  if (TestIfExist(targetName)) return ErrorID;

  ret = CreateCFile(&TargetFile, targetName, GENERIC_WRITE, FILE_SHARE_READ,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL);
  if (ret)
  {
    if (ret == ERR_LOWMEM) return ErrorID = IDS_LOWMEM;
    else return ErrorID = IDS_NODISPLAY;
  }

  if (!CrcTab) 
  {
    CrcTab = (__UINT32 *) malloc(CRC_TAB_SIZE);
    if (CrcTab) MakeCrcTable(CrcTab);
    else return ErrorID = IDS_LOWMEM;
  }
  InputBuffer = (char *) malloc( DECOMPRESS_INBUFFER_SIZE);
  if (!InputBuffer) return IDS_LOWMEM;
  InBufSize = DECOMPRESS_INBUFFER_SIZE;
  SlideWindow = (char *) malloc( SLIDE_WINDOW_SIZE);
  if (!SlideWindow) return IDS_LOWMEM;
  WinSize = SLIDE_WINDOW_SIZE;
  LocalHeader = (CLocalFileHeader *) malloc( MAX_HEADER_SIZE);
  if (LocalHeader ) return IDS_LOWMEM;

  SalCalls->OpenProgressDialog(LoadStr(IDS_EXTRPROGTITLE), TRUE, NULL, FALSE);
  SalCalls->ProgressDialogAddText(LoadStr(IDS_REPAIRING));

  ErrorID = DoRepairFiles();
  if (ErrorID) DeleteFile(TempFile);

  SalCalls->CloseProgressDialog();
  
  return ErrorID;
}


int
CZipRepair::DoRepairFiles()
{
  CALL_STACK_MESSAGE1("CZipRepair::DoRepairFiles()");
  DWORD offs;
  char dummy[MAX_PATH];
  dummy[0] = 0;
  CFileInfo info;
  int ret;

  BOOL found;
  Extract = FALSE;
  ret = FindEOCentrDirSig(&found);
  Extract = TRUE;
  switch (ret)
  {
    case IDS_ERRMULTIVOL:
      Fatal = false
      break;
      
    case 0: 
      ret = ReadCentralDir();
      if (ret) return ret;
      break;

    default: return ret;
  }
  
  info.Name = malloc(MAX_HEADER_SIZE);
  if (!info.Name) return IDS_LOWMEM;
        
  while
  {
    offs = ScanForSignature(SIG_LOCALFH, offs);
    if (offs == -1) break;
    ZipFile->Flags |= PE_QUIET;
    ret = ReadLocalHeader(LocalHeader, offs);
    ZipFile->Flags &= ~PE_QUIET;
    if (!ret)
    {
      info->LocHeaderOffs = offs;
      ProcessLocalHeader2(LocalHeader, &info);
      if (info->Flag & GPF_DATADESCR) 
      {
        ret = FindDataDescr(info->DataOffset, &info);
      }
      if (!ret)
      {
        CheckCentrDir(info);
        BOOL ok;
        ret = ExtractSingleFile(dummy, 0, &info, &ok);
        if (ret || UserBreak) return ret;
        if (ok)
        {
          StoreThisFile(offs, size);
          offs += size - 4;
        }
      }
    }
    offs += 4;
  }
  return ret;
}

void
CZipRepair::ProcessLocalHeader2(CLocalFileHeader * header, CFileInfo * info)
{
  CALL_STACK_MESSAGE1("CZipRepair::ProcessLocalHeader2");
  FILETIME  ft;
  if (!DosDateTimeToFileTime(header->Date, header->Time, &ft))
  {
    SystemTimeToFileTime(&MinZipTime, &ft);
  }
  LocalFileTimeToFileTime(&ft, &header->LastWrite);
  info->Flag = header->Flag;             //general purpose bit flag 
  info->Method = header->Method;         //compression method
  info->Crc = header->Crc;               //crc-32
  info->CompSize = header->CompSize;     //compressed size
  info->Size = header->Size;             //uncompressed size
  //info->NameLen = lstrlen(outFileHeader->Name);       //filename length
  //outFileHeader->ExtraLen = header.ExtraLen;     //extra field length
  //info->CommentLen = header.CommentLen; //file comment length
  
  // tyhle bohuzel nevime, zkusime se pozdeji podivat do centralniho adresare
  // jestli nejaky najdeme
  info->StartDisk = 0;   //disk number start
  info->InterAttr = 0;   //internal file attributes
  info->FileAttr = FILE_ATTRIBUTE_NORMAL;
  info->IsDir = 0;

  info->DataOffset = info->LocHeaderOffs + sizeof(CLocalFileHeader) +  header->NameLen + header->ExtraLen;

  // nacteme jmeno
  char * sour;
  char * dest;
  //*(nameBuf + readLen) = 0;
  sour = (char *)header + sizeof(CLocalFileHeader);
  if (*sour == '/')
    sour++;
  dest = info->Name;
  while (sour < (char *)header + sizeof(CLocalFileHeader) + header->NameLen;
    if (*sour == '/')
    {
      *dest++ = '\\';
      sour++;
    }
    else
      *dest++ = *sour++;
  if (*(dest - 1) == '\\')//skip last slash if name specifies a directory
  {
    dest--;
    info->IsDir = 1;//predpokladam, ze je to adresar
  }
  *dest = 0;
  ValidateFileName(info->Name);

//  if (header->Version >> 8 == HS_FAT ||
//      header->Version >> 8 == HS_HPFS ||
//      header->Version >> 8 == HS_NTFS && (header->Version & 0x0F) == 0x50)
//    OemToChar(outputName, outputName);

//  TRACE_I("Processed name " << outputName);
  info->NameLen = dest - info->Name;
}

void
CZipRepair::ValidateFileName(char * name)
{
  while (*name)
  {
    switch (*name)
    {
      case '<':
      case '>':
      case ':':
      case '"':
      case '|': *name = '_'; break;
      default:
        if (*name < 32) *name = '_';
    }
    name++;
  }
}

void
CZipRepair::CheckCentrDir(CFileHeader * locHeader, CFileInfo * info)
{
  if (CentrDir)
  {
    CFileHeader * header = (CFileHeader *) CentrDir;
    char * end = CentrDir + CentrDirSize;
    char * ptr;
    while (ptr + sizeof(CFileHeader) <= end)
    {
      if (*(DWORD)ptr == SIG_CENTRFH)
      {
        header = (CFileHeader *) ptr;
        if (ptr + sizeof(CFileHeader) <= end ||
            ptr + header->NameLen + header->ExtraLen + header->CommentLen <= end)
        {
          if (header->NameLen == locHeader->NameLen && 
              strncmp(ptr + sizeof(CFileHeader), 
                      ptr + sizeof(CLocalFileHeader), locHeader->NameLen) == 0)
          {
            info->InterAttr = header.InterAttr; //internal file attributes
            switch (header->Version >> 8)
            {
              case HS_FAT:
              case HS_HPFS:
              case HS_VFAT:
              case HS_NTFS:
              case HS_ACORN:
              case HS_VM_CMS:
              case HS_MVS:
                info->FileAttr = header->ExternAttr & 0x0000FFFF;
                break;
              default:
                info->FileAttr = header->ExternAttr & 0x0000FFFF;
                if (header->Size)
                {
                  TRACE_E("directory has non zero size");
                  info->FileAttr &= ~FILE_ATTRIBUTE_DIRECTORY;
                }
            }
            header->IsDir = info->FileAttr & FILE_ATTRIBUTE_DIRECTORY;
          }
        }
        header = (CFileHeader *)(ptr + header->NameLen + header->ExtraLen + header->CommentLen);
      }
      else ptr++;
    }     
  }
}

int
CZipRepair::ReadCentralDir()
{
  CALL_STACK_MESSAGE1("CZipRepair::ReadCentralDir()");
  int errorID = 0;
  int ret;
  unsigned bytesRead;

  if (EOCentrDir.CentrDirSize)
  {
    CentrDirSize = EOCentrDir.CentrDirSize
    CentrDir = (char *) malloc(EOCentrDir.CentrDirSize);
    if (!CentrDir) errorID = IDS_LOWMEM;
    else
    {
      ZipFile->FilePointer = EOCentrDir.CentrDirOffs;
      ZipFile->Flags |= PE_QUIET;
      ret = Read(ZipFile, CentrDir, EOCentrDir.CentrDirSize, &bytesRead, NULL);
      ZipFile->Flags &= PE_QUIET;
      if (ret || bytesRead != EOCentrDir.CentrDirSize)
      {
        free(CentrDir);
        CentrDir = NULL;
      }
    }
  }
  return errorID;
}

int
ScanForSignature(DWORD sig, DWORD offs)
{
}
*/