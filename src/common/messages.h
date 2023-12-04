// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// makro MESSAGES_DISABLE - odpojeni modulu z kompilace
// makro MESSAGES_DEBUG - vypis souboru + cisla radku pred message + povoleni zprav ukazovanych pres DMESSAGE_???
// makro MULTITHREADED_MESSAGES_ENABLE - pripravi messages pro multithreadove aplikace

#ifndef MESSAGES_DISABLE

#define __RESOURCE_STRING_BUFFER_SIZE 1000
#define __SPRINTF_BUFFER_SIZE 1000
#define __ERROR_BUFFER_SIZE 1000

#ifdef MULTITHREADED_MESSAGES_ENABLE

// zajisti aktualnimu threadu pristup k funkcim a datum modulu
void EnterMessagesModul();
// volat az aktualni thread nebude potrebovat pristup k funkcim a datum modulu
void LeaveMessagesModul();

#endif // MULTITHREADED_MESSAGES_ENABLE

/// vraci ukazatel na globalni buffer, ktery naplni retezcem z resourcu
const char* rsc(int resID);
const WCHAR* rscW(int resID);

/// vraci ukazatel na globalni buffer, ktery naplni retezcem z sprintf
const char* spf(const char* formatString, ...);
const WCHAR* spfW(const WCHAR* formatString, ...);

/// vraci ukazatel na globalni buffer, ktery naplni retezcem z sprintf
const char* spf(int formatStringResID, ...);
const WCHAR* spfW(int formatStringResID, ...);

/// vraci ukazatel na globalni buffer, ktery naplni popisem chyby
const char* err(DWORD error);
const WCHAR* errW(DWORD error);

extern const char* __MessagesTitle;
extern const WCHAR* __MessagesTitleW;
extern HWND __MessagesParent;
extern HINSTANCE HInstance; // predpokladana promenna s handlem instance

/// Nastaveni titulu messageboxu generovanych makry MESSAGExx. Dela se kopie stringu.
/// Nastavuje vzdy titulek pro obe verze (ANSI + unicode).
void SetMessagesTitle(const char* title);
void SetMessagesTitleW(const WCHAR* title);

/// Nastaveni parenta messageboxu generovanych makry MESSAGExx
void SetMessagesParent(HWND parent);

class C__Messages
{
protected:
    C__StringStreamBuf MessagesStringBuf; // string buffer drzici data messages streamu
    C__TraceStream MessagesStrStream;     // vlastni messages stream

public:
    C__Messages();
#ifdef MULTITHREADED_MESSAGES_ENABLE
    ~C__Messages();
#endif // MULTITHREADED_MESSAGES_ENABLE

    C__TraceStream& OStream()
    {
        return MessagesStrStream;
    }

    int MessageBoxT(const char* lpCaption, // address of title of message box
                    UINT uType);           // style of message box

    int MessageBox(HWND hWnd,             // handle of owner window
                   const char* lpCaption, // address of title of message box
                   UINT uType);           // style of message box
};

class C__MessagesW
{
protected:
    C__StringStreamBufW MessagesStringBuf; // string buffer drzici data messages streamu
    C__TraceStreamW MessagesStrStream;     // vlastni messages stream

public:
    C__MessagesW();

    C__TraceStreamW& OStream() { return MessagesStrStream; }

    int MessageBoxT(const WCHAR* lpCaption, // address of title of message box
                    UINT uType);            // style of message box

    int MessageBox(HWND hWnd,              // handle of owner window
                   const WCHAR* lpCaption, // address of title of message box
                   UINT uType);            // style of message box
};

extern C__Messages __Messages;
extern C__MessagesW __MessagesW;

/**@name Makra generujici messageboxy.
 * je-li nadefinovano makro MESSAGES_DEBUG, zobrazuje se v messageboxich
 * na prvnim radku soubor a radek, na kterem je v kodu MESSAGExx\\
 *
 */

#ifdef MESSAGES_DEBUG

#ifndef MULTITHREADED_MESSAGES_ENABLE

/** zobrazi messagebox se zadanym textem, neni v novem threadu, distribuje message
    volajiciho threadu */
#define MESSAGE(parent, str, buttons) \
    (__Messages.OStream() << __FILE__ << " " << __LINE__ << ":\n\n", __Messages.OStream() << str, \
     __Messages) \
        .MessageBox(((parent) == NULL) ? __MessagesParent : (parent), __MessagesTitle, (buttons))

