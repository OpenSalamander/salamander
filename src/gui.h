// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CProgressBar
//
// The class is always allocated (CObjectOrigin origin = ooAllocated)

class CProgressBar : public CWindow
{
public:
    // hDlg is the parent window (dialog or window)
    // ctrlID is the child window ID
    CProgressBar(HWND hDlg, int ctrlID);
    ~CProgressBar();

    // SetProgress can be called from any thread; internally it posts WM_USER_SETPROGRESS
    // the thread progress bars themselves must, however, be running
    void SetProgress(DWORD progress, const char* text = NULL);
    void SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal,
                      const char* text = NULL);

    void SetSelfMoveTime(DWORD time);
    void SetSelfMoveSpeed(DWORD moveTime);
    void Stop();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hDC);

    void MoveBar();

protected:
    int Width, Height;
    DWORD Progress;
    CBitmap* Bitmap;     // bitmap for memDC -> paint cache
    int BarX;            // X coordinate of the rectangle for unknown progress (for Progress == -1)
    BOOL MoveBarRight;   // is the rectangle moving to the right?
    DWORD SelfMoveTime;  // 0: after calling SetProgress(-1), the rectangle moves by only one step (0 is the default value)
                         // >0: time in [ms] for which it continues moving after SetProgress(-1) is called
    DWORD SelfMoveTicks; // stored GetTickCount() value from the last call to SetSelfMoveTime()
    DWORD SelfMoveSpeed; // rectangle movement speed: the value is in [ms] and specifies after what time the rectangle moves
                         // the minimum is 10 ms, the default value is 50 ms, i.e. 20 moves per second
                         // beware of low values, the animation itself can then noticeably load the CPU
    BOOL TimerIsRunning; // is the timer running?
    char* Text;          // if not NULL, it is displayed instead of the number
    HFONT HFont;         // font for the progress bar
};

//****************************************************************************
//
// CStaticText
//
// The class is always allocated (CObjectOrigin origin = ooAllocated)

class CStaticText : public CWindow
{
public:
    // hDlg is the parent window (dialog or window)
    // ctrlID is the child window ID
    // flags is a combination of values from the STF_* family (shared\spl_gui.h)
    CStaticText(HWND hDlg, int ctrlID, DWORD flags);
    ~CStaticText();

    // sets Text; returns TRUE on success and FALSE if memory is insufficient
    BOOL SetText(const char* text);

    // note: the returned Text may be NULL
    const char* GetText() { return Text; }

    // sets Text (if it starts or ends with a space, it is wrapped in double quotes),
    // returns TRUE on success and FALSE if memory is insufficient
    BOOL SetTextToDblQuotesIfNeeded(const char* text);

    // some filesystems may use a different path-component separator
    // it must be different from '\0'
    void SetPathSeparator(char separator);

    // assigns the text that will be shown as the tooltip
    BOOL SetToolTipText(const char* text);

    // assigns the window and ID to which WM_USER_TTGETTEXT is sent when the tooltip is shown
    void SetToolTip(HWND hNotifyWindow, DWORD id);

    // if set to TRUE, the tooltip can be triggered by clicking the text or
    // pressing Up/Down/Space when the control has focus
    // the tooltip is then shown just below the text and remains visible
    // by default it is set to FALSE
    void EnableHintToolTip(BOOL enable);

    //    void UpdateControl();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PrepareForPaint();

    BOOL TextHitTest(POINT* screenCursorPos);
    int GetTextXOffset(); // returns the X text offset based on Alignment, Width, and TextWidth
    void DrawFocus(HDC hDC);

    BOOL ToolTipAssigned();

    BOOL ShowHint();

    DWORD Flags;         // flags controlling the control behavior
    char* Text;          // allocated text
    int TextLen;         // string length
    char* Text2;         // allocated text containing the ellipsis; used only with STF_END_ELLIPSIS or STF_PATH_ELLIPSIS
    int Text2Len;        // length of Text2
    int* AlpDX;          // array of substring lengths; used only with STF_END_ELLIPSIS or STF_PATH_ELLIPSIS
    int TextWidth;       // text width in pixels
    int TextHeight;      // text height in pixels
    int Allocated;       // size of the allocated buffer for 'Text' and 'AlpDX'
    int Width, Height;   // dimensions of the static control
    CBitmap* Bitmap;     // drawing cache; used only with STF_CACHED_PAINT
    HFONT HFont;         // font handle used for drawing the text
    BOOL DestroyFont;    // if HFont is allocated, it is TRUE; otherwise it is FALSE
    BOOL ClipDraw;       // drawing must be clipped because it would go outside otherwise
    BOOL Text2Draw;      // the buffer containing the ellipsis will be drawn
    int Alignment;       // 0=left, 1=center, 2=right
    char PathSeparator;  // path-component separator; '\\' by default
    BOOL MouseIsTracked; // we enabled mouse-leave tracking
    // tooltip support
    char* ToolTipText; // string that will be shown as our tooltip
    HWND HToolTipNW;   // notification window
    DWORD ToolTipID;   // and the ID under which the tooltip should ask for the text
    BOOL HintMode;     // should the tooltip be shown as a Hint?
    WORD UIState;      // accelerator display state
};

