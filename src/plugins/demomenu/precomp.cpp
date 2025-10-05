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

// The DemoMenu project contains three groups of modules:
//
// 1) The precomp.cpp module, which builds demomenu.pch (/Yc"precomp.h")
// 2) Modules using demomenu.pch (/Yu"precomp.h")
// 3) Shared code with its own automatically generated WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
