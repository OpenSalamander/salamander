// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Salamander Plugin Development Framework
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	fxfs.cpp
	Contains classes for developing Salamander file system plugins.
*/

#include "precomp.h"
#include "fxfs.h"
#include "fx_lang.rh"

namespace Fx
{

    const CFxPath::XCHAR CFxStandardPathTraits::RootPath[] = {CFxStandardPathTraits::PathSeparator, 0};

    void WINAPI FxPathAltSepToSepAndRemoveDups(
        CFxPath::XCHAR* path,
        CFxPath::XCHAR sep,
        CFxPath::XCHAR altSep)
    {
        CFxPath::XCHAR* s = path;
        while (*s != TEXT('\0'))
        {
            if (*s == altSep)
            {
                *s = sep;
            }

            if (*s == sep && s > path && *(s - 1) == sep)
            {
                memmove(s, s + 1, _tcslen(s + 1) + 1);
                --s;
            }

            ++s;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxPluginInterfaceForFS

    CFxPluginInterfaceForFS::CFxPluginInterfaceForFS(CFxPluginInterface& owner)
        : m_owner(owner)
    {
    }

    CFxPluginInterfaceForFS::~CFxPluginInterfaceForFS()
    {
    }

    void WINAPI CFxPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
    {
        CALL_STACK_MESSAGE2("CFxPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);

        BOOL changeRes;
        int failReason;

        changeRes = SalamanderGeneral->ChangePanelPathToPluginFS(
            panel,
            GetAssignedFSName(),
            TEXT(""),
            &failReason);
    }

    BOOL WINAPI CFxPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(
        HWND parent,
        int panel,
        int x,
        int y,
        CPluginFSInterfaceAbstract* pluginFS,
        const char* pluginFSName,
        int pluginFSNameIndex,
        BOOL isDetachedFS,
        BOOL& refreshMenu,
        BOOL& closeMenu,
        int& postCmd,
        void*& postCmdParam)
    {
        return FALSE;
    }

    void WINAPI CFxPluginInterfaceForFS::ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam)
    {
    }

    void WINAPI CFxPluginInterfaceForFS::ExecuteOnFS(
        int panel,
        CPluginFSInterfaceAbstract* pluginFS,
        const char* pluginFSName,
        int pluginFSNameIndex,
        CFileData& file,
        int isDir)
    {
        // Redirect call to the file system.
        auto* fxPluginFS = static_cast<CFxPluginFSInterface*>(pluginFS);
        fxPluginFS->ExecuteOnFS(file, isDir);
    }

    BOOL WINAPI CFxPluginInterfaceForFS::DisconnectFS(
        HWND parent,
        BOOL isInPanel,
        int panel,
        CPluginFSInterfaceAbstract* pluginFS,
        const char* pluginFSName,
        int pluginFSNameIndex)
    {
        BOOL ret = FALSE;

        CALL_STACK_MESSAGE5("CFxPluginInterfaceForFS::DisconnectFS(, %d, %d, , %s, %d)",
                            isInPanel, panel, pluginFSName, pluginFSNameIndex);

        auto* fxPluginFS = static_cast<CFxPluginFSInterface*>(pluginFS);
        fxPluginFS->m_calledFromDisconnectDialog = true;

        if (isInPanel)
        {
            SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
            ret = SalamanderGeneral->GetPanelPluginFS(panel) != pluginFS;
        }
        else
        {
            ret = SalamanderGeneral->CloseDetachedFS(parent, pluginFS);
        }

        if (!ret)
        {
            fxPluginFS->m_calledFromDisconnectDialog = false;
        }

        return ret;
    }

    void WINAPI CFxPluginInterfaceForFS::ConvertPathToInternal(
        const char* fsName,
        int fsNameIndex,
        char* fsUserPart)
    {
    }

    void WINAPI CFxPluginInterfaceForFS::ConvertPathToExternal(
        const char* fsName,
        int fsNameIndex,
        char* fsUserPart)
    {
    }

    void WINAPI CFxPluginInterfaceForFS::EnsureShareExistsOnServer(int panel, const char* server, const char* share)
    {
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxPluginFSInterface

    CFxPluginFSInterface::CFxPluginFSInterface(CFxPluginInterfaceForFS& owner)
        : m_owner(owner),
          m_calledFromDisconnectDialog(false),
          m_currentPath(nullptr),
          m_currentPathEnumerator(nullptr),
          m_lastParentItemTime(FILETIME_Nul)
    {
    }

    CFxPluginFSInterface::~CFxPluginFSInterface()
    {
        delete m_currentPath;

        if (m_currentPathEnumerator != nullptr)
        {
            m_currentPathEnumerator->Release();
        }
    }

    BOOL WINAPI CFxPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
    {
        TCHAR currentPath[MAX_PATH];
        BOOL ok = GetCurrentPath(currentPath);
        _ASSERTE(ok);
        return (currentFSNameIndex == fsNameIndex) &&
               SalamanderGeneral->IsTheSamePath(currentPath, userPart);
    }

    BOOL WINAPI CFxPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
    {
        UNREFERENCED_PARAMETER(currentFSNameIndex);
        UNREFERENCED_PARAMETER(fsNameIndex);
        UNREFERENCED_PARAMETER(userPart);

        // It's always our path.
        return TRUE;
    }

    BOOL WINAPI CFxPluginFSInterface::GetRootPath(char* userPart)
    {
        // Ineffective default implementation, if possible override in your descendant.
        CFxPath* root = CreatePath(TEXT(""));
        root->SetRoot();
        StringCchCopy(userPart, MAX_PATH, root->GetString());
        delete root;
        return TRUE;
    }

    BOOL WINAPI CFxPluginFSInterface::GetCurrentPath(char* userPart)
    {
        if (m_currentPath == nullptr)
        {
            *userPart = TEXT('\0');
        }
        else
        {
            StringCchCopy(userPart, MAX_PATH, m_currentPath->GetString());
        }

        return TRUE;
    }

    BOOL WINAPI CFxPluginFSInterface::ChangePath(
        int currentFSNameIndex,
        char* fsName,
        int fsNameIndex,
        const char* userPart,
        char* cutFileName,
        BOOL* pathWasCut,
        BOOL forceRefresh,
        int mode)
    {
        // zmeni aktualni cestu v tomto FS na cestu zadanou pres 'fsName' a 'userPart' (presne
        // nebo na nejblizsi pristupnou podcestu 'userPart' - viz hodnota 'mode'); v pripade, ze
        // se cesta zkracuje z duvodu, ze jde o cestu k souboru (staci domenka, ze by mohlo jit
        // o cestu k souboru - po vylistovani cesty se overuje jestli soubor existuje, pripadne
        // se zobrazi uzivateli chyba) a 'cutFileName' neni NULL (mozne jen v 'mode' 3), vraci
        // v bufferu 'cutFileName' (o velikosti MAX_PATH znaku) jmeno tohoto souboru (bez cesty),
        // jinak v bufferu 'cutFileName' vraci prazdny retezec; 'currentFSNameIndex' je index
        // aktualniho jmena FS; 'fsName' je buffer o velikosti MAX_PATH, na vstupu je v nem jmeno
        // FS v ceste, ktere je z tohoto pluginu (ale nemusi se shodovat s aktualnim jmenem FS
        // v tomto objektu, staci kdyz pro nej IsOurPath() vraci TRUE), na vystupu je v 'fsName'
        // aktualni jmeno FS v tomto objektu (musi byt z tohoto pluginu); 'fsNameIndex' je index
        // jmena FS 'fsName' v pluginu (pro snazsi detekci o jake jmeno FS jde); neni-li
        // 'pathWasCut' NULL, vraci se v nem TRUE pokud doslo ke zkraceni cesty; Salamander
        // pouziva 'cutFileName' a 'pathWasCut' u prikazu Change Directory (Shift+F7) pri zadani
        // jmena souboru - dochazi k fokusu tohoto souboru; je-li 'forceRefresh' TRUE, jde o
        // tvrdy refresh (Ctrl+R) a plugin by mel menit cestu bez pouziti informaci z cache
        // (je nutne overit jestli nova cesta existuje); 'mode' je rezim zmeny cesty:
        //   1 (refresh path) - zkracuje cestu, je-li treba; nehlasit neexistenci cesty (bez hlaseni
        //                      zkratit), hlasit soubor misto cesty, nepristupnost cesty a dalsi chyby
        //   2 (volani ChangePanelPathToPluginFS, back/forward in history, etc.) - zkracuje cestu,
        //                      je-li treba; hlasit vsechny chyby cesty (soubor
        //                      misto cesty, neexistenci, nepristupnost a dalsi)
        //   3 (change-dir command) - zkracuje cestu jen jde-li o soubor nebo cestu nelze listovat
        //                      (ListCurrentPath pro ni vraci FALSE); nehlasit soubor misto cesty
        //                      (bez hlaseni zkratit a vratit jmeno souboru), hlasit vsechny ostatni
        //                      chyby cesty (neexistenci, nepristupnost a dalsi)
        // je-li 'mode' 1 nebo 2, vraci FALSE jen pokud na tomto FS zadna cesta neni pristupna
        // (napr. pri vypadku spojeni); je-li 'mode' 3, vraci FALSE pokud neni pristupna
        // pozadovana cesta nebo soubor (ke zkracovani cesty dojde jen v pripade, ze jde o soubor);
        // v pripade, ze je otevreni FS casove narocne (napr. pripojeni na FTP server) a 'mode'
        // je 3, je mozne upravit chovani jako u archivu - zkracovat cestu, je-li treba a vracet FALSE
        // jen pokud na FS neni zadna cesta pristupna, hlaseni chyb se nemeni

        HRESULT hr;

        if (pathWasCut != nullptr)
        {
            *pathWasCut = FALSE;
        }

        if (cutFileName != nullptr)
        {
            *cutFileName = TEXT('\0');
        }

        CFxPath* newPath = CreatePath(userPart);
        hr = newPath->Canonicalize();
        if (FAILED(hr))
        {
            ShowChangePathError(newPath->GetString(), hr);
        }
        else
        {
            // Path is syntactically correct, now check if the path
            // physically exists on the file system.
            CFxItemEnumerator* enumerator = nullptr;
            int cutIndex = -1;
            hr = GetEnumeratorForPath(enumerator, *newPath, cutIndex, !!forceRefresh);
            HRESULT hrShowError = hr;
            CFxPath* showErrorPath = nullptr;

            if (hr == FX_E_PATH_IS_FILE && enumerator != nullptr && cutIndex >= newPath->GetLength() && mode == 3)
            {
                // The last path component is a file and we are in Shift+F7
                // mode. Trim the file specification.
                if (newPath->CutLastComponent(cutFileName, MAX_PATH))
                {
                    *pathWasCut = TRUE;
                    hr = hrShowError = S_OK;
                }
            }
            else if (FAILED(hr) && (mode == 1 || mode == 2) && enumerator != nullptr)
            {
                // Use accessible path.
                showErrorPath = CreatePath(*newPath);
                newPath->Truncate(cutIndex);
                if (newPath->CutLastComponent())
                {
                    // Some path is accessible, change current path to the accessible one.
                    hr = S_OK;

                    if (hrShowError == FX_E_PATH_NOT_FOUND && mode == 1)
                    {
                        // During refresh (mode == 1) do not display
                        // non-existent paths, just change to the accessible
                        // path silently.
                        hrShowError = S_OK;
                    }
                }
            }

            if (FAILED(hrShowError))
            {
                if (showErrorPath == nullptr)
                {
                    showErrorPath = CreatePath(newPath->Left(cutIndex));
                }
                ShowChangePathError(showErrorPath->GetString(), hrShowError);
            }

            delete showErrorPath;

            if (SUCCEEDED(hr))
            {
                SetCurrentPathEnumerator(newPath, enumerator);
                newPath = nullptr; // Prevent double delete.
            }
            else
            {
                if (enumerator != nullptr)
                {
                    enumerator->Release();
                }
            }
        }

        delete newPath;
        return SUCCEEDED(hr);
    }

    BOOL WINAPI CFxPluginFSInterface::ListCurrentPath(
        CSalamanderDirectoryAbstract* dir,
        CPluginDataInterfaceAbstract*& pluginData,
        int& iconsType,
        BOOL forceRefresh)
    {
        iconsType = pitSimple;

        CFxItemEnumerator* enumerator;
        HRESULT hr;
        hr = GetEnumeratorForCurrentPath(enumerator, !!forceRefresh);
        if (FAILED(hr))
        {
            TRACE_E("GetEnumerator failed with HRESULT 0x" << std::hex << hr);
            return FALSE;
        }

        auto fxPluginData = this->CreatePluginData(enumerator);
        pluginData = fxPluginData;
        bool assignItemForFiles = false;
        bool assignItemForDirs = false;
        if (pluginData != nullptr)
        {
            assignItemForFiles = !!pluginData->CallReleaseForFiles();
            assignItemForDirs = !!pluginData->CallReleaseForDirs();
            iconsType = fxPluginData->GetIconsType();
        }

        DWORD validData = enumerator->GetValidData();
        dir->SetValidData(validData);

        IFxItemConverter* converter = GetConverter();
        _ASSERTE(converter != nullptr);

        bool upDirAdded = false;

        while ((hr = enumerator->MoveNext()) == S_OK)
        {
            CFxItem* item = enumerator->GetCurrent();
            _ASSERTE(item != nullptr);
            CFileData fileData;
            bool fileDataIsDir;
            if (converter->ConvertItemToFileData(item, validData, fileData, fileDataIsDir))
            {
                bool addedOk;

                if (fileDataIsDir)
                {
                    if (assignItemForDirs)
                    {
                        // Tranfer ownership of the FSItem to the PluginData.
                        fileData.PluginData = reinterpret_cast<DWORD_PTR>(item);
                        item->AddRef();
                    }
                    addedOk = !!dir->AddDir(nullptr, fileData, nullptr);
                    if (addedOk && !upDirAdded)
                    {
                        upDirAdded = fileData.NameLen == 2 &&
                                     fileData.Name[0] == '.' &&
                                     fileData.Name[1] == '.';
                    }
                }
                else
                {
                    if (assignItemForFiles)
                    {
                        // Tranfer ownership of the FSItem to the PluginData.
                        fileData.PluginData = reinterpret_cast<DWORD_PTR>(item);
                        item->AddRef();
                    }
                    addedOk = !!dir->AddFile(nullptr, fileData, nullptr);
                }

                item->Release();

                _ASSERTE(addedOk);
                if (!addedOk)
                {
                    converter->FreeFileData(fileData);
                }
            }
        }

        enumerator->Release();

        if (SUCCEEDED(hr) && !upDirAdded && IsUpDirNeededForCurrentPath())
        {
            CFileData upDirFileData = {
                0,
            };
            if (GetFileDataForUpDir(upDirFileData))
            {
                _ASSERTE(upDirFileData.NameLen == 2 &&
                         upDirFileData.Name[0] == '.' &&
                         upDirFileData.Name[1] == '.' &&
                         upDirFileData.Name[2] == '\0');
                dir->AddDir(nullptr, upDirFileData, nullptr);
            }
        }

        m_lastParentItemTime = FILETIME_Nul;

        return SUCCEEDED(hr);
    }

    CFxPluginDataInterface* WINAPI CFxPluginFSInterface::CreatePluginData(CFxItemEnumerator* enumerator)
    {
        UNREFERENCED_PARAMETER(enumerator);
        return new CFxPluginFSDataInterface(*this);
    }

    IFxItemConverter* WINAPI CFxPluginFSInterface::GetConverter() const
    {
        return CFxStandardItemConverter::GetInstance();
    }

    HICON WINAPI CFxPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
    {
        destroyIcon = FALSE;
        return m_owner.GetOwner().GetPluginIcon();
    }

    BOOL WINAPI CFxPluginFSInterface::GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
    {
        return FALSE;
    }

    BOOL WINAPI CFxPluginFSInterface::GetFullFSPath(
        HWND parent,
        const char* fsName,
        char* path,
        int pathSize,
        BOOL& success)
    {
        return FALSE;
    }

    BOOL WINAPI CFxPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
    {
        return TRUE;
    }

    void WINAPI CFxPluginFSInterface::Event(int event, DWORD param)
    {
    }

    void WINAPI CFxPluginFSInterface::ReleaseObject(HWND parent)
    {
    }

    BOOL WINAPI CFxPluginFSInterface::GetChangeDriveOrDisconnectItem(
        const char* fsName,
        char*& title,
        HICON& icon,
        BOOL& destroyIcon)
    {
        // If you support FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM service,
        // then you must override this method in your descendant class and
        // provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::GetDropEffect(
        const char* srcFSPath,
        const char* tgtFSPath,
        DWORD allowedEffects,
        DWORD keyState,
        DWORD* dropEffect)
    {
    }

    void WINAPI CFxPluginFSInterface::GetFSFreeSpace(CQuadWord* retValue)
    {
        // If you support FS_SERVICE_GETFREESPACE service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    BOOL WINAPI CFxPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
    {
        // If you support FS_SERVICE_GETNEXTDIRLINEHOTPATH service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::CompleteDirectoryLineHotPath(char* path, int pathBufSize)
    {
        // If you support FS_SERVICE_GETNEXTDIRLINEHOTPATH service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    BOOL WINAPI CFxPluginFSInterface::GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize)
    {
        // If you support FS_SERVICE_GETPATHFORMAINWNDTITLE service, then you
        // must override this method in your descendant class and provide your
        // own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::ShowInfoDialog(const char* fsName, HWND parent)
    {
        // If you support FS_SERVICE_SHOWINFO service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    BOOL WINAPI CFxPluginFSInterface::ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo)
    {
        // If you support FS_SERVICE_COMMANDLINE service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    BOOL WINAPI CFxPluginFSInterface::QuickRename(
        const char* fsName,
        int mode,
        HWND parent,
        CFileData& file,
        BOOL isDir,
        char* newName,
        BOOL& cancel)
    {
        // If you support FS_SERVICE_QUICKRENAME service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::AcceptChangeOnPathNotification(
        const char* fsName,
        const char* path,
        BOOL includingSubdirs)
    {
        // If you support FS_SERVICE_ACCEPTSCHANGENOTIF service, then you must
        // override this method in your descendant class and provide your own
        // implementation.
        _ASSERTE(0);
    }

    BOOL WINAPI CFxPluginFSInterface::CreateDir(
        const char* fsName,
        int mode,
        HWND parent,
        char* newName,
        BOOL& cancel)
    {
        // If you support FS_SERVICE_CREATEDIR service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::ViewFile(
        const char* fsName,
        HWND parent,
        CSalamanderForViewFileOnFSAbstract* salamander,
        CFileData& file)
    {
        // If you support FS_SERVICE_VIEWFILE service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    BOOL WINAPI CFxPluginFSInterface::Delete(
        const char* fsName,
        int mode,
        HWND parent,
        int panel,
        int selectedFiles,
        int selectedDirs,
        BOOL& cancelOrError)
    {
        // If you support FS_SERVICE_DELETE service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    BOOL WINAPI CFxPluginFSInterface::CopyOrMoveFromFS(
        BOOL copy,
        int mode,
        const char* fsName,
        HWND parent,
        int panel,
        int selectedFiles,
        int selectedDirs,
        char* targetPath,
        BOOL& operationMask,
        BOOL& cancelOrHandlePath,
        HWND dropTarget)
    {
        // If you support FS_SERVICE_COPYFROMFS/MOVEFROMFS service, then you
        // must override this method in your descendant class and provide your
        // own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    BOOL WINAPI CFxPluginFSInterface::CopyOrMoveFromDiskToFS(
        BOOL copy,
        int mode,
        const char* fsName,
        HWND parent,
        const char* sourcePath,
        SalEnumSelection2 next,
        void* nextParam,
        int sourceFiles,
        int sourceDirs,
        char* targetPath,
        BOOL* invalidPathOrCancel)
    {
        // If you support FS_SERVICE_COPYFROMDISKTOFS/MOVEFROMDISKTOFS service,
        // then you must override this method in your descendant class and
        // provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    BOOL WINAPI CFxPluginFSInterface::ChangeAttributes(
        const char* fsName,
        HWND parent,
        int panel,
        int selectedFiles,
        int selectedDirs)
    {
        // If you support FS_SERVICE_CHANGEATTRS service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::ShowProperties(
        const char* fsName,
        HWND parent,
        int panel,
        int selectedFiles,
        int selectedDirs)
    {
        // If you support FS_SERVICE_SHOWPROPERTIES service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    void WINAPI CFxPluginFSInterface::ContextMenu(
        const char* fsName,
        HWND parent,
        int menuX,
        int menuY,
        int type,
        int panel,
        int selectedFiles,
        int selectedDirs)
    {
        // If you support FS_SERVICE_CONTEXTMENU service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    BOOL WINAPI CFxPluginFSInterface::HandleMenuMsg(
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT* plResult)
    {
        return FALSE;
    }

    BOOL WINAPI CFxPluginFSInterface::OpenFindDialog(const char* fsName, int panel)
    {
        // If you support FS_SERVICE_OPENFINDDLG service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::OpenActiveFolder(const char* fsName, HWND parent)
    {
        // If you support FS_SERVICE_OPENACTIVEFOLDER service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    void WINAPI CFxPluginFSInterface::GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects)
    {
    }

    BOOL WINAPI CFxPluginFSInterface::GetNoItemsInPanelText(char* textBuf, int textBufSize)
    {
        return FALSE;
    }

    void WINAPI CFxPluginFSInterface::ShowSecurityInfo(HWND parent)
    {
        // If you support FS_SERVICE_SHOWSECURITYINFO service, then you must override this
        // method in your descendant class and provide your own implementation.
        _ASSERTE(0);
    }

    void WINAPI CFxPluginFSInterface::ShowChangePathError(PCTSTR path, HRESULT hr)
    {
        CFxString s;
        FxGetErrorDescription(hr, s);
        ShowChangePathError(path, s);
    }

    void WINAPI CFxPluginFSInterface::ShowChangePathError(PCTSTR path, PCTSTR error)
    {
        CFxString title, message;
        title.LoadString(IDS_FX_CHANGEPATHERRORTITLE);
        CFxString fullPath;
        PTSTR fullPathBuffer = fullPath.GetBuffer(MAX_PATH);
        SalamanderGeneral->GetPluginFSName(fullPathBuffer, 0);
        fullPath.ReleaseBuffer();
        fullPath.AppendChar(TEXT(':'));
        fullPath.Append(path);
        message.Format(IDS_FX_CHANGEPATHERRORFMT, fullPath, error);
        SalamanderGeneral->ShowMessageBox(message, title, MSGBOX_ERROR);
    }

    void WINAPI CFxPluginFSInterface::SetCurrentPath(CFxPath* path)
    {
        _ASSERTE(path != nullptr);

        if (m_currentPath != nullptr)
        {
            delete m_currentPath;
        }

        m_currentPath = path;
    }

    void WINAPI CFxPluginFSInterface::SetCurrentPathEnumerator(CFxItemEnumerator* enumerator)
    {
        _ASSERTE(enumerator != nullptr);
        _ASSERTE(m_currentPath != nullptr);

        if (m_currentPathEnumerator != nullptr)
        {
            m_currentPathEnumerator->Release();
        }

        m_currentPathEnumerator = enumerator;
    }

    HRESULT WINAPI CFxPluginFSInterface::GetEnumeratorForPath(
        _Out_ CFxItemEnumerator*& enumerator,
        _Inout_ CFxPath& path,
        _Out_ int& pathCutIndex,
        bool forceRefresh)
    {
        HRESULT hr;
        CFxItemEnumerator* parentEnumerator = nullptr;
        CFxItemEnumerator* childEnumerator;
        int level = 0;
        CFxPathComponentToken pathComponentToken;

        enumerator = nullptr;

        for (;;)
        {
            childEnumerator = nullptr;
            m_lastParentItemTime = FILETIME_Nul;

            bool gotPathComponent = path.GetNextPathComponent(pathComponentToken);

            CFxItem* parentItem = nullptr;
            if (parentEnumerator != nullptr)
            {
                parentItem = parentEnumerator->GetCurrent();
                _ASSERTE(parentItem != nullptr);
            }

            hr = GetChildEnumerator(childEnumerator, parentItem, level, forceRefresh);
            _ASSERTE(SUCCEEDED(hr) || level > 0);

            if (parentEnumerator != nullptr &&
                (parentEnumerator->GetValidData() & (VALID_DATA_DATE | VALID_DATA_TIME | VALID_DATA_PL_DATE | VALID_DATA_PL_TIME)))
            {
                // Parent enumerator can provide date/time info. Remember
                // last write time of the parent item so we have data
                // for the up-dir item.
                FILETIME parentItemTime;
                if (SUCCEEDED(parentItem->GetLastWriteTime(parentItemTime)))
                {
                    m_lastParentItemTime = parentItemTime;
                }
            }

            if (parentItem != nullptr)
            {
                parentItem->Release();
            }

            bool exitLoop = !gotPathComponent || FAILED(hr);

            if (parentEnumerator != nullptr)
            {
                if (childEnumerator == nullptr && exitLoop)
                {
                    // We are exitting, return the last enumerator we have.
                    childEnumerator = parentEnumerator;
                }
                else
                {
                    parentEnumerator->Release();
                }
            }

            if (exitLoop)
            {
                break;
            }

            pathCutIndex = pathComponentToken.GetTokenEnd();

            if (SUCCEEDED(hr))
            {
                bool componentFound = false;

                while (!componentFound && (hr = childEnumerator->MoveNext()) == S_OK)
                {
                    CFxItem* item = childEnumerator->GetCurrent();
                    _ASSERTE(item != nullptr);
                    CFxString name;
                    item->GetName(name);
                    _ASSERTE(!name.IsEmpty());
                    componentFound = pathComponentToken.ComponentEquals(name);
                    if (componentFound)
                    {
                        // Update the path components because
                        // it may differ in letter cases.
                        pathComponentToken.ReplaceComponent(name);

                        // Path components need to be directories.
                        DWORD attrs = item->GetAttributes();
                        if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
                        {
                            hr = FX_E_PATH_IS_FILE;
                        }
                    }

                    item->Release();
                }

                if (hr == S_FALSE)
                {
                    hr = FX_E_PATH_NOT_FOUND;
                    break;
                }
                else if (FAILED(hr))
                {
                    break;
                }

                parentEnumerator = childEnumerator;
            }
            else
            {
                break;
            }

            ++level;
        }

        _ASSERTE(childEnumerator != nullptr || FAILED(hr));
        enumerator = childEnumerator;

        return hr;
    }

    HRESULT WINAPI CFxPluginFSInterface::GetEnumeratorForCurrentPath(_Out_ CFxItemEnumerator*& enumerator, bool forceRefresh)
    {
        UNREFERENCED_PARAMETER(forceRefresh);

        _ASSERTE(m_currentPathEnumerator != nullptr);

        enumerator = m_currentPathEnumerator;
        enumerator->AddRef();
        enumerator->Reset();

        return S_OK;
    }

    bool WINAPI CFxPluginFSInterface::IsUpDirNeededForCurrentPath()
    {
        // We only want up dirs for non-root paths.
        bool isRoot = false;
        TCHAR rootPath[MAX_PATH];
        if (GetRootPath(rootPath) && m_currentPath != nullptr)
        {
            isRoot = m_currentPath->Equals(rootPath);
        }
        return !isRoot;
    }

    void WINAPI CFxPluginFSInterface::ExecuteOnFS(CFileData& file, int isDir)
    {
        if (isDir == 2)
        {
            ExecuteUpDir();
        }
        else
        {
            auto item = reinterpret_cast<CFxItem*>(file.PluginData);
            _ASSERTE(item != nullptr);
            ExecuteItem(*item);
        }
    }

    void WINAPI CFxPluginFSInterface::ExecuteUpDir()
    {
        _ASSERTE(m_currentPath != nullptr);
        CFxPath* newPath = CreatePath(m_currentPath->GetString());
        TCHAR cutComponent[MAX_PATH];
        newPath->CutLastComponent(cutComponent, _countof(cutComponent));
        ChangeDirectory(newPath->GetString(), cutComponent);
        delete newPath;
    }

    void WINAPI CFxPluginFSInterface::ExecuteItem(CFxItem& item)
    {
        DWORD attr = item.GetAttributes();
        if (attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            _ASSERTE(m_currentPath != nullptr);
            CFxPath* newPath = CreatePath(m_currentPath->GetString());
            newPath->IncludeTrailingSeparator();
            CFxString name;
            item.GetName(name);
            newPath->Append(name);
            ChangeDirectory(newPath->GetString());
            delete newPath;
        }
        else
        {
            // TODO:
            ////SalamanderGeneral->SetUserWorkedOnPanelPath(panel);
            item.Execute();
        }
    }

    void WINAPI CFxPluginFSInterface::ChangeDirectory(PCTSTR newPath, PCTSTR focusedName)
    {
        int panel;
        bool gotFSOk = !!SalamanderGeneral->GetPanelWithPluginFS(this, panel);
        _ASSERTE(gotFSOk);

        TCHAR fsName[MAX_PATH];
        SalamanderGeneral->GetPluginFSName(fsName, 0);

        SalamanderGeneral->ChangePanelPathToPluginFS(
            panel,
            fsName,
            newPath,
            nullptr,
            -1,
            focusedName);
    }

    bool WINAPI CFxPluginFSInterface::GetFileDataForUpDir(_Out_ CFileData& upDirFileData)
    {
        upDirFileData.Name = SalamanderGeneral->DupStr(TEXT(".."));
        upDirFileData.NameLen = 2;
        upDirFileData.Ext = upDirFileData.Name + upDirFileData.NameLen;
        upDirFileData.Attr = FILE_ATTRIBUTE_DIRECTORY;

        if (m_lastParentItemTime.dwLowDateTime != 0U && m_lastParentItemTime.dwHighDateTime != 0U)
        {
            // We have time information from the last ChangePath.
            upDirFileData.LastWrite = m_lastParentItemTime;
        }
        else
        {
            // Just use the current time.
            ::GetSystemTimeAsFileTime(&upDirFileData.LastWrite);
        }

        return true;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxPluginFSDataInterface

    CFxPluginFSDataInterface::CFxPluginFSDataInterface(CFxPluginFSInterface& owner)
        : m_owner(owner)
    {
    }

    CFxPluginFSDataInterface::~CFxPluginFSDataInterface()
    {
    }

    void WINAPI CFxPluginFSDataInterface::SetupView(
        BOOL leftPanel,
        CSalamanderViewAbstract* view,
        const char* archivePath,
        const CFileData* upperDir)
    {
        __super::SetupView(leftPanel, view, archivePath, upperDir);

        view->SetPluginSimpleIconCallback(&CFxPluginFSDataInterface::GetSimpleIconIndex);
    }

    int WINAPI CFxPluginFSDataInterface::GetSimpleIconIndex()
    {
        auto item = reinterpret_cast<CFxItem*>((*CFxPluginDataInterface::s_transferFileData)->PluginData);
        int iconIndex = item->GetSimpleIconIndex();
        return iconIndex;
    }

}; // namespace Fx
