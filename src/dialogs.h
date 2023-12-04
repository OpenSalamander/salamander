// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//
// ****************************************************************************

class CSelectDialog : public CCommonDialog
{
public:
    CSelectDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent, char* mask,
                  CObjectOrigin origin = ooStandard)
        : CCommonDialog(modul, resID, helpID, parent, origin) { Mask = mask; }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    char* Mask;
};

//
// ****************************************************************************

class CCopyMoveDialog : public CCommonDialog
{
protected:
    char *Title,
        *Path;
    CTruncatedString* Subject;
    int PathBufSize;
    char** History;
    int HistoryCount;
    BOOL DirectoryHelper;
    int SelectionEnd;

public:
    // 'history' urcuje, jestli dialog bude obsahovat combbox(TRUE), nebo editline (FALSE)
    // 'directoryHelper' urcuje, zda se pouzije resource s tlacitkem za ediline pro vyber adresare
    // 'selectionEnd' urcuje po ktery znak ma byt selected nazev (slouzi pro quick rename) -1 == all
    CCopyMoveDialog(HWND parent, char* path, int pathBufSize, char* title,
                    CTruncatedString* subject, DWORD helpID,
                    char* history[], int historyCount, BOOL directoryHelper);

    void SetSelectionEnd(int selectionEnd);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CEditNewFileDialog : public CCopyMoveDialog
{
public:
    CEditNewFileDialog(HWND parent, char* path, int pathBufSize, CTruncatedString* subject, char* history[], int historyCount);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCriteriaData;
class CButton;

class CCopyMoveMoreDialog : public CCommonDialog
{
protected:
    char *Title,
        *Path;
    CTruncatedString* Subject;
    int PathBufSize;
    char** History;
    int HistoryCount;
    CCriteriaData* CriteriaInOut; // pro transfer dat do dialogu a ven z nej (pri OK)
    CCriteriaData* Criteria;      // alokuji, protoze kvuli staticke deklaraci bych musel sachovat s headrama
    BOOL HavePermissions;
    BOOL SupportsADS;

    int OriginalWidth;    // plna sirka dialogu
    int OriginalHeight;   // plna vyska dialogu
    int OriginalButtonsY; // Y pozice tlacitek v client souradnicich
    int SpacerHeight;     // vymezovac pro zmensovani/zvetsovani dialogu
    BOOL Expanded;        // jsme prave rozbaleni?

    CButton* MoreButton;

public:
    // 'history' urcuje, jestli dialog bude obsahovat combbox(TRUE), nebo editline (FALSE)
    // 'directoryHelper' urcuje, zda se pouzije resource s tlacitkem za ediline pro vyber adresare
    CCopyMoveMoreDialog(HWND parent, char* path, int pathBufSize, char* title,
                        CTruncatedString* subject, DWORD helpID,
                        char* history[], int historyCount, CCriteriaData* criteriaInOut,
                        BOOL havePermissions, BOOL supportsADS);
    ~CCopyMoveMoreDialog();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void SetOptionsButtonState(BOOL more);
    void DisplayMore(BOOL more, BOOL fast); // fast znamena, ze je vse cerstve inicializovano a nemusime resetit hodnoty
    HDWP OffsetControl(HDWP hdwp, int id, int yOffset);
    void EnableControls();
    void TransferCriteriaControls(CTransferInfo& ti);
    void UpdateAdvancedText();
};

//
// ****************************************************************************

#define MESSAGEBOX_MAXBUTTONS 4 // maximalni pocet tlacitek

class CMessageBox : public CCommonDialog
{
protected:
    DWORD Flags;
    char* Title;
    char* CheckText;
    CTruncatedString Text;
    BOOL* Check;
    HICON HOwnIcon;
    MSGBOXEX_CALLBACK HelpCallback;
    char* AliasBtnNames;
    char* URL;
    char* URLText;
    // pro WM_COPY:
    int ButtonsID[MESSAGEBOX_MAXBUTTONS]; // IDcka tlacitek po premapovani
    int BackgroundSeparator;              // y offset rozdeleni bila/seda (Vista+)

public:
    CMessageBox(HWND parent, DWORD flags, const char* title, const char* text,
                const char* checkText, BOOL* check, HICON hOwnIcon,
                DWORD contextHelpId, MSGBOXEX_CALLBACK helpCallback,
                const char* aliasBtnNames, const char* url, const char* urlText);

    CMessageBox(HWND parent, DWORD flags, const char* title, CTruncatedString* text,
                const char* checkText, BOOL* check, HICON hOwnIcon,
                DWORD contextHelpId, MSGBOXEX_CALLBACK helpCallback,
                const char* aliasBtnNames, const char* url, const char* urlText);

    ~CMessageBox();

    virtual void Transfer(CTransferInfo& ti);

    int Execute();

    BOOL CopyToClipboard();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    BOOL EscapeEnabled();
};

//
// ****************************************************************************

class CChangeAttrDialog : public CCommonDialog
{
private:
    // handly pro TimeDate controly
    HWND HModifiedDate;
    HWND HModifiedTime;
    HWND HCreatedDate;
    HWND HCreatedTime;
    HWND HAccessedDate;
    HWND HAccessedTime;

    // stavove promenne pro disableni checkboxu
    BOOL SelectionContainsDirectory;
    BOOL FileBasedCompression;
    BOOL FileBasedEncryption;

    // pokud user kliknul na prislusny checkbox, bude promenna nastavena na TRUE
    BOOL ArchiveDirty;
    BOOL ReadOnlyDirty;
    BOOL HiddenDirty;
    BOOL SystemDirty;
    BOOL CompressedDirty;
    BOOL EncryptedDirty;

public:
    int Archive,
        ReadOnly,
        Hidden,
        System,
        Compressed,
        Encrypted,
        ChangeTimeModified,
        ChangeTimeCreated,
        ChangeTimeAccessed,
        RecurseSubDirs;

    SYSTEMTIME TimeModified;
    SYSTEMTIME TimeCreated;
    SYSTEMTIME TimeAccessed;

    CChangeAttrDialog(HWND parent, DWORD attr, DWORD attrDiff,
                      BOOL selectedDirectory, BOOL fileBasedCompression,
                      BOOL fileBasedEncryption,
                      const SYSTEMTIME* timeModified,
                      const SYSTEMTIME* timeCreated,
                      const SYSTEMTIME* timeAccessed);

    virtual void Transfer(CTransferInfo& ti);

    void EnableWindows();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL GetAndValidateTime(CTransferInfo* ti, int resIDDate, int resIDTime, SYSTEMTIME* time);
};

//******************************************************************************
//
// CProgressDlgArray
//

struct CProgressDlgArrItem
{
    HANDLE DlgThread; // handle threadu dialogu (muze byt i handle ukonceneho threadu dialogu)
    HWND DlgWindow;   // handle okna dialogu (NULL = dialog jiz byl zavren)

    CProgressDlgArrItem()
    {
        DlgThread = NULL;
        DlgWindow = NULL;
    }
};

class CProgressDlgArray
{
protected:
    CRITICAL_SECTION Monitor;                 // sekce pouzita pro synchronizaci tohoto objektu (chovani - monitor)
    TIndirectArray<CProgressDlgArrItem> Dlgs; // pole dialogu operaci (jen dialogy bezici ve svych threadech)

public:
    CProgressDlgArray();
    ~CProgressDlgArray();

    // alokuje a vlozi do pole strukturu pro novy dialog (data se plni az venku);
    // pri nedostatku pameti vraci NULL, jinak vraci pozadovanou strukturu
    // volat jen z hl. threadu (jinak hrozi konflikt s cistenim dokoncenych threadu)
    CProgressDlgArrItem* PrepareNewDlg();

    // v kriticke sekci pole nastavi data v 'dlg' podle 'dlgThread' a 'dlgWindow';
    // data se meni jen na hodnoty ruzne od NULL (parametr NULL = bez zmeny)
    void SetDlgData(CProgressDlgArrItem* dlg, HANDLE dlgThread, HWND dlgWindow);

    // odstrani strukturu 'dlg' z pole; 'dlg->DlgThread' musi byt NULL; volat jen
    // pokud se nepodari start dialogu, pro nejz byl 'dlg' ziskan pres metodu
    // PrepareNewDlg()
    void RemoveDlg(CProgressDlgArrItem* dlg);

    // odstrani z pole vsechny dialogy, jejichz thready uz se dokoncili (handly techto
    // threadu zavre); vraci pocet dosud bezicich threadu dialogu operaci (takze az vrati
    // nulu, je napr. mozne ukoncit Salamandera)
    int RemoveFinishedDlgs();

    // najde v poli dialog s oknem 'hdlg' a ulozi do jeho 'DlgWindow' NULL (dialog
    // timto zpusobem hlasi, ze se zavira; thread dialogu by se mel ukoncit vzapeti)
    void ClearDlgWindow(HWND hdlg);

    // vraci dalsi otevreny dialog; pokud zadny dialog neni otevreny, vraci NULL;
    // 'index' pred prvnim volanim inicializovat na 0, pro dalsi volani pouzit
    // vracenou hodnotu 'index' (nesahat na 'index' mezi volanimi); vraci dialogy
    // cyklicky (po poslednim v rade se vrati k prvnimu)
    HWND GetNextOpenedDlg(int* index);

    // zajisti zavreni vsech otevrenych dialogu;
    // volat jen z hl. threadu (jinak hrozi otevreni dalsiho dialogu, ktery se prirozene
    // nedozvi, ze se ma ukoncit)
    void PostCancelToAllDlgs();

    // rozeslem vsem dialogum zpravu, ze doslo ke zmene (barvy) ikonky a je treba ji
    // znovu nastavit; volat jen z hl. threadu
    void PostIconChange();
};

//******************************************************************************
//
// CProgressDialog
//

struct CChangeAttrsData;
struct CConvertData;
class COperations;
class CStaticText;
class CProgressBar;
struct CStartProgressDialogData;

// vraci FALSE pokud se nepodarilo v novem threadu otevrit progress dialog nebo
// pokud se v tomto dialogu nepodarilo spustit thread workera operace; pokud
// vraci FALSE, musi volajici uvolnit skript 'script' (jinak se skript uvolnuje
// az po dobehnuti operace v threadu workera)
BOOL StartProgressDialog(COperations* script, const char* caption,
                         CChangeAttrsData* attrsData, CConvertData* convertData);

class CProgressDialog : public CCommonDialog
{
public:
    CProgressDialog(HWND parent, COperations* script, const char* caption,
                    CChangeAttrsData* attrsData, CConvertData* convertData,
                    BOOL runningInOwnThread, CStartProgressDialogData* progrDlgData);
    ~CProgressDialog();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL FlushCachedData(); // posle zmenena data staticum a progress baram; vraci TRUE pokud bylo co updatnout (neco bylo dirty)

    void SetDlgTitle(BOOL minimized);
    void SetWindowIcon();

protected:
    BOOL RunningInOwnThread;                // TRUE/FALSE = dialog bezi ve svem threadu ("na pozadi") / dialog bezi v hl. threadu a je modalni k parentovi (vetsinou k jednomu z panelu)
    CStartProgressDialogData* ProgrDlgData; // neni NULL jen pokud dialog bezi ve svem threadu a jeste nebyl pusten dale hl. thread (ceka na otevreni dialogu a start operace)

    HANDLE Worker;                // thread workera prislusiciho k tomuto dialogu (NULL pokud tento thread jeste/uz neexistuje)
    HANDLE WContinue;             // viceucelovy event
    HANDLE WorkerNotSuspended;    // non-signaled == Worker ma prejit do suspend-modu
    BOOL CancelWorker;            // je-li TRUE, thread workera se ukonci
    int OperationProgress;        // hodnota progressu sdilena s threadem workera
    int SummaryProgress;          // hodnota progressu sdilena s threadem workera
    BOOL ShowPause;               // tlacitko "pause" ma mit text: TRUE = pause, FALSE = resume
    BOOL IsInQueue;               // TRUE = operace je ve fronte (user si to pral + povedlo se ji tam pridat)
    BOOL AutoPaused;              // TRUE pokud je operace ve fronte a je kvuli tomu "paused"
    BOOL StatusPaused;            // TRUE = operace je zastavena, napr. dotaz na Cancel operace (+ostatni dialogy)
    DWORD NextTimeLeftUpdateTime; // cas dalsiho povoleneho updatu time-left (caste updaty jsou u delsich casu na skodu)
    CQuadWord TimeLeftLastValue;  // posledni zobrazena hodnota time-left
    CITaskBarList3 TaskBarList3;  // pro ovladani progress na taskbar od W7

    CProgressBar *Operation,
        *Summary;
    CStaticText *OperationText,
        *Source,
        *Target,
        *Status;
    COperations* Script;
    char Caption[50];
    CChangeAttrsData* AttrsData;
    CConvertData* ConvertData;
    BOOL AcceptCommands;

    HWND HPreposition;

    BOOL CanClose; // zamezime nechtenemu zavreni (uvnitr metody tohoto objektu)

    BOOL TimerIsRunning; // pokud je TRUE, bezi timer pro zobrazovani zmen textu a progress bar

    BOOL FirstUserSetDialog; // TRUE = jeste nebyla zpracovana zprava WM_USER_SETDIALOG (u prvni forcneme prekresleni, aby user videl dialog aspon probliknout)

    HWND NextForegroundWindow; // okno, ktere by melo byt foreground po zavreni tohoto dialogu (na XP se servis packem 1 a otevrenym top-most oknem blbla aktivace okna po zavreni dialogu - aktivovalo se top-most okno misto hlavniho okna Salama)

    BOOL DoNotBeepOnClose; // TRUE = operace se cancluje kvuli exitu Salama, zadny beep nema smysl

    // texty ukladame do cache, vykreslujeme je az na timer
    BOOL CacheIsDirty;
    char OperationCache[100];
    char PrepositionCache[100];
    char SourceCache[2 * MAX_PATH];
    char TargetCache[2 * MAX_PATH];

    // hodnoty ukladame, vykreslujeme az na timer
    BOOL OperationProgressCacheIsDirty;
    int OperationProgressCache;
    BOOL SummaryProgressCacheIsDirty;
    int SummaryProgressCache;
};

//
// ****************************************************************************

class CFileErrorDlg : public CCommonDialog
{
public:
    CFileErrorDlg(HWND parent, const char* caption, const char* file, const char* error,
                  BOOL noSkip = FALSE, int altRes = 0);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *Caption,
        *File,
        *Error;
};

//
// ****************************************************************************

class CErrorReadingADSDlg : public CCommonDialog
{
public:
    CErrorReadingADSDlg(HWND parent, const char* file, const char* error, const char* title = NULL);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *File,
        *Error,
        *Title;
};

//
// ****************************************************************************

class CErrorSettingAttrsDlg : public CCommonDialog
{
public:
    CErrorSettingAttrsDlg(HWND parent, const char* file, DWORD neededAttrs, DWORD currentAttrs);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* File;
    DWORD NeededAttrs;
    DWORD CurrentAttrs;
};

//
// ****************************************************************************

class CErrorCopyingPermissionsDlg : public CCommonDialog
{
public:
    CErrorCopyingPermissionsDlg(HWND parent, const char* sourceFile,
                                const char* targetFile, DWORD error);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* SourceFile;
    const char* TargetFile;
    DWORD Error;
};

//
// ****************************************************************************

class CErrorCopyingDirTimeDlg : public CCommonDialog
{
public:
    CErrorCopyingDirTimeDlg(HWND parent, const char* targetFile, DWORD error);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* TargetFile;
    DWORD Error;
};

//
// ****************************************************************************

class COverwriteDlg : public CCommonDialog
{
public:
    COverwriteDlg(HWND parent, const char* sourceName, const char* sourceAttr,
                  const char* targetName, const char* targetAttr, BOOL yesnocancel = FALSE,
                  BOOL dirOverwrite = FALSE);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *SourceName,
        *SourceAttr,
        *TargetName,
        *TargetAttr;
};

//
// ****************************************************************************

class CHiddenOrSystemDlg : public CCommonDialog
{
public:
    CHiddenOrSystemDlg(HWND parent, const char* caption, const char* name,
                       const char* error, BOOL yesnocancel = FALSE, BOOL yesallcancel = FALSE);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char *Caption,
        *Name,
        *Error;
};

//
// ****************************************************************************

class CConfirmADSLossDlg : public CCommonDialog
{
public:
    CConfirmADSLossDlg(HWND parent, BOOL isFile, const char* name, const char* streams, BOOL isMove);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Name;
    const char* Streams;
    BOOL IsFile;
    BOOL IsMove;
};

//
// ****************************************************************************

class CConfirmLinkTgtCopyDlg : public CCommonDialog
{
public:
    CConfirmLinkTgtCopyDlg(HWND parent, const char* name, const char* details);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Name;
    const char* Details;
};

//
// ****************************************************************************

class CConfirmEncryptionLossDlg : public CCommonDialog
{
public:
    CConfirmEncryptionLossDlg(HWND parent, BOOL isFile, const char* name, BOOL isMove);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Name;
    BOOL IsFile;
    BOOL IsMove;
};

//
// ****************************************************************************

class CCannotMoveDlg : public CCommonDialog
{
public:
    CCannotMoveDlg(HWND parent, int resID, char* sourceName, char* targetName,
                   char* error);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    char *SourceName,
        *TargetName,
        *Error;
};

//
// ****************************************************************************

class CSizeResultsDlg : public CCommonDialog
{
public:
    CSizeResultsDlg(HWND parent, const CQuadWord& size, const CQuadWord& compressed,
                    const CQuadWord& occupied, int files, int dirs,
                    TDirectArray<CQuadWord>* sizes);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void UpdateEstimate();

    CQuadWord Size, Compressed, Occupied;
    int Files, Dirs;
    TDirectArray<CQuadWord>* Sizes;
    char UnknownText[100];
};

//
// ****************************************************************************

class CColorGraph;

class CDriveInfo : public CCommonDialog
{
protected:
    char VolumePath[MAX_PATH]; // k jakemu drivu se maji zobrazit informace (bud root nebo junction-point)
    char OldVolumeName[1000];  // pro detekci zmeny
    CColorGraph* Graph;
    HICON HDriveIcon;

public:
    CDriveInfo(HWND parent, const char* path, CObjectOrigin origin = ooStandard);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void GrowWidth(int resID, int& width);
};

//
// ****************************************************************************

class CChangeCaseDlg : public CCommonDialog
{
private:
    BOOL SelectionContainsDirectory;

public:
    int FileNameFormat; // cisla kompatibilni s funkci AlterFileName
    int Change;         // jaka cast jmena se ma menit  --||--
    BOOL SubDirs;       // vcetne podadresaru?

    CChangeCaseDlg(HWND parent, BOOL selectionContainsDirectory);

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************

class CConvertFilesDlg : public CCommonDialog
{
private:
    BOOL SelectionContainsDirectory;

public:
    char Mask[MAX_PATH]; // ktere soubory budeme konvertovat?
    int Change;          // ktery prevod se ma provest?
    BOOL SubDirs;        // vcetne podadresaru?
    int CodeType;        // ktere kodovani je vybrane (0 = nic)
    int EOFType;         // ktere konce radku jsou vybrane (0 = nic)
                         // 1 = CRLF
                         // 2 = LF
                         // 3 = CR

    CConvertFilesDlg(HWND parent, BOOL selectionContainsDirectory);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void UpdateCodingText();
    //    void UpdateEOFText();

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CFilterDialog : public CCommonDialog
{
protected:
    CMaskGroup* Filter;
    BOOL* UseFilter;
    //    BOOL       *Inverse;
    char** FilterHistory;

public:
    CFilterDialog(HWND parent, CMaskGroup* filter, char** filterHistory,
                  BOOL* use /*, BOOL *inverse*/);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

class CSetSpeedLimDialog : public CCommonDialog
{
protected:
    BOOL* UseSpeedLim; // TRUE = zapnuty speed-limit
    DWORD* SpeedLimit; // speed-limit v bytech za vterinu

public:
    CSetSpeedLimDialog(HWND parent, BOOL* useSpeedLim, DWORD* speedLimit);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

#define USERNAME_MAXLEN (256 + 1 + 256 + 1) // user + '@' + domain + '\0'
#define PASSWORD_MAXLEN (256 + 1)
#define DOMAIN_MAXLEN (256 + 1)

class CEnterPasswdDialog : public CCommonDialog
{
public:
    char Passwd[PASSWORD_MAXLEN];
    char User[USERNAME_MAXLEN];

    CEnterPasswdDialog(HWND parent, const char* path, const char* user, CObjectOrigin origin = ooStandard);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    const char* Path;
};

//
// ****************************************************************************

class CChangeDirDlg : public CCommonDialog
{
public:
    CChangeDirDlg(HWND parent, char* path, BOOL* sendDirectlyToPlugin);

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    char* Path;
    BOOL* SendDirectlyToPlugin;
};

//
// ****************************************************************************

class CPackerConfig;

class CPackDialog : public CCommonDialog
{
public:
    CPackDialog(HWND parent, char* path, const char* pathAlt,
                CTruncatedString* subject, CPackerConfig* config);

    virtual void Transfer(CTransferInfo& ti);

    void SetSelectionEnd(int selectionEnd);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // v bufferu 'name' zmeni priponu na 'ext'; vrati TRUE, pokud se to povedlo
    BOOL ChangeExtension(char* name, const char* ext);

    char* Path;
    const char* PathAlt;
    CTruncatedString* Subject;
    CPackerConfig* PackerConfig;
    int SelectionEnd;
};

//
// ****************************************************************************

class CUnpackerConfig;

class CUnpackDialog : public CCommonDialog
{
public:
    CUnpackDialog(HWND parent, char* path, const char* pathAlt, char* mask,
                  CTruncatedString* subject, CUnpackerConfig* config,
                  BOOL* delArchiveWhenDone);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableDelArcCheckbox();

    char *Mask,
        *Path;
    const char* PathAlt;
    CTruncatedString* Subject;
    CUnpackerConfig* UnpackerConfig;
    BOOL* DelArchiveWhenDone;
};

//
// ****************************************************************************

class CZIPSizeResultsDlg : public CCommonDialog
{
public:
    CZIPSizeResultsDlg(HWND parent, const CQuadWord& size, int files, int dirs);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    CQuadWord Size;
    int Files, Dirs;
};

//
// ****************************************************************************

class CSplashScreen : public CDialog
{
public:
    CSplashScreen();
    ~CSplashScreen();

    BOOL PaintText(const char* text, int x, int y, BOOL bold, COLORREF clr);
    void SetText(const char* text);
    int GetWidth() { return Width; }
    int GetHeight() { return Height; }

    BOOL PrepareBitmap();
    void DestroyBitmap();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    CBitmap* Bitmap;         // vcetne textu
    CBitmap* OriginalBitmap; // pouze grafika bez textu
    HFONT HNormalFont;
    HFONT HBoldFont;
    RECT OpenSalR;
    RECT VersionR;
    RECT CopyrightR;
    RECT StatusR;
    int GradientY;
    int Width;
    int Height;
};

//
// ****************************************************************************

class CAboutDialog : public CCommonDialog
{
protected:
    HFONT HSmallFont;
    HBRUSH HGradientBkBrush;
    CBitmap* BackgroundBitmap;

public:
    CAboutDialog(HWND parent);
    ~CAboutDialog();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

struct CExecuteItem;
class CComboboxEdit;

class CFileListDialog : public CCommonDialog
{
protected:
    CComboboxEdit* EditLine;

public:
    CFileListDialog(HWND parent);
    ~CFileListDialog();

    virtual void Validate(CTransferInfo& ti);
    void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

#ifdef USE_BETA_EXPIRATION_DATE

class CBetaExpiredDialog : public CCommonDialog
{
protected:
    int Count;
    char OldOK[30];

public:
    CBetaExpiredDialog(HWND parent);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnTimer();
};

#endif // USE_BETA_EXPIRATION_DATE

//
// ****************************************************************************

class CTaskListDialog : public CCommonDialog
{
protected:
    DWORD DisplayedVersion; // zobrazena verze seznamu

public:
    CTaskListDialog(HWND parent);

protected:
    void Refresh();
    DWORD GetCurPID(); // vraci PID vybrany v listboxu

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CChangeIconDialog : public CCommonDialog
{
protected:
    // pro transfer dat z/do dialogu
    char* IconFile;
    int* IconIndex;
    BOOL Dirty;
    HICON* Icons;     // pole handlu enumerovanych ikon
    DWORD IconsCount; // pocet ikon v poli

public:
    CChangeIconDialog(HWND hParent, char* iconFile, int* iconIndex);
    ~CChangeIconDialog();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void GetShell32(char* fileName);

    BOOL LoadIcons();    // provede enumeraci ikon a naleje je do pole Icons
    void DestroyIcons(); // vycisti pole Icons
};

//
// ****************************************************************************

class CToolbarHeader;
struct CPluginData;
class CHyperLink;

class CPluginsDlg : public CCommonDialog
{
protected:
    HWND HListView; // nase listview
    CToolbarHeader* Header;
    HIMAGELIST HImageList;      // imagelist pro listview
    BOOL RefreshPanels;         // bude po zavreni dialogu treba refresnout panely?
    BOOL DrivesBarChange;       // bude po zavreni dialogu treba refreshnout Drives bary?
    char FocusPlugin[MAX_PATH]; // prazdny, pokud se nema focusnout plugin; jinak obsahuje cestu
    CHyperLink* Url;
    char ShowInBarText[200];        // text vytazeny z checkboxu pri otevreni dialogu
    char ShowInChDrvText[200];      // text vytazeny z checkboxu pri otevreni dialogu
    char InstalledPluginsText[200]; // text vytazeny z titulku listview pri otevreni dialogu

public:
    CPluginsDlg(HWND hParent);

    // navratove hodnoty dialogu:
    BOOL GetRefreshPanels() { return RefreshPanels; }
    BOOL GetDrivesBarChange() { return DrivesBarChange; }
    const char* GetFocusPlugin() { return FocusPlugin; }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void InitColumns();     // do listview prida sloupce
    void SetColumnWidths(); // nastavi optimalni sirky sloupcu
    void RefreshListView(BOOL setOnly = TRUE, int selIndex = -1, const CPluginData* selectPlugin = NULL, BOOL setColumnWidths = FALSE);
    void OnSelChanged();                                                    // vybrana polozka v listview se zmenila
    CPluginData* GetSelectedPlugin(int* index = NULL, int* lvIndex = NULL); // vraci NULL, pokud neni zadna polozka vybrana; index vraci index do pole Plugins; lvIndex vraci index v ramci listview, muze byt NULL
    void EnableButtons(CPluginData* plugin);
    void OnContextMenu(int x, int y); // na souradnicich x, y vybali kontextove menu pro vybranou polozku
    void OnMove(BOOL up);
    void OnSort();
    void EnableHeader();
};

//
// ****************************************************************************

struct CPluginMenuItem;

class CPluginKeys : public CCommonDialog
{
public:
    BOOL Reset; // navratovka (TRUE pokud byl dialog ukoncen pres Reset)

protected:
    HWND HListView; // nase listview
    CToolbarHeader* Header;
    CPluginData* Plugin;
    DWORD* HotKeys; // nase kopie hot keys (abychom mohli cancelnout)

public:
    CPluginKeys(HWND hParent, CPluginData* plugin);
    ~CPluginKeys();
    BOOL IsGood();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void InitColumns();     // do listview prida sloupce
    void SetColumnWidths(); // nastavi optimalni sirky sloupcu
    void RefreshListView(BOOL setOnly = TRUE);

    WORD GetHotKey(BYTE* virtKey = NULL, BYTE* mods = NULL);
    CPluginMenuItem* GetSelectedItem(int* orgIndex); // orgIndex vrati index do pole Plugin->MenuItems; muze byt NULL
    CPluginMenuItem* GetItem(int index);
    void EnableButtons();
    void HandleConflictWarning();

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************

class CFileTimeStamps;
class CFilesWindow;
class CArchiveUpdateDlg : public CCommonDialog
{
protected:
    CFileTimeStamps* FileStamps;
    CFilesWindow* Panel;

public:
    CArchiveUpdateDlg(HWND hParent, CFileTimeStamps* fileStamps, CFilesWindow* panel);

    void EnableButtons();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CWaitWindow
//
// Modalni okenko slouzici k zobrazeni informace o probihajicim deji.
// Text muze byt viceradkovy, oddeleny znakem '\n'.
// 'showCloseButton' urcuje, zda bude okenko obsahovat Close tlacitko,
// ktere by melo byt ekvivalent k Escape, ale pro mysare.
//

class CWaitWindow : public CWindow
{
protected:
    char* Caption;
    char* Text;
    SIZE TextSize;
    HWND HParent;
    HWND HForegroundWnd;
    BOOL ShowCloseButton;
    BOOL NeedWrap;

    BOOL ShowProgressBar;
    RECT BarRect;
    DWORD BarMax;
    DWORD BarPos;

    CBitmap* CacheBitmap; // pro neblikajici kresleni textu

public:
    CWaitWindow(HWND hParent, int textResID, BOOL showCloseButton, CObjectOrigin origin = ooAllocated, BOOL showProgressBar = FALSE);
    ~CWaitWindow();

    void SetCaption(const char* text); // pokud nebude zavolano, caption bude "Open Salamander"
    void SetText(const char* text);

    void SetProgressMax(DWORD max);
    void SetProgressPos(DWORD pos);

    // pokud je hForegroundWnd ruzny od NULL, bude k nemu okenko centrovano,
    // ale nebude zobrazeno (zobrazuje se v ThreadSafeWaitWindowFBody)
    HWND Create(HWND hForegroundWnd = NULL);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void PaintProgressBar(HDC dc);
    void PaintText(HDC hDC);
};

//****************************************************************************
//
// CTipOfTheDayWindow a CTipOfTheDayDialog
//
//
/*
class CTipOfTheDayDialog;

class CTipOfTheDayWindow: public CWindow
{
  protected:
    HFONT HHeadingFont;
    HFONT HBodyFont;
    CTipOfTheDayDialog *Parent;

  public:
    CTipOfTheDayWindow();
    ~CTipOfTheDayWindow();

    void PaintBodyText(HDC hDC = NULL);

  protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

  friend class CTipOfTheDayDialog;
};

class CTipOfTheDayDialog: public CCommonDialog
{
  protected:
    CTipOfTheDayWindow  TipWindow;
    TDirectArray<DWORD> Tips;

  public:
    CTipOfTheDayDialog(BOOL quiet);
    ~CTipOfTheDayDialog();

    BOOL IsGood() {return Tips.Count > 0;}
    void IncrementTipIndex();
    void InvalidateTipWindow() {InvalidateRect(TipWindow.HWindow, NULL, FALSE);}

  protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void Transfer(CTransferInfo &ti);

    BOOL LoadTips(BOOL quiet);  // nacucne ze souboru tipy do pole Tips
    void FreeTips();  // uvolni pole

  friend class CTipOfTheDayWindow;
};
*/
//****************************************************************************
//
// CImportConfigDialog
//

struct CImportOldKey
{
    char* SalamanderVersion;
    char* SalamanderPath;
};

class CImportConfigDialog : public CCommonDialog
{
public:
    // pole odpovidajici poli SalamanderConfigurationRoots; TRUE:konfigurace existuje FALSE:neexistuje
    BOOL ConfigurationExist[SALCFG_ROOTS_COUNT];
    // ukazatel na stejne velke pole, kam dialog nastavi TRUE pro konfigurace urcene ke smazani
    BOOL* DeleteConfigurations;
    // v teto promenne dialog vraci kterou konfiguraci si uzivatel preje importovat; -1 -> zadnou
    // index je do pole SalamanderConfigurationRoots
    int IndexOfConfigurationToLoad;

public:
    CImportConfigDialog();
    ~CImportConfigDialog();

protected:
    virtual void Transfer(CTransferInfo& ti);
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CLanguageSelectorDialog
//

class CHyperLink;

class CLanguageSelectorDialog : public CCommonDialog
{
protected:
    TDirectArray<CLanguage> Items;
    CHyperLink* Web;
    char* SLGName;
    BOOL OpenedFromConfiguration;
    BOOL OpenedForPlugin;
    HWND HListView;
    const char* PluginName;
    char ExitButtonLabel[100];

public:
    CLanguageSelectorDialog(HWND hParent, char* slgName, const char* pluginName);
    ~CLanguageSelectorDialog();

    int Execute();

    // obejde adresar lang a vsechny platne SLG soubory prida do pole
    BOOL Initialize(const char* slgSearchPath = NULL, HINSTANCE pluginDLL = NULL);

    int GetLanguagesCount() { return Items.Count; }
    BOOL GetSLGName(char* path, int index = 0); //vrati xxxx.slg od polozky na indexu 'index'
    BOOL SLGNameExists(const char* slgName);    // overi jestli je 'slgName' v 'Items'

    void FillControls();

    void LoadListView();

    void Transfer(CTransferInfo& ti);

    // vrati index do pole Items; nejvyssi prioritu ma 'selectSLGName' (neni-li NULL), pak
    // nastaveni Windows, a je-li 'exactMatch' FALSE, tak pak jeste english.slg a jinak prvni
    // nalezene .slg; je-li 'exactMatch' TRUE, vraci index 'selectSLGName' (neni-li NULL)
    // nebo nastaveni Windows nebo -1 (nenalezeno)
    int GetPreferredLanguageIndex(const char* selectSLGName, BOOL exactMatch = FALSE);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CConversionTablesDialog
//

class CConversionTablesDialog : public CCommonDialog
{
protected:
    HWND HListView;
    char* DirName;

public:
    CConversionTablesDialog(HWND parent, char* dirName);
    ~CConversionTablesDialog();

    void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CSkillLevelDialog
//

class CSkillLevelDialog : public CCommonDialog
{
protected:
    int* Level;

public:
    CSkillLevelDialog(HWND hParent, int* level);

    void Transfer(CTransferInfo& ti);
};

//****************************************************************************
//
// CSharesDialog
//

class CSharesDialog : public CCommonDialog
{
protected:
    HWND HListView;
    CShares SharedDirs; // radeji podrzime vlastni instanci, aby nam s nima
                        // nekdo nerefreshnul na pozadi
    BYTE SortBy;        // udava sloupec, podle ktereho je razeno
    int FocusedIndex;   // slouzi pro navrat hodnoty

public:
    CSharesDialog(HWND hParent);

    const char* GetFocusedPath(); // vrati cestu vybraneho sharu; lze volat az po navratu z dialogu
                                  // vrati NULL, pokud nebylo stisteno "Focus" a dialog vratil IDOK
                                  // z Execute()

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    HIMAGELIST CreateImageList(); // vytvori image list s jednou ikonou (sdileny adresar)

    void InitColumns(); // nahazi sloupce do ListView
    void Refresh();     // nacte sdilene adresare a prida je do listview
    static int CALLBACK SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
    void SortItems();                        // seradi polozky na zaklade promenne SortBy
    int GetFocusedIndex();                   // vrati index do pole SharetDirs nebo -1, pokud neni zadna polozka vybrana
    void DeleteShare(const char* shareName); // ukeca system, aby zrusil share
    void OnContextMenu(int x, int y);        // na souradnicich x, y vybali kontextove menu pro vybranou polozku
    void EnableControls();                   // enabler pro tlacitka
};

//****************************************************************************
//
// CDisconnectDialog
//

// typy pro CConnectionItem
enum CConnectionItemType
{
    citGroup,   // oznacuje nasledujici skupinu (sitove zdroje, pluginy, ...)
    citNetwork, // jde o sitovy zdroj
    citPlugin   // jde o connection pluginu
};

#define CONNECTION_ICON_NETWORK 0
#define CONNECTION_ICON_PLUGIN 1
#define CONNECTION_ICON_ACCESSIBLE 2
#define CONNECTION_ICON_INACCESSIBLE 3

// jednotlive polozky zobrazovane v Disconnect dialogu; plni se v EnumConnections()
struct CConnectionItem
{
    CConnectionItemType Type;
    int IconIndex; // index ikonky v HImageList (CONNECTION_ICON_xxx)
    char* Name;    // sloupec Name
    char* Path;    // sloupec Path
    BOOL Default;  // pokud je TRUE, bude polozka po naplneni listview FOCUSED+SELECTED; pouze jedna polozka v poli by mela mit Default==TRUE

    // jen pro Type == citPlugin: interface FS (nemusi byt platny, overovat)
    CPluginFSInterfaceAbstract* PluginFS;
};

class CDisconnectDialog : public CCommonDialog
{
protected:
    CFilesWindow* Panel; // panel, ze ktereho byl vyvolan Disconnect a ze ktereho cerpame defualt cestu
    HWND HListView;
    HIMAGELIST HImageList;
    BOOL NoConncection;
    TDirectArray<CConnectionItem> Connections;

public:
    CDisconnectDialog(CFilesWindow* panel);
    ~CDisconnectDialog();

    const char* GetFocusedPath();                 // vrati cestu vybraneho sharu; lze volat az po navratu z dialogu
                                                  // vrati NULL, pokud nebylo stisteno "Focus" a dialog vratil IDOK
                                                  // z Execute()
    BOOL NoConnection() { return NoConncection; } // vrati TRUE, pokud dialog nebyl otevren, protoze nebyla zadna connection

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Validate(CTransferInfo& ti);

    HIMAGELIST CreateImageList(); // vytvori image list 0-pristupny 1-nepristupny network drive

    void EnumConnections(); // naplni pole Connections

    void InitColumns(); // nahazi sloupce do ListView
    void Refresh();

    // odpoji SELECTED polozky; vraci TRUE pokud vse probehlo dobre a cely dialog se muze zavrit
    // pokud nastane chyba, zobrazi messagebox a vrati FALSE; dialog se v tomto pripade nezavre
    BOOL OnDisconnect();

    void EnableControls(); // enabler pro tlacitka

    // vlozi novou polozku do pole Connections
    // pokud je index == -1, vlozi prvek na konec seznamu
    // pokud je ignoreDuplicate, prohleda se napred pole a pokud stejny prvek existuje, neprida se
    BOOL InsertItem(int index, BOOL ignoreDuplicate, CConnectionItemType type, int iconIndex,
                    const char* name, const char* path, BOOL defaultItem,
                    CPluginFSInterfaceAbstract* pluginFS);

    void DestroyConnections(); // vyprazdni/uvolni pole Connections
};

//****************************************************************************
//
// CSaveSelectionDialog
//

class CSaveSelectionDialog : public CCommonDialog
{
public:
    CSaveSelectionDialog(HWND hParent, BOOL* clipboard);

    virtual void Transfer(CTransferInfo& ti);

protected:
    BOOL* Clipboard;
};

//****************************************************************************
//
// CLoadSelectionDialog
//

enum CLoadSelectionOperation
{
    lsoCOPY,
    lsoOR,
    lsoDIFF,
    lsoAND
};

class CLoadSelectionDialog : public CCommonDialog
{
public:
    CLoadSelectionDialog(HWND hParent, CLoadSelectionOperation* operation, BOOL* clipboard,
                         BOOL clipboardValid, BOOL globalValid);

    virtual void Transfer(CTransferInfo& ti);

protected:
    CLoadSelectionOperation* Operation;
    BOOL* Clipboard;
    BOOL ClipboardValid;
    BOOL GlobalValid;
};

//****************************************************************************
//
// CCompareDirsDialog
//

class CCompareDirsDialog : public CCommonDialog
{
protected:
    BOOL EnableByDateAndTime;
    BOOL EnableBySize;
    BOOL EnableByAttrs;
    BOOL EnableByContent;
    BOOL EnableSubdirs;
    BOOL EnableCompAttrsOfSubdirs;
    CFilesWindow* LeftPanel;
    CFilesWindow* RightPanel;

    int OriginalWidth;    // plna sirka dialogu
    int OriginalHeight;   // plna vyska dialogu
    int OriginalButtonsY; // Y pozice tlacitek v client souradnicich
    int SpacerHeight;     // vymezovac pro zmensovani/zvetsovani dialogu
    BOOL Expanded;        // jsme prave rozbaleni?

public:
    CCompareDirsDialog(HWND hParent, BOOL enableByDateAndTime, BOOL enableBySize,
                       BOOL enableByAttrs, BOOL enableByContent, BOOL enableSubdirs,
                       BOOL enableCompAttrsOfSubdirs, CFilesWindow* leftPanel,
                       CFilesWindow* rightPanel);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
    void DisplayMore(BOOL more);
    HDWP OffsetControl(HDWP hdwp, int id, int yOffset);
};

//****************************************************************************
//
// CCmpDirProgressDialog
//

class CCmpDirProgressDialog : public CCommonDialog
{
protected:
    BOOL HasProgress;
    BOOL Cancel;

    // dialog controls
    CStaticText* Source;
    CStaticText* Target;
    CProgressBar* Progress;
    CProgressBar* TotalProgress;

    char DelayedSource[2 * MAX_PATH]; // text s odlozenym zobrazenim
    char DelayedTarget[2 * MAX_PATH]; // text s odlozenym zobrazenim
    BOOL DelayedSourceDirty;
    BOOL DelayedTargetDirty;

    // progress s odlozenym zobrazenim
    BOOL SizeIsDirty;
    CQuadWord FileSize;
    CQuadWord ActualFileSize;
    CQuadWord TotalSize;
    CQuadWord ActualTotalSize;

    DWORD LastTickCount; // pro detekci ze uz je treba prekreslit zmena data

    CITaskBarList3* TaskBarList3; // ukazatel na interface patrici hlavnimu oknu Salamandera

public:
    CCmpDirProgressDialog(HWND hParent, BOOL hasProgress, CITaskBarList3* taskBarList3); //pokud je 'hasProgress' TRUE, pouzije se dialog s progress bar

    // nastaveni textu
    void SetSource(const char* text);
    void SetTarget(const char* text);

    // nasledujici tri metody maji vyznam je-li zobrazen progress bar

    // nastavi celkovou velikost jednoho souboru
    void SetFileSize(const CQuadWord& size);
    // nastavi absolutni progress
    void SetActualFileSize(const CQuadWord& size);
    // nastavi celkovou velikost vsech souboru
    void SetTotalSize(const CQuadWord& size);
    // nastavi aktualni celkovou velikost vsech souboru
    void SetActualTotalSize(const CQuadWord& size);
    // vytahne aktualni celkovou velikost vsech souboru
    void GetActualTotalSize(CQuadWord& size);
    // relativne zmeni progress
    void AddSize(const CQuadWord& size);

    // distribuuje zpravy, vraci FALSE pokud uzivatel stornoval operaci
    BOOL Continue();

    void FlushDataToControls(); // preda ulozene hodnoty controlum k zobrazeni

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CExitingOpenSal
//

class CExitingOpenSal : public CCommonDialog
{
protected:
    int NextOpenedDlgIndex;

public:
    CExitingOpenSal(HWND hParent);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CDriveSelectErrDlg : public CCommonDialog
{
public:
    CDriveSelectErrDlg(HWND parent, const char* errText, const char* drvPath);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    const char* ErrText;
    char DrvPath[MAX_PATH];
    int CounterForAllowedUseOfTimer;
};

//
// ****************************************************************************

class CCompareArgsDlg : public CCommonDialog
{
public:
    CCompareArgsDlg(HWND parent, BOOL comparingFiles, char* compareName1,
                    char* compareName2, int* cnfrmShowNamesToCompare);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    BOOL ComparingFiles;
    char* CompareName1;
    char* CompareName2;
    int* CnfrmShowNamesToCompare;
};

extern CProgressDlgArray ProgressDlgArray; // pole dialogu diskovych operaci (jen dialogy bezici ve svych threadech)