//****************************************************************************
//
// CHyperLink
//

class CHyperLink : public CStaticText
{
public:
    // hDlg is the parent window (dialog or window)
    // ctrlID is the child window ID
    // flags is a combination of values from the STF_* family (shared\spl_gui.h)
    CHyperLink(HWND hDlg, int ctrlID, DWORD flags = STF_UNDERLINE | STF_HYPERLINK_COLOR);

    void SetActionOpen(const char* file);
    void SetActionPostCommand(WORD command);
    BOOL SetActionShowHint(const char* text);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnContextMenu(int x, int y);
    BOOL ExecuteIt();

protected:
    char File[MAX_PATH]; // if not zero, it is passed to ShellExecute
    WORD Command;        // if not zero, it is posted on action
    HWND HDialog;        // parent dialog
};

//****************************************************************************
//
// CColorRectangle
//
// paints the entire object area with Color
// combine with WS_EX_CLIENTEDGE
//

class CColorRectangle : public CWindow
{
protected:
    COLORREF Color;

public:
    CColorRectangle(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated);

    void SetColor(COLORREF color);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void PaintFace(HDC hdc);
};

//****************************************************************************
//
// CColorGraph
//

class CColorGraph : public CWindow
{
protected:
    HBRUSH Color1Light;
    HBRUSH Color1Dark;
    HBRUSH Color2Light;
    HBRUSH Color2Dark;

    RECT ClientRect;
    double UsedProc;

public:
    CColorGraph(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated);
    ~CColorGraph();

    void SetColor(COLORREF color1Light, COLORREF color1Dark,
                  COLORREF color2Light, COLORREF color2Dark);

    void SetUsed(double used); // used = <0, 1>

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void PaintFace(HDC hdc);
};

//****************************************************************************
//
// CBitmapButton
//

class CButton : public CWindow
{
protected:
    DWORD Flags;
    BOOL DropDownPressed;
    BOOL Checked;
    BOOL ButtonPressed;
    BOOL Pressed;
    BOOL DefPushButton;
    BOOL Captured;
    BOOL Space;
    RECT ClientRect;
    // tooltip support
    BOOL MouseIsTracked;  // we enabled mouse-leave tracking
    char* ToolTipText;    // string that will be shown as our tooltip
    HWND HToolTipNW;      // notification window
    DWORD ToolTipID;      // and the ID under which the tooltip should ask for the text
    DWORD DropDownUpTime; // time in [ms] when the drop-down was released, to guard against a new press
    // XP Theme support
    BOOL Hot;
    WORD UIState; // accelerator display state

public:
    CButton(HWND hDlg, int ctrlID, DWORD flags, CObjectOrigin origin = ooAllocated);
    ~CButton();

    // assigns the text that will be shown as the tooltip
    BOOL SetToolTipText(const char* text);

    // assigns the window and ID to which WM_USER_TTGETTEXT is sent when the tooltip is shown
    void SetToolTip(HWND hNotifyWindow, DWORD id);

    DWORD GetFlags();
    void SetFlags(DWORD flags, BOOL updateWindow);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void PaintFace(HDC hdc, const RECT* rect, BOOL enabled);

    int HitTest(LPARAM lParam); // returns 0: nowhere; 1: button; 2: drop-down
    void PaintFrame(HDC hDC, const RECT* r, BOOL down);
    void PaintDrop(HDC hDC, const RECT* r, BOOL enabled);
    int GetDropPartWidth();

    void RePaint();
    void NotifyParent(WORD notify);

    BOOL ToolTipAssigned();
};

//****************************************************************************
//
// CColorArrowButton
//
// background with text followed by an arrow; used to expand the menu
//

class CColorArrowButton : public CButton
{
protected:
    COLORREF TextColor;
    COLORREF BkgndColor;
    BOOL ShowArrow;

public:
    CColorArrowButton(HWND hDlg, int ctrlID, BOOL showArrow, CObjectOrigin origin = ooAllocated);

    void SetColor(COLORREF textColor, COLORREF bkgndColor);
    //    void     SetColor(COLORREF color);

    void SetTextColor(COLORREF textColor);
    void SetBkgndColor(COLORREF bkgndColor);

    COLORREF GetTextColor() { return TextColor; }
    COLORREF GetBkgndColor() { return BkgndColor; }

protected:
    virtual void PaintFace(HDC hdc, const RECT* rect, BOOL enabled);
};

//****************************************************************************
//
// CToolbarHeader
//

//#define TOOLBARHDR_USE_SVG

class CToolBar;

class CToolbarHeader : public CWindow
{
protected:
    CToolBar* ToolBar;
#ifdef TOOLBARHDR_USE_SVG
    HIMAGELIST HEnabledImageList;
    HIMAGELIST HDisabledImageList;
#else
    HIMAGELIST HHotImageList;
    HIMAGELIST HGrayImageList;
#endif
    DWORD ButtonMask;   // buttons in use
    HWND HNotifyWindow; // where commands are sent
    WORD UIState;       // accelerator display state

public:
    CToolbarHeader(HWND hDlg, int ctrlID, HWND hAlignWindow, DWORD buttonMask);

