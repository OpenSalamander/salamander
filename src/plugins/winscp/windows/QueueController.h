//---------------------------------------------------------------------------
#ifndef QueueControllerH
#define QueueControllerH
//---------------------------------------------------------------------------
#include <ComCtrls.hpp>
//---------------------------------------------------------------------------
enum TQueueOperation { qoNone, qoGoTo, qoPreferences, qoItemUserAction,
  qoItemQuery, qoItemError, qoItemPrompt, qoItemDelete, qoItemExecute,
  qoItemUp, qoItemDown, qoItemPause, qoItemResume, qoItemSpeed, qoPauseAll, qoResumeAll,
  qoOnceEmpty };
class TCustomListView;
class TCriticalSection;
class TQueueItemProxy;
class TTerminalQueueStatus;
//---------------------------------------------------------------------------
class TQueueController
{
public:
  __fastcall TQueueController(TListView * ListView);
  __fastcall ~TQueueController();

  TQueueOperation __fastcall DefaultOperation();
  bool __fastcall AllowOperation(TQueueOperation Operation, void ** Param = NULL);
  void __fastcall ExecuteOperation(TQueueOperation Operation, void * Param = NULL);

  void __fastcall UpdateQueueStatus(TTerminalQueueStatus * QueueStatus);
  void __fastcall RefreshQueueItem(TQueueItemProxy * QueueItem);
  static bool __fastcall QueueItemNeedsFrequentRefresh(TQueueItemProxy * QueueItem);

  __property TNotifyEvent OnChange = { read = FOnChange, write = FOnChange };
  __property bool Empty = { read = GetEmpty };

private:
  TListView * FListView;
  TTerminalQueueStatus * FQueueStatus;
  TNotifyEvent FOnChange;

  TQueueItemProxy * __fastcall QueueViewItemToQueueItem(
    TListItem * Item, bool * Detail = NULL);
  void __fastcall QueueViewDblClick(TObject * Sender);
  void __fastcall QueueViewKeyDown(TObject * Sender, WORD & Key, TShiftState Shift);
  virtual void __fastcall DoChange();
  bool __fastcall GetEmpty();

  static void __fastcall FillQueueViewItem(TListItem * Item,
    TQueueItemProxy * QueueItem, bool Detail);
};
//---------------------------------------------------------------------------
#endif