#define MESSAGEW(parent, str, buttons) \
    (__MessagesW.OStream() << __WFILE__ << L" " << __LINE__ << L":\n\n", __MessagesW.OStream() << str, \
     __MessagesW) \
        .MessageBox(((parent) == NULL) ? __MessagesParent : (parent), __MessagesTitleW, (buttons))

/** zobrazi messagebox se zadanym textem, v novem threadu, nerozdistribuje message
    volajiciho threadu */
#define MESSAGE_T(str, buttons) \
    (__Messages.OStream() << __FILE__ << " " << __LINE__ << ":\n\n", __Messages.OStream() << str, \
     __Messages) \
        .MessageBoxT(__MessagesTitle, (buttons))

#define MESSAGE_TW(str, buttons) \
    (__MessagesW.OStream() << __WFILE__ << L" " << __LINE__ << L":\n\n", __MessagesW.OStream() << str, \
     __MessagesW) \
        .MessageBoxT(__MessagesTitleW, (buttons))

#else // MULTITHREADED_MESSAGES_ENABLE

#define MESSAGE(parent, str, buttons) \
    (EnterMessagesModul(), __Messages.OStream() << __FILE__ << " " << __LINE__ << ":\n\n", \
     __Messages.OStream() << str, __Messages) \
        .MessageBox(((parent) == NULL) ? __MessagesParent : (parent), __MessagesTitle, (buttons))

#define MESSAGEW(parent, str, buttons) \
    (EnterMessagesModul(), __MessagesW.OStream() << __WFILE__ << L" " << __LINE__ << L":\n\n", \
     __MessagesW.OStream() << str, __MessagesW) \
        .MessageBox(((parent) == NULL) ? __MessagesParent : (parent), __MessagesTitleW, (buttons))

#define MESSAGE_T(str, buttons) \
    (EnterMessagesModul(), __Messages.OStream() << __FILE__ << " " << __LINE__ << ":\n\n", \
     __Messages.OStream() << str, __Messages) \
        .MessageBoxT(__MessagesTitle, (buttons))

#define MESSAGE_TW(str, buttons) \
    (EnterMessagesModul(), __MessagesW.OStream() << __WFILE__ << L" " << __LINE__ << L":\n\n", \
     __MessagesW.OStream() << str, __MessagesW) \
        .MessageBoxT(__MessagesTitleW, (buttons))

#endif // MULTITHREADED_MESSAGES_ENABLE

#else // MESSAGES_DEBUG

#ifndef MULTITHREADED_MESSAGES_ENABLE

#define MESSAGE(parent, str, buttons) \
    (__Messages.OStream() << str, __Messages).MessageBox(((parent) == NULL) ? __MessagesParent : (parent), __MessagesTitle, (buttons))

#define MESSAGEW(parent, str, buttons) \
    (__MessagesW.OStream() << str, __MessagesW).MessageBox(((parent) == NULL) ? __MessagesParent : (parent), __MessagesTitleW, (buttons))

#define MESSAGE_T(str, buttons) \
    (__Messages.OStream() << str, __Messages).MessageBoxT(__MessagesTitle, (buttons))

#define MESSAGE_TW(str, buttons) \
    (__MessagesW.OStream() << str, __MessagesW).MessageBoxT(__MessagesTitleW, (buttons))

#else // MULTITHREADED_MESSAGES_ENABLE

#define MESSAGE(parent, str, buttons) \
    (EnterMessagesModul(), __Messages.OStream() << str, \
     __Messages) \
        .MessageBox(((parent) == NULL) ? __MessagesParent : (parent), \
                    __MessagesTitle, (buttons))

#define MESSAGEW(parent, str, buttons) \
    (EnterMessagesModul(), __MessagesW.OStream() << str, \
     __MessagesW) \
        .MessageBox(((parent) == NULL) ? __MessagesParent : (parent), \
                    __MessagesTitleW, (buttons))

#define MESSAGE_T(str, buttons) \
    (EnterMessagesModul(), __Messages.OStream() << str, \
     __Messages) \
        .MessageBoxT(__MessagesTitle, (buttons))

#define MESSAGE_TW(str, buttons) \
    (EnterMessagesModul(), __MessagesW.OStream() << str, \
     __MessagesW) \
        .MessageBoxT(__MessagesTitleW, (buttons))

