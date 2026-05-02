// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Salamander Plugin Development Framework
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	fx.cpp
	Contains base classes for developing Salamander plugins.
*/

#include "precomp.h"
#include "fx.h"
#include "fxfs.h"
#include "fx.rh"
#include "fx_lang.rh"

extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern CSalamanderGUIAbstract* SalamanderGUI;

namespace Fx
{

#if !_FX_ATL_INTERWORK
    HINSTANCE g_hFxLangInst;
#endif

    const FILETIME FILETIME_Nul;

    ////////////////////////////////////////////////////////////////////////////////
    // CFxRefCounted

    CFxRefCounted::CFxRefCounted()
        : m_cRef(1)
    {
    }

    CFxRefCounted::~CFxRefCounted()
    {
        _ASSERTE(m_cRef == DESTRUCTOR_REFCOUNT);
    }

    void WINAPI CFxRefCounted::FinalRelease()
    {
    }

    void CFxRefCounted::AddRef()
    {
        InterlockedIncrement(&m_cRef);
    }

    void CFxRefCounted::Release()
    {
        LONG cRef;

        _ASSERTE(m_cRef > 0);

        cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            FinalRelease();
            m_cRef = DESTRUCTOR_REFCOUNT; // avoid double-destruction
            delete this;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxItem

    CFxItem::CFxItem()
    {
    }

    CFxItem::~CFxItem()
    {
    }

    unsigned WINAPI CFxItem::GetNameLengthWithoutExtension()
    {
        CFxString name;
        GetName(name);
        PCTSTR pszName = name;
        PCTSTR pszExt = PathFindExtension(pszName);
        return static_cast<unsigned>(pszExt - pszName);
    }

    DWORD WINAPI CFxItem::GetAttributes()
    {
        return 0U;
    }

    HRESULT WINAPI CFxItem::GetSize(ULONGLONG& size)
    {
        _ASSERTE(0);
        return E_NOTIMPL;
    }

    HRESULT WINAPI CFxItem::GetLastWriteTime(FILETIME& time)
    {
        _ASSERTE(0);
        return E_NOTIMPL;
    }

    int WINAPI CFxItem::GetSimpleIconIndex()
    {
        _ASSERTE(0);
        return -1;
    }

    bool WINAPI CFxItem::HasSlowIcon()
    {
        return false;
    }

    HRESULT WINAPI CFxItem::GetSlowIcon(int size, _Out_ HICON& hico)
    {
        _ASSERTE(0);
        return E_NOTIMPL;
    }

    HRESULT WINAPI CFxItem::Execute()
    {
        return S_OK;
    }

    CFxItem::STREAM_OPTIONS WINAPI CFxItem::GetSupportedStreamOptions()
    {
        return SO_None;
    }

    HRESULT WINAPI CFxItem::GetStream(STREAM_OPTIONS flags, _Out_ IStream*& stream)
    {
        return E_NOTIMPL;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxItemEnumerator

    DWORD WINAPI CFxItemEnumerator::GetFlags() const
    {
        return FLAGS_None;
    }

    DWORD WINAPI CFxItemEnumerator::GetValidData() const
    {
        // Return mask for attributes we have
        return
            // Extension is inferred from name. Name is mandatory
            // (CFxItem::GetName is abstract).
            VALID_DATA_EXTENSION |

            // Default CFxItem::GetAttributes is provided.
            VALID_DATA_ATTRIBUTES |

            // Can be inferred from extension.
            VALID_DATA_ISLINK |
            VALID_DATA_TYPE |

            // These two can be inferred from attributes.
            VALID_DATA_HIDDEN |
            VALID_DATA_ISOFFLINE |

            // We don't have default implementation for these.
            //// VALID_DATA_SIZE |
            //// VALID_DATA_DATE |
            //// VALID_DATA_TIME |

            0U;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxPluginInterface

    CFxPluginInterface::CFxPluginInterface()
        : m_hicoSmall(nullptr),
          m_uFlags(0U)
    {
    }

    CFxPluginInterface::~CFxPluginInterface()
    {
        _ASSERTE(m_hicoSmall == nullptr);
    }

    void WINAPI CFxPluginInterface::About(HWND parent)
    {
        CFxString message, caption, name, description;

        GetPluginName(name);
        GetPluginDescription(description);
        caption.LoadString(IDS_FX_ABOUT);

        message.Format(TEXT("%s ") TEXT(VERSINFO_VERSION) TEXT("\n\n")
                           TEXT(VERSINFO_COPYRIGHT) TEXT("\n\n")
                               TEXT("%s"),
                       name,
                       description);

        SalamanderGeneral->SalMessageBox(
            parent,
            message,
            caption,
            MB_OK | MB_ICONINFORMATION);
    }

    BOOL WINAPI CFxPluginInterface::Release(HWND parent, BOOL force)
    {
        if (m_uFlags & FLAGS_InterfaceForFSCreated)
        {
            delete m_pInterfaceForFS;
            m_pInterfaceForFS = nullptr;
            m_uFlags &= ~FLAGS_InterfaceForFSCreated;
        }

        if (NeedsWinLib())
        {
            ReleaseWinLib(FxGetModuleInstance());
        }

        if (m_hicoSmall != nullptr)
        {
            BOOL ok = ::DestroyIcon(m_hicoSmall);
            _ASSERTE(ok);
            m_hicoSmall = nullptr;
        }

        return TRUE;
    }

    void WINAPI CFxPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
    {
    }

    void WINAPI CFxPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
    {
    }

    void WINAPI CFxPluginInterface::Configuration(HWND parent)
    {
    }

    void WINAPI CFxPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
    {
        SetPluginIcons(salamander);

        DWORD supportedFunctions = GetSupportedFunctions();
        if (supportedFunctions & FUNCTION_FILESYSTEM)
        {
            SetFSChangeDriveItem(salamander);
        }
    }

    void WINAPI CFxPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
    {
        // Typecast to call the correct virtual destructor.
        auto fxPluginData = static_cast<CFxPluginDataInterface*>(pluginData);
        delete fxPluginData;
    }

    CPluginInterfaceForArchiverAbstract* WINAPI CFxPluginInterface::GetInterfaceForArchiver()
    {
        return nullptr;
    }

    CPluginInterfaceForViewerAbstract* WINAPI CFxPluginInterface::GetInterfaceForViewer()
    {
        return nullptr;
    }

    CPluginInterfaceForMenuExtAbstract* WINAPI CFxPluginInterface::GetInterfaceForMenuExt()
    {
        return nullptr;
    }

    CPluginInterfaceForFSAbstract* WINAPI CFxPluginInterface::GetInterfaceForFS()
    {
        if (!(m_uFlags & FLAGS_InterfaceForFSCreated))
        {
            m_pInterfaceForFS = CreateInterfaceForFS();
            m_uFlags |= FLAGS_InterfaceForFSCreated;
        }
        return m_pInterfaceForFS;
    }

    CPluginInterfaceForThumbLoaderAbstract* WINAPI CFxPluginInterface::GetInterfaceForThumbLoader()
    {
        return nullptr;
    }

    void WINAPI CFxPluginInterface::Event(int event, DWORD param)
    {
    }

    void WINAPI CFxPluginInterface::ClearHistory(HWND parent)
    {
    }

    void WINAPI CFxPluginInterface::AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs)
    {
    }

    void WINAPI CFxPluginInterface::PasswordManagerEvent(HWND parent, int event)
    {
    }

    bool WINAPI CFxPluginInterface::Initialize(CSalamanderPluginEntryAbstract* salamander)
    {
        CFxString s;

        // Load language module.
        GetPluginEnglishName(s);
        HINSTANCE hLangInst = salamander->LoadLanguageModule(salamander->GetParentWindow(), s);
        if (hLangInst == nullptr)
        {
            return false;
        }

#if _FX_ATL_INTERWORK
        ATL::_AtlBaseModule.SetResourceInstance(hLangInst);
#else
        g_hFxLangInst = hLangInst;
#endif

        if (NeedsWinLib())
        {
            // Note that 's' variable still contains invariant name!
            if (!InitializeWinLib(s, FxGetModuleInstance()))
            {
                return false;
            }

            SetupWinLibHelp(&CFxPluginInterface::WinLibHtmlHelpCallback);
        }

        // Setup help file name.
        GetPluginHelpFileName(s);
        SalamanderGeneral->SetHelpFileName(s);

        // Setup plugin home page.
        GetPluginHomePageUrl(s);
        salamander->SetPluginHomePageURL(s);

        return true;
    }

    void WINAPI CFxPluginInterface::SetPluginIcons(CSalamanderConnectAbstract* salamander)
    {
        auto iconList = SalamanderGUI->CreateIconList();
        iconList->CreateFromPNG(FxGetModuleInstance(), MAKEINTRESOURCE(IDB_FX_PLUGIN_ICONS), 16);
        m_hicoSmall = iconList->GetIcon(0);
        salamander->SetIconListForGUI(iconList);
        salamander->SetPluginIcon(0);
        salamander->SetPluginMenuAndToolbarIcon(0);
    }

    void WINAPI CFxPluginInterface::SetFSChangeDriveItem(CSalamanderConnectAbstract* salamander)
    {
        CFxString itemText;
        GetPluginName(itemText);
        itemText.Insert(0, TEXT('\t'));
        salamander->SetChangeDriveMenuItem(itemText, 0);
    }

    void WINAPI CFxPluginInterface::GetPluginName(CFxString& name) const
    {
        // Load the name from the language module.
        name.LoadString(IDS_FX_PLUGIN_NAME);
    }

    void WINAPI CFxPluginInterface::GetPluginDescription(CFxString& description) const
    {
        // Load the description from the language module.
        description.LoadString(IDS_FX_PLUGIN_DESCRIPTION);
    }

    void WINAPI CFxPluginInterface::GetPluginConfigKey(CFxString& key) const
    {
        // By default the configuration key is equal to the name of the plugin
        // module (without the extension).
        TCHAR fileName[MAX_PATH];
        DWORD cch = GetModuleFileName(FxGetModuleInstance(), fileName, _countof(fileName));
        _ASSERTE(cch > 0);
        PTSTR namePart = PathFindFileName(fileName);
        PTSTR ext = PathFindExtension(namePart);
        *ext = TEXT('\0');
        key = namePart;
    }

    void WINAPI CFxPluginInterface::GetPluginHelpFileName(CFxString& fileName) const
    {
        GetPluginConfigKey(fileName);
        fileName.Append(TEXT(".chm"));
    }

    void WINAPI CFxPluginInterface::GetPluginHomePageUrl(CFxString& url) const
    {
        url = TEXT("www.altap.cz");
    }

    bool WINAPI CFxPluginInterface::NeedsWinLib() const
    {
        // Initialize the WinLib, plugin can opt-out.
        return true;
    }

    void WINAPI CFxPluginInterface::WinLibHtmlHelpCallback(HWND hWindow, UINT helpID)
    {
        SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
    }

    CFxPluginInterfaceForFS* WINAPI CFxPluginInterface::CreateInterfaceForFS()
    {
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxPluginDataInterface

    const CFileData** CFxPluginDataInterface::s_transferFileData;
    int* CFxPluginDataInterface::s_transferIsDir;
    char* CFxPluginDataInterface::s_transferBuffer;
    int* CFxPluginDataInterface::s_transferLen;
    DWORD* CFxPluginDataInterface::s_transferRowData;
    CPluginDataInterfaceAbstract** CFxPluginDataInterface::s_transferPluginDataInterface;
    DWORD* CFxPluginDataInterface::s_transferActCustomData;

    CFxPluginDataInterface::CFxPluginDataInterface()
    {
    }

    CFxPluginDataInterface::~CFxPluginDataInterface()
    {
    }

    BOOL WINAPI CFxPluginDataInterface::CallReleaseForFiles()
    {
        return TRUE;
    }

    BOOL WINAPI CFxPluginDataInterface::CallReleaseForDirs()
    {
        return TRUE;
    }

    void WINAPI CFxPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
    {
        auto* item = reinterpret_cast<CFxItem*>(file.PluginData);

        // May be null for framework created up-dirs.
        if (item != nullptr)
        {
            item->Release();
        }
    }

    void WINAPI CFxPluginDataInterface::GetFileDataForUpDir(const char* archivePath, CFileData& upDir)
    {
    }

    BOOL WINAPI CFxPluginDataInterface::GetFileDataForNewDir(const char* dirName, CFileData& dir)
    {
        return FALSE;
    }

    HIMAGELIST WINAPI CFxPluginDataInterface::GetSimplePluginIcons(int iconSize)
    {
        return nullptr;
    }

    BOOL WINAPI CFxPluginDataInterface::HasSimplePluginIcon(CFileData& file, BOOL isDir)
    {
        auto* item = reinterpret_cast<CFxItem*>(file.PluginData);
        _ASSERTE(item != nullptr);
        return !item->HasSlowIcon();
    }

    HICON WINAPI CFxPluginDataInterface::GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon)
    {
        HRESULT hr;
        HICON hico = nullptr;

        auto* item = reinterpret_cast<CFxItem*>(file->PluginData);
        _ASSERTE(item != nullptr);
        hr = item->GetSlowIcon(FxSalIconSizeToPixelSize(iconSize), hico);
        if (SUCCEEDED(hr))
        {
            destroyIcon = (hr == FX_S_DESTROY_ICON);
            return hico;
        }

        _ASSERTE(hico == nullptr);
        destroyIcon = FALSE;
        return nullptr;
    }

    int WINAPI CFxPluginDataInterface::CompareFilesFromFS(const CFileData* file1, const CFileData* file2)
    {
        return _tcscmp(file1->Name, file2->Name);
    }

    void WINAPI CFxPluginDataInterface::SetupView(
        BOOL leftPanel,
        CSalamanderViewAbstract* view,
        const char* archivePath,
        const CFileData* upperDir)
    {
        view->GetTransferVariables(
            s_transferFileData,
            s_transferIsDir,
            s_transferBuffer,
            s_transferLen,
            s_transferRowData,
            s_transferPluginDataInterface,
            s_transferActCustomData);
    }

    void WINAPI CFxPluginDataInterface::ColumnFixedWidthShouldChange(
        BOOL leftPanel,
        const CColumn* column,
        int newFixedWidth)
    {
    }

    void WINAPI CFxPluginDataInterface::ColumnWidthWasChanged(
        BOOL leftPanel,
        const CColumn* column,
        int newWidth)
    {
    }

    BOOL WINAPI CFxPluginDataInterface::GetInfoLineContent(
        int panel,
        const CFileData* file,
        BOOL isDir,
        int selectedFiles,
        int selectedDirs,
        BOOL displaySize,
        const CQuadWord& selectedSize,
        char* buffer,
        DWORD* hotTexts,
        int& hotTextsCount)
    {
        return FALSE;
    }

    BOOL WINAPI CFxPluginDataInterface::CanBeCopiedToClipboard()
    {
        return FALSE;
    }

    BOOL WINAPI CFxPluginDataInterface::GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size)
    {
        HRESULT hr = FX_E_NO_DATA;
        CFxItem* item = reinterpret_cast<CFxItem*>(file->PluginData);
        if (item != nullptr)
        {
            hr = item->GetSize(size->Value);
        }
        return SUCCEEDED(hr);
    }

    BOOL WINAPI CFxPluginDataInterface::GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date)
    {
        return GetItemLastWriteDateTime(file, VALID_DATA_DATE, date);
    }