    void EnableToolbar(DWORD enableMask);
    void CheckToolbar(DWORD checkMask);
    void SetNotifyWindow(HWND hWnd) { HNotifyWindow = hWnd; }

protected:
#ifdef TOOLBARHDR_USE_SVG
    void CreateImageLists(HIMAGELIST* enabled, HIMAGELIST* disabled);
#endif

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnPaint(HDC hDC, BOOL hideAccel, BOOL prefixOnly);
};

//****************************************************************************
//
// CAnimate
//

/*
class CAnimate: public CWindow
{
  protected:
    HBITMAP          HBitmap;             // bitmap from which we take the individual animation frames
    int              FramesCount;         // number of frames in the bitmap
    int              FirstLoopFrame;      // when looping, we jump from the end back to this frame
    SIZE             FrameSize;           // frame size in pixels
    CRITICAL_SECTION GDICriticalSection;  // critical section for access to GDI resources
    CRITICAL_SECTION DataCriticalSection; // critical section for data access
    HANDLE           HThread;
    HANDLE           HRunEvent;           // if signed, the animation thread is running
    HANDLE           HTerminateEvent;     // if signed, the thread terminates
    COLORREF         BkColor;

    // control variables used when HRunEvent is signed
    BOOL             SleepThread;         // the thread should sleep, HRunEvent will be reset

    int              CurrentFrame;        // zero-based index of the currently displayed frame
    int              NestedCount;
    BOOL             MouseIsTracked;      // we enabled mouse-leave tracking

  public:
    // 'hBitmap'          is the bitmap from which we draw the frames during animation;
    //                    the frames must be stacked vertically and must have constant height
    // 'framesCount'      specifies the total number of frames in the bitmap
    // 'firstLoopFrame'   zero-based frame index to which we return after reaching
    //                    the end during cyclic animation
    CAnimate(HBITMAP hBitmap, int framesCount, int firstLoopFrame, COLORREF bkColor, CObjectOrigin origin = ooAllocated);
    BOOL IsGood();                // did the constructor complete successfully?

    void Start();                 // if not animating, start it
    void Stop();                  // stops the animation and shows the initial frame
    void GetFrameSize(SIZE *sz);  // returns the size in pixels needed to display a frame

  protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hdc = NULL);   // displays the current frame; if hdc is NULL, obtain the window DC
    void FirstFrame();            // sets Frame to the initial frame
    void NextFrame();             // sets Frame to the next frame; skip the initial sequence

    // thread bodies
    static unsigned ThreadF(void *param);
    static unsigned AuxThreadEH(void *param);
    static DWORD WINAPI AuxThreadF(void *param);

    // ThreadF is a friend so it can access our data
    friend static unsigned ThreadF(void *param);
};
*/

//
//  ****************************************************************************
// ChangeToArrowButton
//

BOOL ChangeToArrowButton(HWND hParent, int ctrlID);

//
//  ****************************************************************************
// ChangeToIconButton
//

BOOL ChangeToIconButton(HWND hParent, int ctrlID, int iconID);

//
//  ****************************************************************************
// VerticalAlignChildToChild
//
// used to align the "browse" button next to an edit line / combo box
// (in Resource Workshop it is hard to place the button correctly next to a combo box)
// adjusts the size and position of child window 'alignID' so that it sits at the same height
// (and has the same height) as child 'toID'
void VerticalAlignChildToChild(HWND hParent, int alignID, int toID);

//
//  ****************************************************************************
// CondenseStaticTexts
//
// moves static texts so that they follow each other closely; the distance between them
// will equal the width of a space in the dialog font; 'staticsArr' is an array of static IDs terminated by zero
void CondenseStaticTexts(HWND hWindow, int* staticsArr);

//
//  ****************************************************************************
// ArrangeHorizontalLines
//
// finds horizontal lines and extends them from the right up to the text they follow
// it also finds checkboxes and radio buttons that serve as groupbox labels and shortens
// them according to their text and the current dialog font (eliminating unnecessary
// gaps caused by different display DPI settings)
void ArrangeHorizontalLines(HWND hWindow);

//
//  ****************************************************************************
// GetWindowFontHeight
//
// gets the current font for hWindow and returns its height
int GetWindowFontHeight(HWND hWindow);

//
//  ****************************************************************************
// GetWindowFontHeight
//
// creates an imagelist containing the two checkbox states (unchecked and checked)
// and returns its handle; 'itemSize' is the width and height of one item in pixels
HIMAGELIST CreateCheckboxImagelist(int itemSize);

//
//  ****************************************************************************
// SalLoadIcon
//
// loads the icon specified by 'hInst' and 'iconName' and returns its handle, or NULL on error;
// 'iconSize' specifies the requested icon size; the function is High DPI ready
//
// Note: the old LoadIcon() API cannot handle larger icon sizes, so we introduce this
// function, which loads icons using the newer LoadIconWithScaleDown()
HICON SalLoadIcon(HINSTANCE hInst, LPCTSTR iconName, CIconSizeEnum iconSize);
