// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Salamander Plugin Development Framework
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	fx.h
	Contains base classes for developing Salamander plugins.
*/

#pragma once

#include "fx_errcodes.h"

namespace Fx
{

#ifndef _FX_ATL_INTERWORK
#ifdef __ATLCORE_H__
#define _FX_ATL_INTERWORK 1
#else
#define _FX_ATL_INTERWORK 0
#endif
#endif

#if _FX_ATL_INTERWORK

    static inline HINSTANCE FxGetModuleInstance()
    {
        return ATL::_AtlBaseModule.GetModuleInstance();
    }

    static inline HINSTANCE FxGetLangInstance()
    {
        return ATL::_AtlBaseModule.GetResourceInstance();
    }

    extern const FILETIME FILETIME_Nul;

    typedef ATL::CStringA CFxString;

#else

    extern "C" IMAGE_DOS_HEADER __ImageBase;

    static inline HINSTANCE FxGetModuleInstance()
    {
        return reinterpret_cast<HINSTANCE>(&__ImageBase);
    }

    /**
	Retrieves module handle of the language resources.
	
	\return The return value is handle to the language resources module.
*/
    static inline HINSTANCE FxGetLangInstance()
    {
        extern HINSTANCE g_hFxLangInst;
        return g_hFxLangInst;
    }

    template <typename TChar>
    class StrTraitFx : public ATL::ChTraitsCRT<TChar>
    {
    public:
        static HINSTANCE FindStringResourceInstance(_In_ UINT nID) throw()
        {
            HINSTANCE hLangInst = FxGetLangInstance();
            // The string may be loaded after the language module is loaded.
            _ASSERTE(hLangInst);
            return hLangInst;
        }

        static ATL::IAtlStringMgr* GetDefaultManager() throw()
        {
            return ATL::CAtlStringMgr::GetInstance();
        }
    };

    typedef ATL::CStringT<char, StrTraitFx<char>> CFxString;

#endif // _FX_ATL_INTERWORK

    /// Maintains object lifetime by keeping track of number of references.
    class CFxRefCounted
    {
    private:
        LONG m_cRef;

        // http://blogs.msdn.com/oldnewthing/archive/2005/09/28/474855.aspx
        enum
        {
            DESTRUCTOR_REFCOUNT = 42
        };

        /// Called when the ref-count drops to zero but before the
        /// destructor is called.
        virtual void WINAPI FinalRelease();

    protected:
        CFxRefCounted();

    public:
        virtual ~CFxRefCounted();

        void WINAPI AddRef();
        void WINAPI Release();
    };

    class CFxItem : public CFxRefCounted
    {
    protected:
        CFxItem();

    public:
        virtual ~CFxItem();

        virtual void WINAPI GetName(CFxString& name) = 0;

        virtual unsigned WINAPI GetNameLengthWithoutExtension();

        virtual DWORD WINAPI GetAttributes();

        virtual HRESULT WINAPI GetSize(ULONGLONG& size);

        virtual HRESULT WINAPI GetLastWriteTime(FILETIME& time);

        virtual int WINAPI GetSimpleIconIndex();

        virtual bool WINAPI HasSlowIcon();

        /// \param size Size of the icon, in pixels.
        /// \return S_OK if the icon for item was successfully extracted.
        ///         FX_S_DESTROYICON if the icon was successfully extracted
        ///         and needs to be destroyed by Salamander.
        /// \remarks This method is called in context of the icon reader thread.
        virtual HRESULT WINAPI GetSlowIcon(int size, _Out_ HICON& hico);

        virtual HRESULT WINAPI Execute();

        enum STREAM_OPTIONS
        {
            SO_None = 0,
            SO_Read = 1,
            SO_Write = 2,
            SO_ReadWrite = SO_Read | SO_Write,
            SO_ThreadSafe = 4
        };

        virtual STREAM_OPTIONS WINAPI GetSupportedStreamOptions();

        virtual HRESULT WINAPI GetStream(STREAM_OPTIONS flags, _Out_ IStream*& stream);
    };

    template <typename T, typename TCurrent = T*>
    class TFxEnumerator : public CFxRefCounted
    {
    public:
        /// Moves to the next item.
        /// \return If the operation succeeds returns S_OK and \see GetCurrent
        ///         returns current item.
        ///         If there are no more items the return value is S_FALSE.
        ///         If the enumeration fails then returns appropriate error code.
        virtual HRESULT WINAPI MoveNext() = 0;

        virtual void WINAPI Reset() = 0;

        virtual TCurrent WINAPI GetCurrent() const = 0;
    };

    /// Enumerates items.
    class CFxItemEnumerator : public TFxEnumerator<CFxItem>
    {
    public:
        enum FLAGS
        {
            FLAGS_None = 0,
            FLAGS_MayTakeLong = 1,
        };

        virtual DWORD WINAPI GetFlags() const;

        /// Returns information about attributes that are valid for
        /// the enumerated items.
        /// \return Combinatation of VALID_DATA_xxx flags.
        virtual DWORD WINAPI GetValidData() const;
    };

    class CFxPluginInterface : public CPluginInterfaceAbstract
    {
    private:
        enum FLAGS
        {
            FLAGS_InterfaceForFSCreated = 1,
        };

        HICON m_hicoSmall;

        CPluginInterfaceForFSAbstract* m_pInterfaceForFS;

        UINT m_uFlags;

        // Copy constructor is private to prevent use.
        CFxPluginInterface(const CFxPluginInterface&);

        static void WINAPI WinLibHtmlHelpCallback(HWND hWindow, UINT helpID);

    protected:
        CFxPluginInterface();

    public:
        virtual ~CFxPluginInterface();

