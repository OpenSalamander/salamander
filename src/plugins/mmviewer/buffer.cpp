// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "buffer.h"
#include "output.h"
#include "renderer.h"
#include "mmviewer.h"

BOOL CBuffer::Reserve(int size)
{
    CALL_STACK_MESSAGE2("CBuffer::Reserve(%d)", size);
    if (size > Allocated)
    {
        int s = max(Allocated * 2, size);
        /*
    if (Persistent)
    {
      void * ptr = SalGeneral->Realloc(Buffer, s);
      if (!ptr) return FALSE;
      Buffer = ptr;
    }
    else*/
        {
            Release();
            Buffer = SalGeneral->Alloc(s);
            if (!Buffer)
                return FALSE;
        }
        Allocated = s;
    }
    return TRUE;
}

void CBuffer::Release()
{
    if (Buffer)
        SalGeneral->Free(Buffer);
    Buffer = NULL;
    Allocated = 0;
}