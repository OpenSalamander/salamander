// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*************************************************************************

How to host an IContextMenu, part 1 - Initial foray
http://blogs.msdn.com/oldnewthing/archive/2004/09/20/231739.aspx

How to host an IContextMenu, part 2 - Displaying the context menu
http://blogs.msdn.com/oldnewthing/archive/2004/09/22/232836.aspx

How to host an IContextMenu, part 3 - Invocation location
http://blogs.msdn.com/oldnewthing/archive/2004/09/23/233376.aspx

How to host an IContextMenu, part 4 - Key context
http://blogs.msdn.com/oldnewthing/archive/2004/09/24/234113.aspx

How to host an IContextMenu, part 5 - Handling menu messages
http://blogs.msdn.com/oldnewthing/archive/2004/09/27/234739.aspx

How to host an IContextMenu, part 6 - Displaying menu help
http://blogs.msdn.com/oldnewthing/archive/2004/09/28/235242.aspx

How to host an IContextMenu, part 7 - Invoking the default verb
http://blogs.msdn.com/oldnewthing/archive/2004/09/30/236133.aspx

How to host an IContextMenu, part 8 - Optimizing for the default command
http://blogs.msdn.com/oldnewthing/archive/2004/10/01/236627.aspx

How to host an IContextMenu, part 9 - Adding custom commands
http://blogs.msdn.com/oldnewthing/archive/2004/10/04/237507.aspx

How to host an IContextMenu, part 10 - Composite extensions - groundwork
http://blogs.msdn.com/oldnewthing/archive/2004/10/06/238630.aspx

How to host an IContextMenu, part 11 - Composite extensions - composition
http://blogs.msdn.com/oldnewthing/archive/2004/10/07/239197.aspx
*************************************************************************/

#define CMIC_MASK_SHIFT_DOWN 0x10000000   //IE
#define CMIC_MASK_CONTROL_DOWN 0x40000000 //IE

#define SCRATCH_QCM_FIRST 1
#define SCRATCH_QCM_LAST 0x6FFF
//file menu items
#define IDM_FOCUS 0x7000
#define IDM_ZOOMIN 0x7001
#define IDM_ZOOMOUT 0x7002
//dirline menu items
#define IDM_OPEN 0x7010
#define IDM_GOTO 0x7011

class CShellMenu
{
protected:
    IContextMenu* _pcm;
    IContextMenu2* _pcm2;
    IContextMenu3* _pcm3;

    IShellFolder* _pDesktop;

    HWND _hWnd;

    //  PURPOSE:    This routine takes a full path to a file and converts that
    //  to a fully qualified ITEMIDLIST.
    //  PARAMETERS:
    //      pszFile  - Full path to the file.
    //  RETURN VALUE:
    //      Returns a fully qualified ITEMIDLIST, or NULL if an error occurs.
    LPITEMIDLIST PIDL_GetFromPath(LPWSTR pszPath)
    {
        LPITEMIDLIST res = NULL;
        if (FAILED(this->_pDesktop->ParseDisplayName(this->_hWnd, NULL, pszPath, NULL, &res, NULL)))
        {
            return NULL;
        }
        return res;
    }

    //  PURPOSE:    Returns the total number of bytes in an ITEMIDLIST.
    //  PARAMETERS:
    //      pidl - Pointer to the ITEMIDLIST that you want the size of.
    DWORD PIDL_GetSize(LPITEMIDLIST pidl)
    {
        if (pidl == NULL)
            return 0;
        DWORD res = 0;
        res += sizeof pidl->mkid.cb;
        while (pidl->mkid.cb != 0)
        {
            res += pidl->mkid.cb;
            //pidl = pidl + pidl->mkid.cb;
            pidl = (LPITEMIDLIST)((LONG_PTR)pidl + pidl->mkid.cb);
        }
        return res;
    }

