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

// projekt DemoPlug obsahuje tri skupiny modulu
//
// 1) modul precomp.cpp, ktery postavi demoplug.pch (/Yc"precomp.h")
// 2) moduly vyuzivajici demoplug.pch (/Yu"precomp.h")
// 3) commony maji vlastni, automaticky generovany WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