#endif // MULTITHREADED_MESSAGES_ENABLE

#endif // MESSAGES_DEBUG

/** zobrazi messagebox se zadanym textem a ikonou informaci, neni v novem
    threadu, distribuje message volajiciho threadu */
#define MESSAGE_I(parent, str, buttons) \
    MESSAGE(parent, str, MB_ICONINFORMATION | (buttons))

#define MESSAGE_IW(parent, str, buttons) \
    MESSAGEW(parent, str, MB_ICONINFORMATION | (buttons))

/** zobrazi messagebox se zadanym textem a ikonou otazky, neni v novem
    threadu, distribuje message volajiciho threadu */
#define MESSAGE_Q(parent, str, buttons) \
    MESSAGE(parent, str, MB_ICONQUESTION | (buttons))

#define MESSAGE_QW(parent, str, buttons) \
    MESSAGEW(parent, str, MB_ICONQUESTION | (buttons))

/** zobrazi messagebox se zadanym textem a ikonou chyby, neni v novem
    threadu, distribuje message volajiciho threadu */
#define MESSAGE_E(parent, str, buttons) \
    MESSAGE(parent, str, MB_ICONEXCLAMATION | (buttons))

#define MESSAGE_EW(parent, str, buttons) \
    MESSAGEW(parent, str, MB_ICONEXCLAMATION | (buttons))

/** zobrazi messagebox se zadanym textem a ikonou informaci, v novem threadu,
    nerozdistribuje message volajiciho threadu */
#define MESSAGE_TI(str, buttons) \
    MESSAGE_T(str, MB_ICONINFORMATION | (buttons))

#define MESSAGE_TIW(str, buttons) \
    MESSAGE_TW(str, MB_ICONINFORMATION | (buttons))

/** zobrazi messagebox se zadanym textem a ikonou otazky, v novem threadu,
    nerozdistribuje message volajiciho threadu */
#define MESSAGE_TQ(str, buttons) \
    MESSAGE_T(str, MB_ICONQUESTION | (buttons))

#define MESSAGE_TQW(str, buttons) \
    MESSAGE_TW(str, MB_ICONQUESTION | (buttons))

/** zobrazi messagebox se zadanym textem a ikonou chyby, v novem threadu,
    nerozdistribuje message volajiciho threadu */
#define MESSAGE_TE(str, buttons) \
    MESSAGE_T(str, MB_ICONEXCLAMATION | (buttons))

#define MESSAGE_TEW(str, buttons) \
    MESSAGE_TW(str, MB_ICONEXCLAMATION | (buttons))

#ifdef MESSAGES_DEBUG

/// MESSAGE_I zavisla na existenci makra MESSAGES_DEBUG
#define DMESSAGE_I(parent, str, buttons) MESSAGE_I(parent, str, (buttons))
#define DMESSAGE_IW(parent, str, buttons) MESSAGE_IW(parent, str, (buttons))

/// MESSAGE_Q zavisla na existenci makra MESSAGES_DEBUG
#define DMESSAGE_Q(parent, str, buttons) MESSAGE_Q(parent, str, (buttons))
#define DMESSAGE_QW(parent, str, buttons) MESSAGE_QW(parent, str, (buttons))

/// MESSAGE_E zavisla na existenci makra MESSAGES_DEBUG
#define DMESSAGE_E(parent, str, buttons) MESSAGE_E(parent, str, (buttons))
#define DMESSAGE_EW(parent, str, buttons) MESSAGE_EW(parent, str, (buttons))

/// MESSAGE_TI zavisla na existenci makra MESSAGES_DEBUG
#define DMESSAGE_TI(str, buttons) MESSAGE_TI(str, (buttons))
#define DMESSAGE_TIW(str, buttons) MESSAGE_TIW(str, (buttons))

/// MESSAGE_TQ zavisla na existenci makra MESSAGES_DEBUG
#define DMESSAGE_TQ(str, buttons) MESSAGE_TQ(str, (buttons))
#define DMESSAGE_TQW(str, buttons) MESSAGE_TQW(str, (buttons))

/// MESSAGE_E zavisla na existenci makra MESSAGES_DEBUG
#define DMESSAGE_TE(str, buttons) MESSAGE_TE(str, (buttons))
#define DMESSAGE_TEW(str, buttons) MESSAGE_TEW(str, (buttons))

#else // MESSAGES_DEBUG