    //  PURPOSE:    Creates a new ITEMIDLIST of the specified size.
    //  PARAMETERS:
    //      piMalloc - Pointer to the allocator interface that should allocate memory.
    //  cbSize   - Size of the ITEMIDLIST to create.
    //  RETURN VALUE:
    //      Returns a pointer to the new ITEMIDLIST, or NULL if a problem occured.
    LPITEMIDLIST PIDL_Create(DWORD size)
    {
        LPITEMIDLIST res = (LPITEMIDLIST)CoTaskMemAlloc(size);
        if (res != NULL)
        {
            memset(res, 0, size);
        }
        return res;
    }

    //  PURPOSE:    Creates a new copy of an ITEMIDLIST.
    //  PARAMETERS:
    //      piMalloc - Pointer to the allocator interfaced to be used to allocate the new ITEMIDLIST.
    //  RETURN VALUE:
    //      Returns a pointer to the new ITEMIDLIST, or NULL if an error occurs.
    LPITEMIDLIST PIDL_Copy(LPITEMIDLIST pidlSource)
    {
        if (pidlSource == NULL)
            return NULL;
        DWORD cbSource;
        LPITEMIDLIST res = NULL;
        cbSource = PIDL_GetSize(pidlSource);
        res = PIDL_Create(cbSource);
        if (res == NULL)
            return NULL;

        //CopyMemory(res, pidlSource, cbSource);
        memcpy(res, pidlSource, cbSource);
        return res;
    }

    //  PURPOSE:    Returns a pointer to the next item in the ITEMIDLIST.
    //  PARAMETERS:
    //      pidl - Pointer to an ITEMIDLIST to walk through
    LPITEMIDLIST PIDL_GetNextItem(LPITEMIDLIST pidl)
    {
        if (pidl == NULL)
            return NULL;
        return (LPITEMIDLIST)((LONG_PTR)pidl + pidl->mkid.cb);
    }

    //  PURPOSE:    Takes a fully qualified pidl and returns the the relative pidl
    //  and the root part of that pidl.
    //  PARAMETERS:
    //  pidlFQ   - Pointer to the fully qualified ITEMIDLIST that needs to be parsed.
    //  pidlRoot - Points to the pidl that will contain the root after parsing.
    //  pidlItem - Points to the item relative to pidlRoot after parsing.
    void PIDL_Free(LPITEMIDLIST pidl)
    {
        if (pidl != NULL)
            CoTaskMemFree(pidl);
    }

    //  PURPOSE:    This routine takes a fully qualified pidl for a folder and returns
    //  the IShellFolder pointer for that pidl
    //  PARAMETERS:
    //  pidl     - Pointer to a fully qualified ITEMIDLIST for the folder
    //      piParentFolder - Pointer to the IShellFolder of the folder (Return value).
    //  RETURN VALUE:
    //      Returns TRUE if successful, FALSE otherwise.
    HRESULT PIDL_GetFileFolder(LPITEMIDLIST pidl, IShellFolder** ppfolder)
    {
        return this->_pDesktop->BindToObject(pidl, NULL, IID_IShellFolder, (void**)ppfolder);
    }

    //  PURPOSE:    Takes a fully qualified pidl and returns the the relative pidl
    //  and the root part of that pidl.
    //  PARAMETERS:
    //  pidlFQ   - Pointer to the fully qualified ITEMIDLIST that needs to be parsed.
    //  pidlRoot - Points to the pidl that will contain the root after parsing.
    //  pidlItem - Points to the item relative to pidlRoot after parsing.
    void PIDL_GetRelative(LPITEMIDLIST pidlFQ, LPITEMIDLIST* ppidlRoot, LPITEMIDLIST* ppidlItem)
    {
        if (pidlFQ == NULL)
        {
            ppidlRoot = NULL;
            ppidlItem = NULL;
            return;
        }
        *ppidlRoot = PIDL_Copy(pidlFQ);
        LPITEMIDLIST pidlTemp = *ppidlRoot;
        while (pidlTemp->mkid.cb != 0)
        {
            LPITEMIDLIST pidlNext = PIDL_GetNextItem(pidlTemp);
            if (pidlNext->mkid.cb == 0)
            {
                *ppidlItem = PIDL_Copy(pidlTemp);
                pidlTemp->mkid.cb = 0;
                pidlTemp->mkid.abID[0] = 0;
            }
            pidlTemp = pidlNext;
        }
    }