        HICON GetPluginIcon() const
        {
            return m_hicoSmall;
        }

        /* Overridables */

        virtual bool WINAPI Initialize(CSalamanderPluginEntryAbstract* salamander);

        virtual DWORD WINAPI GetSupportedFunctions() const = 0;

        virtual void WINAPI GetPluginEnglishName(CFxString& name) const = 0;

        virtual void WINAPI GetPluginConfigKey(CFxString& key) const;

        virtual void WINAPI GetPluginName(CFxString& name) const;

        virtual void WINAPI GetPluginDescription(CFxString& description) const;

        virtual void WINAPI GetPluginHelpFileName(CFxString& fileName) const;

        virtual void WINAPI GetPluginHomePageUrl(CFxString& url) const;

    protected:
        virtual void WINAPI SetPluginIcons(CSalamanderConnectAbstract* salamander);

        virtual void WINAPI SetFSChangeDriveItem(CSalamanderConnectAbstract* salamander);

        virtual bool WINAPI NeedsWinLib() const;

        virtual class CFxPluginInterfaceForFS* WINAPI CreateInterfaceForFS();

    public:
        /* CPluginInterfaceAbstract */

        virtual void WINAPI About(HWND parent) override;

        virtual BOOL WINAPI Release(HWND parent, BOOL force) override;

        virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) override;

        virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) override;

        virtual void WINAPI Configuration(HWND parent) override;

        virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander) override;

        virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) override;

        virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() override;

        virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() override;

        virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() override;

        virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() override;

        virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() override;

        virtual void WINAPI Event(int event, DWORD param) override;

        virtual void WINAPI ClearHistory(HWND parent) override;

        virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) override;

        virtual void WINAPI PasswordManagerEvent(HWND parent, int event) override;
    };

    class CFxPluginDataInterface : public CPluginDataInterfaceAbstract
    {
    private:
        BOOL WINAPI GetItemLastWriteDateTime(_In_ const CFileData* file, _In_ DWORD validData, _Out_ SYSTEMTIME* time);

    protected:
        static const CFileData** s_transferFileData;
        static int* s_transferIsDir;
        static char* s_transferBuffer;
        static int* s_transferLen;
        static DWORD* s_transferRowData;
        static CPluginDataInterfaceAbstract** s_transferPluginDataInterface;
        static DWORD* s_transferActCustomData;

        void WINAPI AppendInfoLineContentPart(
            PCTSTR partText,
            char* buffer,
            DWORD* hotTexts,
            int& hotTextsCount);

    public:
        CFxPluginDataInterface();
        virtual ~CFxPluginDataInterface();

        /* Overridables */

        virtual int WINAPI GetIconsType() const;

        /* CPluginDataInterfaceAbstract */

        virtual BOOL WINAPI CallReleaseForFiles() override;

        virtual BOOL WINAPI CallReleaseForDirs() override;

        virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir) override;

        virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) override;

        virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) override;

        virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) override;

        virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) override;

        virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) override;

        virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2) override;

        virtual void WINAPI SetupView(
            BOOL leftPanel,
            CSalamanderViewAbstract* view,
            const char* archivePath,
            const CFileData* upperDir) override;

        virtual void WINAPI ColumnFixedWidthShouldChange(
            BOOL leftPanel,
            const CColumn* column,
            int newFixedWidth) override;

        virtual void WINAPI ColumnWidthWasChanged(
            BOOL leftPanel,
            const CColumn* column,
            int newWidth) override;

        virtual BOOL WINAPI GetInfoLineContent(
            int panel,
            const CFileData* file,
            BOOL isDir,
            int selectedFiles,
            int selectedDirs,
            BOOL displaySize,
            const CQuadWord& selectedSize,
            char* buffer,
            DWORD* hotTexts,
            int& hotTextsCount) override;

        virtual BOOL WINAPI CanBeCopiedToClipboard() override;

        virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) override;

        virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) override;

        virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) override;
    };

    class __declspec(novtable) IFxItemConverter
    {
    public:
        /// Converts CFxItem into Salamander's CFileData structure.
        /// \remarks This method should not touch the PluginData member, it is
        ///          managed by the framework.
        virtual bool WINAPI ConvertItemToFileData(_In_ CFxItem* item, _In_ DWORD validData, _Out_ CFileData& data, _Out_ bool& isDir) throw() = 0;

        /// Frees resources in the Salamander's CFileData structure allocated
        /// in \see ConvertItemToFileData.
        /// \remarks This method should not touch the PluginData member, it is
        ///          managed by the framework.
        virtual void WINAPI FreeFileData(_In_ CFileData& data) throw() = 0;
    };

    class CFxStandardItemConverter : public IFxItemConverter
    {
    private:
        static CFxStandardItemConverter s_instance;

    public:
        virtual bool WINAPI ConvertItemToFileData(_In_ CFxItem* item, _In_ DWORD validData, _Out_ CFileData& data, _Out_ bool& isDir) throw() override;

        virtual void WINAPI FreeFileData(_In_ CFileData& data) throw() override;

        static CFxStandardItemConverter* GetInstance();
    };

    /// Converts SALICONSIZE_nn definition into actual pixel size.
    int WINAPI FxSalIconSizeToPixelSize(int size);

    bool WINAPI FxGetWin32ErrorDescription(HRESULT hr, CFxString& text);
    bool WINAPI FxGetErrorDescription(HRESULT hr, CFxString& text, bool useDefaultIfNotFound = true);

}; // namespace Fx

#ifndef _FX_NO_AUTOMATIC_NAMESPACE
using namespace Fx;
#endif