// aby nedochazelo k problemum se stredniky v nize nadefinovanych makrech
inline int __MessagesEmptyFunction() { return 0; }

#define DMESSAGE_I(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_IW(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_Q(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_QW(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_E(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_EW(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TI(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TIW(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TQ(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TQW(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TE(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TEW(str, buttons) __MessagesEmptyFunction()

#endif // MESSAGES_DEBUG

#else // MESSAGES_DISABLE

// aby nedochazelo k problemum se stredniky v nize nadefinovanych makrech
inline int __MessagesEmptyFunction() { return 0; }

#define EnterMessagesModul() __MessagesEmptyFunction()
#define LeaveMessagesModul() __MessagesEmptyFunction()

/* tyhle funkce musi hlasit kompilacni chybu
const char *rsc(int resID);
const WCHAR *rscW(int resID);
const char *spf(const char *formatString, ...);
const WCHAR *spfW(const WCHAR *formatString, ...);
const char *spf(int formatStringResID, ...);
const WCHAR *spfW(int formatStringResID, ...);
const char *err(DWORD error);
const WCHAR *errW(DWORD error);
*/

#define SetMessagesTitle(title) __MessagesEmptyFunction()
#define SetMessagesTitleW(title) __MessagesEmptyFunction()
#define SetMessagesParent(parent) __MessagesEmptyFunction()

#define MESSAGE_I(parent, str, buttons) __MessagesEmptyFunction()
#define MESSAGE_IW(parent, str, buttons) __MessagesEmptyFunction()
#define MESSAGE_Q(parent, str, buttons) __MessagesEmptyFunction()
#define MESSAGE_QW(parent, str, buttons) __MessagesEmptyFunction()
#define MESSAGE_E(parent, str, buttons) __MessagesEmptyFunction()
#define MESSAGE_EW(parent, str, buttons) __MessagesEmptyFunction()
#define MESSAGE_TI(str, buttons) __MessagesEmptyFunction()
#define MESSAGE_TIW(str, buttons) __MessagesEmptyFunction()
#define MESSAGE_TQ(str, buttons) __MessagesEmptyFunction()
#define MESSAGE_TQW(str, buttons) __MessagesEmptyFunction()
#define MESSAGE_TE(str, buttons) __MessagesEmptyFunction()
#define MESSAGE_TEW(str, buttons) __MessagesEmptyFunction()

#define DMESSAGE_I(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_IW(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_Q(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_QW(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_E(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_EW(parent, str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TI(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TIW(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TQ(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TQW(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TE(str, buttons) __MessagesEmptyFunction()
#define DMESSAGE_TEW(str, buttons) __MessagesEmptyFunction()

#endif // MESSAGES_DISABLE

#ifndef UNICODE

#define rscT rsc
#define spfT spf
#define errT err

#define SetMessagesTitleT SetMessagesTitle

#define MESSAGE_IT MESSAGE_I
#define MESSAGE_QT MESSAGE_Q
#define MESSAGE_ET MESSAGE_E
#define MESSAGE_TIT MESSAGE_TI
#define MESSAGE_TQT MESSAGE_TQ
#define MESSAGE_TET MESSAGE_TE

#define DMESSAGE_IT DMESSAGE_I
#define DMESSAGE_QT DMESSAGE_Q
#define DMESSAGE_ET DMESSAGE_E
#define DMESSAGE_TIT DMESSAGE_TI
#define DMESSAGE_TQT DMESSAGE_TQ
#define DMESSAGE_TET DMESSAGE_TE

#else // UNICODE

#define rscT rscW
#define spfT spfW
#define errT errW

#define SetMessagesTitleT SetMessagesTitleW

#define MESSAGE_IT MESSAGE_IW
#define MESSAGE_QT MESSAGE_QW
#define MESSAGE_ET MESSAGE_EW
#define MESSAGE_TIT MESSAGE_TIW
#define MESSAGE_TQT MESSAGE_TQW
#define MESSAGE_TET MESSAGE_TEW

#define DMESSAGE_IT DMESSAGE_IW
#define DMESSAGE_QT DMESSAGE_QW
#define DMESSAGE_ET DMESSAGE_EW
#define DMESSAGE_TIT DMESSAGE_TIW
#define DMESSAGE_TQT DMESSAGE_TQW
#define DMESSAGE_TET DMESSAGE_TEW

#endif // UNICODE