    BOOL WINAPI CFxPluginDataInterface::GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time)
    {
        return GetItemLastWriteDateTime(file, VALID_DATA_TIME, time);
    }

    BOOL WINAPI CFxPluginDataInterface::GetItemLastWriteDateTime(_In_ const CFileData* file, _In_ DWORD validData, _Out_ SYSTEMTIME* time)
    {
        CFxItem* item = reinterpret_cast<CFxItem*>(file->PluginData);
        if (item == nullptr)
        {
            return FALSE;
        }

        FILETIME ft;
        HRESULT hr = item->GetLastWriteTime(ft);
        if (FAILED(hr))
        {
            return FALSE;
        }

        FILETIME ftLocal;
        if (!FileTimeToLocalFileTime(&ft, &ftLocal))
        {
            return FALSE;
        }

        SYSTEMTIME st;
        if (!FileTimeToSystemTime(&ftLocal, &st))
        {
            return FALSE;
        }

        // Only return the requested part of SYSTEMTIME, the other part must
        // stay unmodified.

        if (validData & VALID_DATA_DATE)
        {
            time->wYear = st.wYear;
            time->wMonth = st.wMonth;
            time->wDay = st.wDay;
            time->wDayOfWeek = st.wDayOfWeek;
        }

        if (validData & VALID_DATA_TIME)
        {
            time->wHour = st.wHour;
            time->wMinute = st.wMinute;
            time->wSecond = st.wSecond;
            time->wMilliseconds = st.wMilliseconds;
        }

        return TRUE;
    }

    int WINAPI CFxPluginDataInterface::GetIconsType() const
    {
        HIMAGELIST himl = const_cast<CFxPluginDataInterface*>(this)->GetSimplePluginIcons(SALICONSIZE_16);
        return (himl != nullptr) ? pitFromPlugin : pitSimple;
    }

    void WINAPI CFxPluginDataInterface::AppendInfoLineContentPart(
        PCTSTR partText,
        char* buffer,
        DWORD* hotTexts,
        int& hotTextsCount)
    {
        if (*buffer != TEXT('\0'))
        {
            StringCchCat(buffer, 1000, TEXT(", "));
        }

        if (*partText == TEXT('\0'))
        {
            StringCchCat(buffer, 1000, TEXT("-"));
        }
        else
        {
            hotTexts[hotTextsCount++] = MAKELONG(_tcslen(buffer), _tcslen(partText));
            StringCchCat(buffer, 1000, partText);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CFxStandardItemConverter

    CFxStandardItemConverter CFxStandardItemConverter::s_instance;

    bool WINAPI CFxStandardItemConverter::ConvertItemToFileData(
        _In_ CFxItem* item,
        _In_ DWORD validData,
        _Out_ CFileData& data,
        _Out_ bool& isDir) throw()
    {
        memset(&data, 0, sizeof(CFileData));

        CFxString name;
        item->GetName(name);
        data.Name = SalamanderGeneral->DupStr(name);
        data.NameLen = name.GetLength();

        if (validData & VALID_DATA_EXTENSION)
        {
            data.Ext = data.Name + item->GetNameLengthWithoutExtension();
            if (*data.Ext == '.')
            {
                // Salamander wants pointer *after* the dot.
                ++data.Ext;
            }
        }

        HRESULT hr;

        if (validData & VALID_DATA_SIZE)
        {
            hr = item->GetSize(data.Size.Value);

            // This should succeed. If you don't have size for all of
            // your items, then you should rather specify VALID_DATA_PL_SIZE
            // in your enumerator's GetValidData.
            _ASSERTE(SUCCEEDED(hr));
        }

        if (validData & (VALID_DATA_DATE | VALID_DATA_TIME))
        {
            hr = item->GetLastWriteTime(data.LastWrite);

            // This should succeed. If you don't have date/time for all of
            // your items, then you should rather specify VALID_DATA_PL_DATE
            // and/or VALID_DATA_PL_TIME in your enumerator's GetValidData.
            _ASSERTE(SUCCEEDED(hr));
        }

        if (validData & VALID_DATA_ATTRIBUTES)
        {
            DWORD attrs = item->GetAttributes();
            isDir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            data.Attr = attrs;

            if ((validData & (VALID_DATA_ISLINK | VALID_DATA_EXTENSION)) == (VALID_DATA_ISLINK | VALID_DATA_EXTENSION))
            {
                if (!isDir && *data.Ext != '\0')
                {
                    data.IsLink = SalamanderGeneral->IsFileLink(data.Ext);
                }
            }

            // The following attributes are cheap, no need to
            // check against VALID_DATA_xxx.
            data.Hidden = (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
            data.IsOffline = (attrs & FILE_ATTRIBUTE_OFFLINE) != 0;
        }

        return true;
    }

    void WINAPI CFxStandardItemConverter::FreeFileData(
        _In_ CFileData& data) throw()
    {
        SalamanderGeneral->Free(data.Name);
    }

    CFxStandardItemConverter* CFxStandardItemConverter::GetInstance()
    {
        return &s_instance;
    }

    ////////////////////////////////////////////////////////////////////////////////

    static const int s_pixelSizes[] =
        {
            0,  // error
            16, // SALICONSIZE_16
            32, // SALICONSIZE_32
            48, // SALICONSIZE_48
    };

    int WINAPI FxSalIconSizeToPixelSize(int size)
    {
        int pixelSize = 0;

        if (size < _countof(s_pixelSizes))
        {
            pixelSize = s_pixelSizes[size];
        }

        _ASSERTE(pixelSize > 0);
        return pixelSize;
    }

    bool WINAPI FxGetWin32ErrorDescription(HRESULT hr, CFxString& text)
    {
        PTSTR szMessage = nullptr;
        DWORD dwLangId = 0U;

        if (FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                hr,
                dwLangId,
                (PTSTR)&szMessage,
                0,
                nullptr) == 0)
        {
            // Unknown HRESULT.
            text.Empty();
            return false;
        }

        text = szMessage;
        LocalFree(szMessage);

        int nLen = text.GetLength();
        while (nLen > 0 && (text[nLen - 1] == '\r' || text[nLen - 1] == '\n'))
        {
            nLen--;
        }

        text.Truncate(nLen);

        return true;
    }

    bool WINAPI FxGetErrorDescription(HRESULT hr, CFxString& text, bool useDefaultIfNotFound)
    {
        bool found;

        if (HRESULT_FACILITY(hr) == __FX_HRFACILITY)
        {
            found = !!text.LoadString(IDS_FX_ERRDESCRIPTION_BASE + HRESULT_CODE(hr));
        }
        else
        {
            found = FxGetWin32ErrorDescription(hr, text);
        }

        if (!found && useDefaultIfNotFound)
        {
            text.Format("0x%08X", hr);
        }

        return found;
    }

}; // namespace Fx
