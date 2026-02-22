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

// the DemoPlug project contains three groups of modules
//
// 1) module precomp.cpp, which builds demoplug.pch (/Yc"precomp.h")
// 2) modules that use demoplug.pch (/Yu"precomp.h")
// 3) the common sources have their own automatically generated WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
