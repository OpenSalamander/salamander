// VirtThread.cpp

#include "StdAfx.h"

#include "VirtThread.h"

static THREAD_FUNC_DECL CoderThread(void *p)
{
  for (;;)
  {
    CVirtThread *t = (CVirtThread *)p;
    t->StartEvent.Lock();
    if (t->Exit)
      return 0;
    t->Execute();
    t->FinishedEvent.Set();
  }
}

// OPENSAL_7ZIP_PATCH BEGIN
extern "C" {
  DWORD
  RunThreadWithCallStackObject(LPTHREAD_START_ROUTINE startAddress, LPVOID parameter);
}

static THREAD_FUNC_DECL CoderThreadWrap(void *p) {
  return (THREAD_FUNC_RET_TYPE) RunThreadWithCallStackObject((LPTHREAD_START_ROUTINE) CoderThread, p);
}

WRes CVirtThread::Create()
{
  RINOK(StartEvent.CreateIfNotCreated());
  RINOK(FinishedEvent.CreateIfNotCreated());
  StartEvent.Reset();
  FinishedEvent.Reset();
  Exit = false;
  if (Thread.IsCreated())
    return S_OK;
  return Thread.Create(CoderThreadWrap, this);
}
// OPENSAL_7ZIP_PATCH END

void CVirtThread::Start()
{
  Exit = false;
  StartEvent.Set();
}

void CVirtThread::WaitThreadFinish()
{
  Exit = true;
  if (StartEvent.IsCreated())
    StartEvent.Set();
  if (Thread.IsCreated())
  {
    Thread.Wait();
    Thread.Close();
  }
}
