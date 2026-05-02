// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

// the DemoView project contains three groups of modules
//
// 1) the precomp.cpp module, which builds demoview.pch (/Yc"precomp.h")
// 2) modules that use demoview.pch (/Yu"precomp.h")
// 3) common modules have their own automatically generated WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