    HRESULT GetUIObjectOfFile(IShellFolder* psf, LPCITEMIDLIST pidl, REFIID riid, void** ppv)
    {
        return psf->GetUIObjectOf(this->_hWnd, 1, &pidl, riid, NULL, ppv);
    }

    HRESULT GetUIObjectOfFile(LPWSTR pszPath, REFIID riid, void** ppv)
    {
        IShellFolder* ShellFolder = NULL;
        LPITEMIDLIST DirPIDL = NULL;
        LPITEMIDLIST DirPIDLFQ = NULL;
        LPITEMIDLIST ParentPIDL = NULL;

        HRESULT hr;
        DirPIDLFQ = PIDL_GetFromPath(pszPath);
        if (DirPIDLFQ)
        {
            PIDL_GetRelative(DirPIDLFQ, &ParentPIDL, &DirPIDL);
            if (FAILED(hr = PIDL_GetFileFolder(ParentPIDL, &ShellFolder)))
            {
                PIDL_Free(DirPIDL);
                PIDL_Free(DirPIDLFQ);
                PIDL_Free(ParentPIDL);
                return hr;
            }
            //LPCITEMIDLIST cDirPIDL = DirPIDL;
            hr = GetUIObjectOfFile(ShellFolder, DirPIDL, riid, ppv);
            PIDL_Free(DirPIDL);
            PIDL_Free(DirPIDLFQ);
            PIDL_Free(ParentPIDL);
            ShellFolder->Release();
            return hr;
            //ShellDisplayContextMenu(Handle, P, ShellFolder, 1, DirPIDL, AllowRename, Verb, PerformPaste);
        }
        return E_FAIL;
    }

public:
    CShellMenu(HWND hWnd)
    {
        this->_pcm = NULL;
        this->_pcm2 = NULL;
        this->_pcm3 = NULL;
        this->_hWnd = hWnd;
        if (FAILED(SHGetDesktopFolder(&this->_pDesktop)))
        {
            this->_pDesktop = NULL;
        }
    }
    ~CShellMenu()
    {
        if (this->_pcm2)
            this->_pcm2->Release();
        this->_pcm2 = NULL;
        if (this->_pcm3)
            this->_pcm3->Release();
        this->_pcm3 = NULL;
        if (this->_pcm)
            this->_pcm->Release();
        this->_pcm = NULL;
        if (this->_pDesktop)
            this->_pDesktop->Release();
        this->_pDesktop = NULL;
    }
    BOOL HandleMenuMessage(UINT uiMsg, WPARAM wParam, LPARAM lParam)
    {
        if (this->_pcm3)
        {
            LRESULT lres;
            if (SUCCEEDED(this->_pcm3->HandleMenuMsg2(uiMsg, wParam, lParam, &lres)))
            {
                //return lres;
                return TRUE;
            }
        }
        else if (this->_pcm2)
        {
            if (SUCCEEDED(this->_pcm2->HandleMenuMsg(uiMsg, wParam, lParam)))
            {
                //return 0;
                return TRUE;
            }
        }
        return FALSE;
    }

