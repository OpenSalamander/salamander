/* 7zDecode.c */

#include "7zDecode.h"
#include "../../lzma/LzmaDecode.h"
#include "../../Branch/BranchX86.h"

CMethodID k_LZMA = { { 0x3, 0x1, 0x1 }, 3 };
CMethodID k_BCJ_X86 = { { 0x3, 0x3, 0x1, 0x3 }, 4 };
CMethodID k_BCJ2 = { { 0x3, 0x3, 0x1, 0x1B }, 4 };

#ifdef _LZMA_IN_CB

typedef struct _CLzmaInCallbackImp
{
  ILzmaInCallback InCallback;
  ISzInStream *InStream;
  size_t Size;
} CLzmaInCallbackImp;

int LzmaReadImp(void *object, const unsigned char **buffer, SizeT *size)
{
  CLzmaInCallbackImp *cb = (CLzmaInCallbackImp *)object;
  size_t processedSize;
  SZ_RESULT res;
  *size = 0;
  res = cb->InStream->Read((void *)cb->InStream, (void **)buffer, cb->Size, &processedSize);
  *size = (SizeT)processedSize;
  if (processedSize > cb->Size)
    return (int)SZE_FAIL;
  cb->Size -= processedSize;
  if (res == SZ_OK)
    return 0;
  return (int)res;
}

#endif

