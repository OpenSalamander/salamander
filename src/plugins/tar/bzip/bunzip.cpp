#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "bzlib.h"
#include "bzip.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

CBZip::CBZip(const char *filename, HANDLE file, unsigned char *buffer, unsigned long start, unsigned long read, CQuadWord inputSize):
  CZippedFile(filename, file, buffer, start, read, inputSize), BZStream(NULL), EndReached(FALSE)
{
  CALL_STACK_MESSAGE2("CBZip::CBZip(%s, , , )", filename);
  
  // pokud neprosel konstruktor parenta, balime to rovnou
  if (!Ok)
    return;

  // nemuze to byt bzip, pokud mame min nez hlavicku...
  if (DataEnd - DataStart < 4)
  {
    Ok = FALSE;
    FreeBufAndFile = FALSE;
    return;
  }
  // pokud neni "magicke cislo" na zacatku, nejde o bzip
  if (DataStart[0] != 'B' || DataStart[1] != 'Z' ||
      DataStart[2] != 'h' || DataStart[3] < '0' || DataStart[3] > '9')
  {
    Ok = FALSE;
    FreeBufAndFile = FALSE;
    return;
  }
  // mame bzip, ale precteny header nepotvrzujeme, knihovna ho bude overovat znovu...

  // pripravime extractor
  BZStream = (bz_stream *)malloc(sizeof(bz_stream));
  if (BZStream == NULL)
  {
    Ok = FALSE;
    ErrorCode = IDS_ERR_MEMORY;
    FreeBufAndFile = FALSE;
    return;
  }
  memset(BZStream, 0, sizeof(bz_stream));

  int ret = BZ2_bzDecompressInit(BZStream, 0, 0);
  if (ret != BZ_OK)
  {
    free(BZStream);
    BZStream = NULL;
    Ok = FALSE;
    FreeBufAndFile = FALSE;
    switch (ret)
    {
      case BZ_MEM_ERROR:
        ErrorCode = IDS_ERR_MEMORY;
        break;
      case BZ_CONFIG_ERROR:
      case BZ_PARAM_ERROR:
      default:
        ErrorCode = IDS_ERR_INTERNAL;
        break;
    }
    return;
  }
  // hotovo
}

CBZip::~CBZip()
{
  CALL_STACK_MESSAGE1("CBZip::~CBZip()");
  if (BZStream)
  {
    int ret = BZ2_bzDecompressEnd(BZStream);
    if (ret != BZ_OK)
      SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_INTERNAL), LoadStr(IDS_GZERR_TITLE),
                                        MSGBOX_ERROR);
    free(BZStream);
  }
}

BOOL
CBZip::DecompressBlock(unsigned short needed)
{
  if (EndReached)
    return TRUE;
  int ret = BZ_OK;
  while (ret != BZ_STREAM_END && ExtrEnd < Window + BUFSIZE)
  {
    unsigned char *src = DataStart;
    // aspon jeden byte musi byt v bufferu
    if (DataEnd == DataStart)
      src = (unsigned char *)FReadBlock(0);
    if (src == NULL)
      return FALSE;
    if (DataEnd == DataStart)
    {
      Ok = FALSE;
      ErrorCode = IDS_ERR_EOF;
      return FALSE;
    }
    BZStream->next_in = (char *)DataStart;
    BZStream->avail_in = (unsigned int)(DataEnd - DataStart);
    BZStream->next_out = (char *)ExtrEnd;
    BZStream->avail_out = BUFSIZE - (unsigned int)(ExtrEnd - Window);
    ret = BZ2_bzDecompress(BZStream);
    if (ret != BZ_OK && ret != BZ_STREAM_END)
    {
      Ok = FALSE;
      switch (ret)
      {
        case BZ_DATA_ERROR:
        case BZ_DATA_ERROR_MAGIC:
          ErrorCode = IDS_ERR_CORRUPT;
          break;
        case BZ_MEM_ERROR:
          ErrorCode = IDS_ERR_MEMORY;
          break;
        case BZ_PARAM_ERROR:
        default:
          ErrorCode = IDS_ERR_INTERNAL;
          break;
      }
      return FALSE;
    }
    // commitnu prectena data ze vstupu
    FReadBlock((unsigned int)(BZStream->next_in - (char *)DataStart));
    unsigned short extracted = (unsigned short)((unsigned char *)BZStream->next_out - ExtrEnd);
    ExtrEnd = (unsigned char *)BZStream->next_out;
  }
  if (ret == BZ_STREAM_END)
    EndReached = TRUE;
  return TRUE;
}

extern "C" {
void bz_internal_error(int errcode);
}

void bz_internal_error(int errcode)
{
  SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_INTERNAL), LoadStr(IDS_GZERR_TITLE),
                                    MSGBOX_ERROR);
}
