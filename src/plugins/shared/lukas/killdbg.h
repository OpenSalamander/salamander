// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************
//
// Eliminace TRACE a CALL-STACK pro projekty, ktere to nepodporuji

// ****************************************************************************
//
// TRACE
//

// to avoid semicolon issues in the macros defined below
inline void __TraceEmptyFunction() {}

#define TRACE_MI(file, line, str) __TraceEmptyFunction()
#define TRACE_I(str) __TraceEmptyFunction()
#define TRACE_W(str) __TraceEmptyFunction()
#define TRACE_ME(file, line, str) __TraceEmptyFunction()
#define TRACE_E(str) __TraceEmptyFunction()
#define TRACE_MC(file, line, str) (*((int*)NULL) = 0x666)
#define TRACE_C(str) (*((int*)NULL) = 0x666)
#define SetTraceThreadName(name) __TraceEmptyFunction()
#define ConnectToTraceServer() __TraceEmptyFunction()

// ****************************************************************************
//
// CALL-STACK
//

#define CALL_STACK_MESSAGE1(p1)
#define CALL_STACK_MESSAGE2(p1, p2)
#define CALL_STACK_MESSAGE3(p1, p2, p3)
#define CALL_STACK_MESSAGE4(p1, p2, p3, p4)
#define CALL_STACK_MESSAGE5(p1, p2, p3, p4, p5)
#define CALL_STACK_MESSAGE6(p1, p2, p3, p4, p5, p6)
#define CALL_STACK_MESSAGE7(p1, p2, p3, p4, p5, p6, p7)
#define CALL_STACK_MESSAGE8(p1, p2, p3, p4, p5, p6, p7, p8)
#define CALL_STACK_MESSAGE9(p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define CALL_STACK_MESSAGE10(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define CALL_STACK_MESSAGE11(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
#define CALL_STACK_MESSAGE12(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)
#define CALL_STACK_MESSAGE13(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)

// empty macro: tells CheckStk that no call-stack message is wanted for this function
#define CALL_STACK_MESSAGE_NONE

// ****************************************************************************
//
// SetThreadNameInVCAndTrace
//

// eliminates calls to SetThreadNameInVCAndTrace
inline void SetThreadNameInVCAndTrace(const char* name) {}