    BOOL InvokeDefaultCommand(TCHAR* filename)
    {
        //if (this->_pcm != NULL) return FALSE; //menu je otevreno

#ifdef UNICODE
        WCHAR* wFileName = filename;
#else
        WCHAR wFileName[2 * MAX_PATH + 2];
        int wl = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, filename, -1, wFileName, ARRAYSIZE(wFileName));
        if (wl == 0)
            return FALSE;
#endif
        BOOL res = FALSE;
        IContextMenu* pcm;
        if (SUCCEEDED(GetUIObjectOfFile(wFileName, IID_IContextMenu, (void**)&pcm)))
        {
            HMENU hmenu = CreatePopupMenu();
            if (hmenu)
            {
                if (SUCCEEDED(pcm->QueryContextMenu(hmenu, 0,
                                                    SCRATCH_QCM_FIRST, SCRATCH_QCM_LAST,
                                                    CMF_DEFAULTONLY)))
                {
                    UINT id = GetMenuDefaultItem(hmenu, FALSE, 0);
                    if (id != (UINT)-1)
                    {
                        CMINVOKECOMMANDINFO info = {0};
                        info.cbSize = sizeof(info);
                        info.hwnd = this->_hWnd;
                        info.lpVerb = MAKEINTRESOURCEA(id - SCRATCH_QCM_FIRST);
                        info.nShow = SW_SHOWNORMAL;
                        res = (SUCCEEDED(pcm->InvokeCommand(&info)));
                    }
                }
                DestroyMenu(hmenu);
            }
            pcm->Release();
        }
        return res;
    }

#ifdef SALAMANDER
    int ShowFileMenu(TCHAR* filename, int xPos, int yPos, BOOL canFocus, BOOL canZoomIn, BOOL canZoomOut)
#else
    int ShowFileMenu(TCHAR* filename, int xPos, int yPos, BOOL canZoomIn, BOOL canZoomOut)
#endif
    {
        if (this->_pcm != NULL)
            return NULL; //menu jiz otevreno

        POINT pt = {xPos, yPos};
        /*if (pt.x == -1 && pt.y == -1) {
			pt.x = pt.y = 0;
			ClientToScreen(this->_hWnd, &pt);
		}*/
        int res = 0;
#ifdef UNICODE
        WCHAR* wFileName = filename;
#else
        WCHAR wFileName[2 * MAX_PATH + 2];
        int wl = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, filename, -1, wFileName, ARRAYSIZE(wFileName));
        if (wl == 0)
            return FALSE;
