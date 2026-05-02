// This file is not part of libbzip2. It implements external function required by libbzip2.

#include "precomp.h"

extern "C" void bz_internal_error(int errcode)
{
  SalMessageBox(NULL, "Internal error", "libbzip2 encountered an error", MB_OK | MB_ICONEXCLAMATION);
}
