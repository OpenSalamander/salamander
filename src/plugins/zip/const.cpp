// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "dbg.h"

#include "config.h"
#include "selfextr\\comdefs.h"
#include "typecons.h"
//#include "common.h"

extern CSalamanderGeneralAbstract* SalamanderGeneral;

const SYSTEMTIME MaxZipTime = {2107, 12, 5, 30, 23, 59, 58, 000};
const SYSTEMTIME MinZipTime = {1980, 01, 2, 01, 00, 00, 00, 000};

/*
CSfxSettings::CSfxSettings(const CSfxSettings &origin) 
{
  Flags = origin.Flags;
  strcpy(Command, origin.Command);
  strcpy(SfxFile, origin.SfxFile);
  strcpy(Text, origin.Text);
  strcpy(Title, origin.Title);
  strcpy(TargetDir, origin.TargetDir);
  strcpy(ExtractBtnText, origin.ExtractBtnText);
  strcpy(IconFile, origin.IconFile);
  IconIndex = origin.IconIndex;
  MBoxStyle = origin.MBoxStyle;
  MBoxText = SalamanderGeneral->DupStr(origin.MBoxText);
  strcpy(MBoxTitle, origin.MBoxTitle);
  strcpy(WaitFor, origin.WaitFor);
}

CSfxSettings & CSfxSettings::operator=(const CSfxSettings &origin) 
{
  Flags = origin.Flags;
  strcpy(Command, origin.Command);
  strcpy(SfxFile, origin.SfxFile);
  strcpy(Text, origin.Text);
  strcpy(Title, origin.Title);
  strcpy(TargetDir, origin.TargetDir);
  strcpy(ExtractBtnText, origin.ExtractBtnText);
  strcpy(IconFile, origin.IconFile);
  IconIndex = origin.IconIndex;
  MBoxStyle = origin.MBoxStyle;
  if (MBoxText) free(MBoxText);
  MBoxText = SalamanderGeneral->DupStr(origin.MBoxText);
  strcpy(MBoxTitle, origin.MBoxTitle);
  strcpy(WaitFor, origin.WaitFor);
  return *this;
}

char * CSfxSettings::SetMBoxText(const char * text)
{
  if (MBoxText) free(MBoxText);
  MBoxText = SalamanderGeneral->DupStr(text);
  return MBoxText;
}
*/