SZ_RESULT SzDecode(const CFileSize *packSizes, const CFolder *folder,
    #ifdef _LZMA_IN_CB
    ISzInStream *inStream,
    #else
    const Byte *inBuffer,
    #endif
    Byte *outBuffer, size_t outSize, 
    size_t *outSizeProcessed, ISzAlloc *allocMain)
{
  UInt32 si, i;
  size_t inSize = 0;
  CCoderInfo *coder;
//  if (folder->NumPackStreams != 1)
//    return SZE_NOTIMPL;
  coder = folder->Coders;
  *outSizeProcessed = 0;

  for (si = 0; si < folder->NumPackStreams; si++)
    inSize += (size_t)packSizes[si];

  // DA: ! BACHA ! tady umime rozpakovat jeden LZMA stream s BJC koderem
  // jakmile tam bude neco jinyho, nebude to fungovat
  for (i = 0; i < folder->NumCoders; i++) {
    coder = folder->Coders + i;
    if (coder->NumInStreams == 1 && coder->NumOutStreams == 1) {
      if (AreMethodsEqual(&coder->MethodID, &k_LZMA)) {
        #ifdef _LZMA_IN_CB
        CLzmaInCallbackImp lzmaCallback;
        #else
        SizeT inProcessed;
        #endif

        CLzmaDecoderState state;  /* it's about 24-80 bytes structure, if int is 32-bit */
        int result;
        SizeT outSizeProcessedLoc;

        #ifdef _LZMA_IN_CB
        lzmaCallback.Size = inSize;
        lzmaCallback.InStream = inStream;
        lzmaCallback.InCallback.Read = LzmaReadImp;
        #endif

        if (LzmaDecodeProperties(&state.Properties, coder->Properties.Items, 
            coder->Properties.Capacity) != LZMA_RESULT_OK)
          return SZE_FAIL;

        state.Probs = (CProb *)allocMain->Alloc(LzmaGetNumProbs(&state.Properties) * sizeof(CProb));
        if (state.Probs == 0)
          return SZE_OUTOFMEMORY;

        #ifdef _LZMA_OUT_READ
        if (state.Properties.DictionarySize == 0)
          state.Dictionary = 0;
        else
        {
          state.Dictionary = (unsigned char *)allocMain->Alloc(state.Properties.DictionarySize);
          if (state.Dictionary == 0)
          {
            allocMain->Free(state.Probs);
            return SZE_OUTOFMEMORY;
          }
        }
        LzmaDecoderInit(&state);
        #endif

        result = LzmaDecode(&state,
            #ifdef _LZMA_IN_CB
            &lzmaCallback.InCallback,
            #else
            inBuffer, (SizeT)inSize, &inProcessed,
            #endif
            outBuffer, (SizeT)outSize, &outSizeProcessedLoc);
        *outSizeProcessed = (size_t)outSizeProcessedLoc;
        allocMain->Free(state.Probs);
        #ifdef _LZMA_OUT_READ
        allocMain->Free(state.Dictionary);
        #endif
        if (result == LZMA_RESULT_DATA_ERROR)
          return SZE_DATA_ERROR;
        if (result != LZMA_RESULT_OK)
          return SZE_FAIL;

      }
      else if (AreMethodsEqual(&coder->MethodID, &k_BCJ_X86)) {
        UInt32 _prevMask;
        UInt32 _prevPos;
        x86_Convert_Init(_prevMask, _prevPos);
        x86_Convert(outBuffer, *outSizeProcessed, 0, &_prevMask, &_prevPos, 0);
      }
      else {
        // coder that we can not handle
        return SZE_NOTIMPL;
      }
    }
    else {
      if (AreMethodsEqual(&coder->MethodID, &k_BCJ2)) {
        return SZE_NOTIMPL;
      }
      else {
        // coder that we can not handle
        return SZE_NOTIMPL;
      }
    }
  }
  // end of DA

  return SZ_OK;

/*
  if (AreMethodsEqual(&coder->MethodID, &k_LZMA))
  {
    #ifdef _LZMA_IN_CB
    CLzmaInCallbackImp lzmaCallback;
    #else
    SizeT inProcessed;
    #endif

    CLzmaDecoderState state;  /* it's about 24-80 bytes structure, if int is 32-bit */
/*    int result;
    SizeT outSizeProcessedLoc;

    #ifdef _LZMA_IN_CB
    lzmaCallback.Size = inSize;
    lzmaCallback.InStream = inStream;
    lzmaCallback.InCallback.Read = LzmaReadImp;
    #endif

    if (LzmaDecodeProperties(&state.Properties, coder->Properties.Items, 
        coder->Properties.Capacity) != LZMA_RESULT_OK)
      return SZE_FAIL;

    state.Probs = (CProb *)allocMain->Alloc(LzmaGetNumProbs(&state.Properties) * sizeof(CProb));
    if (state.Probs == 0)
      return SZE_OUTOFMEMORY;

    #ifdef _LZMA_OUT_READ
    if (state.Properties.DictionarySize == 0)
      state.Dictionary = 0;
    else
    {
      state.Dictionary = (unsigned char *)allocMain->Alloc(state.Properties.DictionarySize);
      if (state.Dictionary == 0)
      {
        allocMain->Free(state.Probs);
        return SZE_OUTOFMEMORY;
      }
    }
    LzmaDecoderInit(&state);
    #endif

    result = LzmaDecode(&state,
        #ifdef _LZMA_IN_CB
        &lzmaCallback.InCallback,
        #else
        inBuffer, (SizeT)inSize, &inProcessed,
        #endif
        outBuffer, (SizeT)outSize, &outSizeProcessedLoc);
    *outSizeProcessed = (size_t)outSizeProcessedLoc;
    allocMain->Free(state.Probs);
    #ifdef _LZMA_OUT_READ
    allocMain->Free(state.Dictionary);
    #endif
    if (result == LZMA_RESULT_DATA_ERROR)
      return SZE_DATA_ERROR;
    if (result != LZMA_RESULT_OK)
      return SZE_FAIL;

    // DA: FILTER (BJC)
    *outSizeProcessed = (size_t)outSizeProcessedLoc;
    if (folder->NumCoders == 2)  // useFilter
    {
    }
    else if (folder->NumCoders > 2)
      return SZE_NOTIMPL;
    // end of DA

    return SZ_OK;
  }
*/

//  return SZE_NOTIMPL;
}