#endif
        if (SUCCEEDED(GetUIObjectOfFile(wFileName, IID_IContextMenu, (void**)&this->_pcm)))
        {
            HMENU hmenu = CreatePopupMenu();
            if (hmenu)
            {
                UINT qcmFlags = CMF_NORMAL; // | CMF_NODEFAULT;
                if (GetKeyState(VK_SHIFT) < 0)
                {
                    qcmFlags |= CMF_EXTENDEDVERBS;
                }
                int mPos = 0;

                if (
#ifdef SALAMANDER
                    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani InsertMenu() dole...
MENU_TEMPLATE_ITEM FileMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_DISKMAP_SHELL_FOCUS
	{MNTT_IT, IDS_DISKMAP_SHELL_ZOOMIN
	{MNTT_IT, IDS_DISKMAP_SHELL_ZOOMOUT
	{MNTT_PE, 0
};
*/
                    InsertMenu(hmenu, mPos++, MF_BYPOSITION | (canFocus ? MF_ENABLED : MF_GRAYED), IDM_FOCUS, CZResourceString::GetString(IDS_DISKMAP_SHELL_FOCUS)) &&
#endif
                    InsertMenu(hmenu, mPos++, MF_BYPOSITION | (canZoomIn ? MF_ENABLED : MF_GRAYED), IDM_ZOOMIN, CZResourceString::GetString(IDS_DISKMAP_SHELL_ZOOMIN)) &&
                    InsertMenu(hmenu, mPos++, MF_BYPOSITION | (canZoomOut ? MF_ENABLED : MF_GRAYED), IDM_ZOOMOUT, CZResourceString::GetString(IDS_DISKMAP_SHELL_ZOOMOUT)) &&
                    InsertMenu(hmenu, mPos++, MF_BYPOSITION | MF_SEPARATOR, NULL, NULL) &&
                    SUCCEEDED(this->_pcm->QueryContextMenu(hmenu, mPos, SCRATCH_QCM_FIRST, SCRATCH_QCM_LAST, qcmFlags)))
                {
#ifdef SALAMANDER
                    if (canZoomIn)
                    {
                        SetMenuDefaultItem(hmenu, 1, TRUE);
                    }
                    /*else if (canFocus)
					{
						SetMenuDefaultItem(hmenu, 0, TRUE);
					}*/
                    else
                    {
                        //C4245 ok: podle MSDN ma byt -1 pro nevybrani zadneho defaultu
                        SetMenuDefaultItem(hmenu, (UINT)-1, TRUE);
                    }
#else
                    if (canZoomIn)
                    {
                        SetMenuDefaultItem(hmenu, 0, TRUE);
                    }
                    else
                    {
                        //C4245 ok: podle MSDN ma byt -1 pro nevybrani zadneho defaultu
                        SetMenuDefaultItem(hmenu, (UINT)-1, TRUE);
                    }
#endif
                    this->_pcm->QueryInterface(IID_IContextMenu2, (void**)&this->_pcm2);
                    this->_pcm->QueryInterface(IID_IContextMenu3, (void**)&this->_pcm3);

                    int iCmd = TrackPopupMenuEx(hmenu, TPM_RETURNCMD, pt.x, pt.y, this->_hWnd, NULL);

                    if (this->_pcm2)
                    {
                        this->_pcm2->Release();
                        this->_pcm2 = NULL;
                    }
                    if (this->_pcm3)
                    {
                        this->_pcm3->Release();
                        this->_pcm3 = NULL;
                    }

                    if (iCmd == IDM_FOCUS)
                    {
                        res = iCmd;
                    }
                    else if (iCmd == IDM_ZOOMIN)
                    {
                        res = iCmd;
                    }
                    else if (iCmd == IDM_ZOOMOUT)
                    {
                        res = iCmd;
                    }
                    else if (iCmd > 0)
                    {
                        CMINVOKECOMMANDINFOEX info = {0};
                        info.cbSize = sizeof(info);
                        info.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
                        if (GetKeyState(VK_CONTROL) < 0)
                        {
                            info.fMask |= CMIC_MASK_CONTROL_DOWN;
                        }
                        if (GetKeyState(VK_SHIFT) < 0)
                        {
                            info.fMask |= CMIC_MASK_SHIFT_DOWN;
                        }
                        info.hwnd = this->_hWnd;
                        info.lpVerb = MAKEINTRESOURCEA(iCmd - SCRATCH_QCM_FIRST);
                        info.lpVerbW = MAKEINTRESOURCEW(iCmd - SCRATCH_QCM_FIRST);
                        info.nShow = SW_SHOWNORMAL;
                        info.ptInvoke = pt;
                        this->_pcm->InvokeCommand((LPCMINVOKECOMMANDINFO)&info);
                    }
                }
                DestroyMenu(hmenu);
            }
            this->_pcm->Release();
            this->_pcm = NULL;
        }
        return res;
    }
#ifdef SALAMANDER
    int ShowDirMenu(TCHAR* filename, int xPos, int yPos, BOOL canOpen, BOOL canGoto)
#else
    int ShowDirMenu(TCHAR* filename, int xPos, int yPos, BOOL canGoto)
#endif
    {
        if (this->_pcm != NULL)
            return NULL; //menu jiz otevreno

        POINT pt = {xPos, yPos};
        /*if (pt.x == -1 && pt.y == -1) {
		pt.x = pt.y = 0;
		ClientToScreen(this->_hWnd, &pt);
		}*/
        int res = 0;
#ifdef UNICODE
        WCHAR* wFileName = filename;
#else
        WCHAR wFileName[2 * MAX_PATH + 2];
        int wl = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, filename, -1, wFileName, ARRAYSIZE(wFileName));
        if (wl == 0)
            return FALSE;
#endif
        if (SUCCEEDED(GetUIObjectOfFile(wFileName, IID_IContextMenu, (void**)&this->_pcm)))
        {
            HMENU hmenu = CreatePopupMenu();
            if (hmenu)
            {
                UINT qcmFlags = CMF_NORMAL; // | CMF_NODEFAULT;
                if (GetKeyState(VK_SHIFT) < 0)
                {
                    qcmFlags |= CMF_EXTENDEDVERBS;
                }
                int mPos = 0;

                if (
#ifdef SALAMANDER
                    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani InsertMenu() dole...
MENU_TEMPLATE_ITEM DirMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_DISKMAP_SHELL_DIROPEN
  {MNTT_IT, IDS_DISKMAP_SHELL_DIRCHANGE
  {MNTT_PE, 0
};
*/
                    InsertMenu(hmenu, mPos++, MF_BYPOSITION | (canOpen ? MF_ENABLED : MF_GRAYED), IDM_OPEN, CZResourceString::GetString(IDS_DISKMAP_SHELL_DIROPEN)) &&
#endif
                    InsertMenu(hmenu, mPos++, MF_BYPOSITION | (canGoto ? MF_ENABLED : MF_GRAYED), IDM_GOTO, CZResourceString::GetString(IDS_DISKMAP_SHELL_DIRCHANGE)) &&
                    InsertMenu(hmenu, mPos++, MF_BYPOSITION | MF_SEPARATOR, NULL, NULL) &&
                    SUCCEEDED(this->_pcm->QueryContextMenu(hmenu, mPos, SCRATCH_QCM_FIRST, SCRATCH_QCM_LAST, qcmFlags)))
                {
#ifdef SALAMANDER
                    if (canGoto)
                    {
                        SetMenuDefaultItem(hmenu, 1, TRUE);
                    }
                    /*else if (canFocus)
					{
					SetMenuDefaultItem(hmenu, 0, TRUE);
					}*/
                    else
                    {
                        //C4245 ok: podle MSDN ma byt -1 pro nevybrani zadneho defaultu
                        SetMenuDefaultItem(hmenu, (UINT)-1, TRUE);
                    }
#else
                    if (canGoto)
                    {
                        SetMenuDefaultItem(hmenu, 0, TRUE);
                    }
                    else
                    {
                        //C4245 ok: podle MSDN ma byt -1 pro nevybrani zadneho defaultu
                        SetMenuDefaultItem(hmenu, (UINT)-1, TRUE);
                    }
#endif
                    this->_pcm->QueryInterface(IID_IContextMenu2, (void**)&this->_pcm2);
                    this->_pcm->QueryInterface(IID_IContextMenu3, (void**)&this->_pcm3);

                    int iCmd = TrackPopupMenuEx(hmenu, TPM_RETURNCMD, pt.x, pt.y, this->_hWnd, NULL);

                    if (this->_pcm2)
                    {
                        this->_pcm2->Release();
                        this->_pcm2 = NULL;
                    }
                    if (this->_pcm3)
                    {
                        this->_pcm3->Release();
                        this->_pcm3 = NULL;
                    }

                    if (iCmd == IDM_OPEN)
                    {
                        res = iCmd;
                    }
                    else if (iCmd == IDM_GOTO)
                    {
                        res = iCmd;
                    }
                    else if (iCmd > 0)
                    {
                        CMINVOKECOMMANDINFOEX info = {0};
                        info.cbSize = sizeof(info);
                        info.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
                        if (GetKeyState(VK_CONTROL) < 0)
                        {
                            info.fMask |= CMIC_MASK_CONTROL_DOWN;
                        }
                        if (GetKeyState(VK_SHIFT) < 0)
                        {
                            info.fMask |= CMIC_MASK_SHIFT_DOWN;
                        }
                        info.hwnd = this->_hWnd;
                        info.lpVerb = MAKEINTRESOURCEA(iCmd - SCRATCH_QCM_FIRST);
                        info.lpVerbW = MAKEINTRESOURCEW(iCmd - SCRATCH_QCM_FIRST);
                        info.nShow = SW_SHOWNORMAL;
                        info.ptInvoke = pt;
                        this->_pcm->InvokeCommand((LPCMINVOKECOMMANDINFO)&info);
                    }
                }
                DestroyMenu(hmenu);
            }
            this->_pcm->Release();
            this->_pcm = NULL;
        }
        return res;
    }
};
