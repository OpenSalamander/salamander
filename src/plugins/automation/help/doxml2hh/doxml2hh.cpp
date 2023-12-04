// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// doxml2hh.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

static const TCHAR HTML_HEADER[] =
    _T("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n")
    _T("<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n")
    _T("<head>\n")
    _T("<title>%s</title>\n")
    _T("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n")
    _T("<link rel=\"stylesheet\" type=\"text/css\" href=\"../salamander_help.css\" />\n")
    _T("<link rel=\"stylesheet\" type=\"text/css\" href=\"../salamander_help_shared.css\" />\n")
    _T("</head>\n")
    _T("\n")
    _T("<body>\n")
    _T("<div id=\"help_page\">\n")
    _T("<div class=\"header\">Open Salamander: Plugins: Automation: Reference%s</div>\n")
    _T("<div class=\"page\">\n")
    _T("\n")
    _T("<h1>%s</h1>\n")
    _T("\n")
    _T("%s\n");

static const TCHAR HTML_FOOTER[] =
    _T("\n</div> <!--page-->\n")
    _T("<div class=\"footer\">&copy; 2023 Open Salamander Authors</div>\n")
    _T("</div> <!--help_page-->\n")
    _T("</body>\n")
    _T("</html>\n");

static const TCHAR HTML_METHODS[] = _T("<h2>Methods</h2>\n");
static const TCHAR HTML_PROPERTIES[] = _T("<h2>Properties</h2>\n");

static const TCHAR HTML_NAMETABLE_BEGIN[] =
    _T("<table>\n")
    _T("	<tr>\n")
    _T("		<td class=\"hdr\">Name</td>\n")
    _T("		<td class=\"hdr\">Description</td>\n")
    _T("	</tr>\n");

static const TCHAR HTML_NAMETABLE_METHOD[] =
    _T("	<tr>\n")
    _T("		<td class=\"method\">%s</td>\n")
    _T("		<td>%s</td>\n")
    _T("	</tr>\n");

static const TCHAR HTML_NAMETABLE_PROPERTY[] =
    _T("	<tr>\n")
    _T("		<td class=\"property\">%s</td>\n")
    _T("		<td>%s</td>\n")
    _T("	</tr>\n");

static const TCHAR HTML_NAMETABLE_END[] =
    _T("</table>\n");

static const TCHAR HTML_SEEALSO[] =
    _T("\n\n<h2>See Also</h2>\n");

static const TCHAR HTML_SEEALSO_LINK[] =
    _T("<p><a href=\"%s\">%s</a></p>\n");

static const TCHAR HTML_METHOD_SYNTAX[] =
    _T("<h2>Syntax</h2>\n")
    _T("<pre><code>%s</code></pre>\n");

static const TCHAR HTML_RETURNVALUES[] =
    _T("<h2>Return Values</h2>\n");

static const TCHAR HTML_PROPERTYVALUES[] =
    _T("\n<h2>Values</h2>\n");

static const TCHAR HTML_METHODPARAMS[] =
    _T("<h2>Parameters</h2>\n");

static const TCHAR HTML_REMARKS[] =
    _T("\n\n<h2>Remarks</h2>\n");

static const TCHAR HTML_METHODPARAM[] =
    _T("<h3>%s</h3>\n")
    _T("%s\n");

CPath g_sIndexHHK;

enum ProcessType
{
    TypeObject,
    TypeMethod,
    TypeProperty,
};

struct Int
{
    int i;
    Int()
    {
        i = 0;
    }

    operator int()
    {
        return i;
    }

    int& operator++()
    {
        i++;
        return i;
    }
};

typedef CAtlMap<CString, Int> CReferenceMap;

struct Link
{
    CString sHref;
    CString sName;
    bool bLong;

    Link()
    {
        bLong = false;
    }

    Link(CString& sHref, CString sName)
    {
        this->sHref = sHref;
        this->sName = sName;
        this->bLong = false;
    }

    bool operator<(const Link& other)
    {
        return this->sName < other.sName;
    }
};

template <typename E>
class CSortedAtlList : public CAtlList<E>
{
public:
    POSITION Insert(INARGTYPE element)
    {
        POSITION prev = NULL;
        POSITION pos = GetHeadPosition();
        if (pos == NULL)
        {
            return AddTail(element);
        }

        while (pos && GetAt(pos) < element)
        {
            prev = pos;
            GetNext(pos);
        }

        if (prev == NULL)
        {
            return AddHead(element);
        }

        return InsertAfter(prev, element);
    }
};

CString GenFileName(const CString& sBase, const CString& sName)
{
    int iBlank;
    CString sFile = _T("ref_");

    if (!sBase.IsEmpty())
    {
        iBlank = sBase.Find(' ');
        if (iBlank >= 0)
        {
            sFile.Append(sBase, iBlank);
        }
        else
        {
            sFile += sBase;
        }
        if (!sName.IsEmpty())
        {
            sFile += _T("_");
        }
    }

    iBlank = sName.Find(' ');
    if (iBlank >= 0)
    {
        sFile.Append(sName, iBlank);
    }
    else
    {
        sFile += sName;
    }

    sFile += ".htm";

    return sFile.MakeLower();
}

CString TrueRef(PCTSTR pszRef)
{
    PCTSTR trueRef = pszRef;
    while (!_istalpha(*trueRef))
    {
        ++trueRef;
    }

    return trueRef;
}

enum DoxyParaToHtmlFlags
{
    WantsPara = 1,
    NextWordIsRef = 2,
    InTable = 4,
    InPara = 8,
};

CString DoxyParaToHtml(
    MSXML2::IXMLDOMNodePtr node,
    const CString& sBaseName,
    CReferenceMap& refMap,
    UINT& uFlags,
    PCTSTR& pszRef)
{
    CString s;
    CString sTag;
    CString sClass;

    switch (node->nodeType)
    {
    case MSXML2::NODE_ELEMENT:
    {
        PCWSTR nodeName = node->nodeName;
        if (wcscmp(nodeName, L"bold") == 0)
        {
            sTag = _T("b");
        }
        else if ((uFlags & WantsPara) && wcscmp(nodeName, L"para") == 0)
        {
            sTag = _T("p");
            uFlags |= InPara;
        }
        else if (wcscmp(nodeName, L"anchor") == 0)
        {
            pszRef = node->attributes->getNamedItem(L"id")->text;
            ++refMap[pszRef];
            uFlags |= NextWordIsRef;
        }
        else if (wcscmp(nodeName, L"table") == 0)
        {
            sTag = _T("table");
            uFlags &= ~WantsPara;
            if (uFlags & InPara)
            {
                s.TrimRight();
                s += _T("</p>");
                uFlags &= ~InPara;
            }
        }
        else if (wcscmp(nodeName, L"row") == 0)
        {
            sTag = _T("tr");
        }
        else if (wcscmp(nodeName, L"entry") == 0)
        {
            sTag = _T("td");
            if (wcscmp(node->attributes->getNamedItem(L"thead")->text, L"yes") == 0)
            {
                sClass = _T("hdr");
            }
        }
        else if (wcscmp(nodeName, L"emphasis") == 0)
        {
            sTag = _T("i");
        }
        break;
    }

    case MSXML2::NODE_TEXT:
        if (uFlags & NextWordIsRef)
        {
            int iSep;
            CString sAnchor, sRemText;

            sRemText = (PCTSTR)node->text;
            iSep = sRemText.FindOneOf(_T("., "));
            if (iSep > 0)
            {
                sAnchor.SetString(sRemText, iSep);
                sRemText.Delete(0, iSep);
            }
            else
            {
                sAnchor = sRemText;
                sRemText.Empty();
            }

            if (refMap[pszRef] > 1)
            {
                // already referenced
                s.AppendFormat(_T("<b>%s</b>%s"), sAnchor, sRemText);
            }
            else
            {
                // referenced for the 1st time
                s.AppendFormat(_T("<a href=\"%s\">%s</a>%s"), GenFileName(_T(""), TrueRef(pszRef)), sAnchor, sRemText);
            }

            uFlags &= ~NextWordIsRef;
            pszRef = NULL;
        }
        else
        {
            s += (PCTSTR)node->text;
        }
        return s;

    default:
        return s;
    }

    if (!sTag.IsEmpty())
    {
        s += _T("<");
        s += sTag;
        if (!sClass.IsEmpty())
        {
            s.AppendFormat(_T(" class=\"%s\""), sClass);
        }
        s += _T(">");
    }

    if (node->childNodes)
    {
        MSXML2::IXMLDOMNodePtr child;
        long length = node->childNodes->length;

        for (long i = 0; i < length; i++)
        {
            child = node->childNodes->Getitem(i);
            s += DoxyParaToHtml(child, sBaseName, refMap, uFlags, pszRef);
        }
    }

    if (!sTag.IsEmpty())
    {
        if (sTag == _T("p") && !(uFlags & InPara))
        {
            return s;
        }

        if (sTag == _T("p") || sTag == _T("td"))
        {
            s.TrimRight();
        }
        s += _T("</");
        s += sTag;
        s += _T(">");
    }

    return s;
}

CString DoxyParaToHtml(MSXML2::IXMLDOMNodePtr node, const CString& sBaseName, CReferenceMap& refMap, const UINT uFlags = 0)
{
    UINT uFlagsRef = uFlags;
    PCTSTR nil = NULL;
    return DoxyParaToHtml(node, sBaseName, refMap, uFlagsRef, nil);
}

bool SaveToFile(const CString& sHtml, const CString& sBase, const CString& sName)
{
    CPath sFileName;
    HANDLE hFile;
    CW2A sHtml8(sHtml, CP_UTF8);
    DWORD cbLen, cbWritten;
    CPath sFilePath;

    sFileName.m_strPath = GenFileName(sBase, sName);
    sFilePath += sFileName;

    hFile = CreateFile(sFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    cbLen = strlen(sHtml8);
    WriteFile(hFile, (const char*)sHtml8, cbLen, &cbWritten, NULL);
    CloseHandle(hFile);

    _tprintf(_T("%s written.\n"), sFileName);

    return true;
}

bool SaveIndex(const CSortedAtlList<Link>& index, const CString& sBase, const CString& sName)
{
    CPath sFileName;
    FILE* file;
    CPath sFilePath;
    CPath sDir(_T("hh\\automation\\"));
    POSITION pos;

    sFileName.m_strPath = GenFileName(sBase, sName);
    sFileName.RenameExtension(_T(".hhk"));
    sFilePath += sFileName;

    _tfopen_s(&file, sFilePath, _T("w+"));

    _ftprintf(file,
              _T("\t<LI> <OBJECT type=\"text/sitemap\">\n")
              _T("\t\t<param name=\"Name\" value=\"%s\">\n")
              _T("\t\t<param name=\"Name\" value=\"%s\">\n")
              _T("\t\t<param name=\"Local\" value=\"%s%s\">\n")
              _T("\t\t</OBJECT>\n")
              _T("\t<UL>\n"),
              sName,
              sName,
              sDir, GenFileName(sBase, sName));

    pos = index.GetHeadPosition();
    while (pos)
    {
        const Link& link = index.GetNext(pos);
        _ftprintf(file,
                  _T("\t\t<LI> <OBJECT type=\"text/sitemap\">\n")
                  _T("\t\t\t<param name=\"Name\" value=\"%s\">\n")
                  _T("\t\t\t<param name=\"Name\" value=\"%s\">\n")
                  _T("\t\t\t<param name=\"Local\" value=\"%s%s\">\n")
                  _T("\t\t\t</OBJECT>\n"),
                  link.sName,
                  sName,
                  sDir, link.sHref);
    }

    _ftprintf(file,
              _T("\t</UL>\n"));

    fclose(file);

    _tprintf(_T("%s written.\n"), sFileName);

    return true;
}

bool SaveContents(const CSortedAtlList<Link>& index, const CString& sBase, const CString& sName)
{
    CPath sFileName;
    FILE* file;
    CPath sFilePath;
    CPath sDir(_T("hh\\automation\\"));
    POSITION pos;

    sFileName.m_strPath = GenFileName(sBase, sName);
    sFileName.RenameExtension(_T(".hhc"));
    sFilePath += sFileName;

    _tfopen_s(&file, sFilePath, _T("w+"));

    _ftprintf(file,
              _T("\t<LI> <OBJECT type=\"text/sitemap\">\n")
              _T("\t\t<param name=\"Name\" value=\"%s\">\n")
              _T("\t\t<param name=\"Local\" value=\"%s%s\">\n")
              _T("\t\t</OBJECT>\n")
              _T("\t<UL>\n"),
              sName,
              sDir, GenFileName(sBase, sName));

    pos = index.GetHeadPosition();
    while (pos)
    {
        const Link& link = index.GetNext(pos);
        if (link.bLong)
        {
            _ftprintf(file,
                      _T("\t\t<LI> <OBJECT type=\"text/sitemap\">\n")
                      _T("\t\t\t<param name=\"Name\" value=\"%s\">\n")
                      _T("\t\t\t<param name=\"Local\" value=\"%s%s\">\n")
                      _T("\t\t\t</OBJECT>\n"),
                      link.sName,
                      sDir, link.sHref);
        }
    }

    _ftprintf(file,
              _T("\t</UL>\n"));

    fclose(file);

    _tprintf(_T("%s written.\n"), sFileName);

    return true;
}

CString MakePretty(const CString& s)
{
    CString sPretty = s;

    sPretty.SetAt(0, _totupper(sPretty[0]));

    int i;
    i = sPretty.Find(_T("dialog"));
    if (i >= 0)
    {
        sPretty.SetAt(i, _totupper(sPretty[i]));
    }

    i = sPretty.Find(_T("window"));
    if (i >= 0)
    {
        sPretty.SetAt(i, _totupper(sPretty[i]));
    }

    i = sPretty.Find(_T("coll"));
    if (i >= 0)
    {
        sPretty.Delete(i, sPretty.GetLength() - i);
        sPretty += _T(" Collection");
        return sPretty;
    }

    if (sPretty.Find(_T("Object") < 0))
    {
        sPretty += _T(" Object");
    }

    return sPretty;
}

#if 0
MSHTML::IHTMLDOMNodePtr FindHhkObject(MSHTML::IHTMLDOMNodePtr body, const CString &sName)
{
	MSHTML::IHTMLDOMNodePtr ul = body->firstChild;
	MSHTML::IHTMLDOMChildrenCollectionPtr lis = ul->childNodes;
	MSHTML::IHTMLDOMNodePtr li;
	CString s;
	long c = lis->length;
	MSHTML::IHTMLDOMNodePtr object;
	MSHTML::IHTMLDOMNodePtr param;
	MSHTML::IHTMLAttributeCollection2Ptr attrs;
	MSHTML::IHTMLDOMNodePtr childul;
	MSHTML::IHTMLDOMChildrenCollectionPtr lichildren;

	for (long i = 0; i < c; i++)
	{
		li = lis->item(i);
		//s = (PCTSTR)li->nodeName;
		lichildren = li->childNodes;
		long clich = lichildren->length;
		object = li->firstChild;
		//s = (PCTSTR)object->nodeName;
		param = object->firstChild;
		attrs = param->attributes;
		_bstr_t value = attrs->getNamedItem(L"value")->nodeValue;
		if (sName.Compare(value) == 0)
		{
			childul = object->nextSibling;
			s = (PCTSTR)childul->nodeName;
			
			return li;
		}
	}

	return NULL;
}
#endif

bool ProcessObject(MSXML2::IXMLDOMNodePtr nodeRoot, const CString& sBase, ProcessType type /*, MSHTML::IHTMLElement5Ptr hhkbodyel = NULL*/)
{
    CString sHtml;
    CString sName;
    CString sDescr;
    CString sNavPath;
    MSXML2::IXMLDOMElementPtr el;
    MSXML2::IXMLDOMNodeListPtr list;
    MSXML2::IXMLDOMElementPtr el2;
    CSortedAtlList<CString> strlist;
    POSITION pos;
    CString sMemberName, sMemberDescr, sMemberLongDescr;
    CString sxpath;
    CReferenceMap refMap;
    CSortedAtlList<Link> index;

    if (type == TypeObject)
    {
        el = nodeRoot->selectSingleNode(L"briefdescription");
        sName = (PCTSTR)el->text;
        sName.TrimRight(_T(". "));
        sName.TrimLeft();
    }
    else
    {
        el = nodeRoot->selectSingleNode(L"name");
        sName = (PCTSTR)el->text;
        if (type == TypeMethod)
        {
            sName += _T(" Method");
        }
        else
        {
            sName += _T(" Property");
        }
    }

    if (type == TypeObject)
    {
        el = nodeRoot->selectSingleNode(L"detaileddescription/para[count(simplesect)=0]");
    }
    else
    {
        el = nodeRoot->selectSingleNode(L"detaileddescription/para[count(simplesect)=0]");
        if (!el)
        {
            el = nodeRoot->selectSingleNode(L"briefdescription/para");
        }
    }
    sDescr = DoxyParaToHtml(el, sName, refMap, WantsPara).Trim();

    // header
    if (!sBase.IsEmpty())
    {
        sNavPath.Format(_T(": %s"), sBase);
    }
    sHtml.Format(HTML_HEADER, sName, sNavPath, sName, sDescr);

    if (type == TypeObject)
    {
        //MSHTML::IHTMLElement5Ptr objEl = FindHhkObject(hhkbodyel, sName);

        // methods
        list = nodeRoot->selectNodes(L"sectiondef[@kind='public-func']/memberdef");
        if (list->length > 0)
        {
            sHtml += _T("\n");
            sHtml += HTML_METHODS;
            sHtml += HTML_NAMETABLE_BEGIN;

            strlist.RemoveAll();
            while ((el = list->nextNode()) != NULL)
            {
                Link link;

                el2 = el->selectSingleNode(L"name");
                strlist.Insert((PCTSTR)el2->text);
                link.sName = (PCTSTR)el2->text;
                link.sName += _T(" Method");

                sxpath.Format(L"sectiondef[@kind='public-func']/memberdef[name='%s']/detaileddescription", (PCTSTR)el2->text);
                el2 = nodeRoot->selectSingleNode((PCWSTR)sxpath);
                sMemberLongDescr = (PCWSTR)el2->text;
                if (!sMemberLongDescr.Trim().IsEmpty())
                {
                    if (!ProcessObject(el, sName, TypeMethod))
                    {
                        return false;
                    }

                    link.sHref = GenFileName(sName, link.sName);
                    link.bLong = true;
                }
                else
                {
                    link.sHref = GenFileName(sBase, sName);
                }

                index.Insert(link);
            }

            pos = strlist.GetHeadPosition();
            while (pos)
            {
                sMemberName = strlist.GetNext(pos);

                sxpath.Format(L"sectiondef[@kind='public-func']/memberdef[name='%s']/briefdescription", sMemberName);
                el = nodeRoot->selectSingleNode((PCWSTR)sxpath);
                sMemberDescr = DoxyParaToHtml(el, sName, refMap).Trim();

                sxpath.Format(L"sectiondef[@kind='public-func']/memberdef[name='%s']/detaileddescription", sMemberName);
                el = nodeRoot->selectSingleNode((PCWSTR)sxpath);
                sMemberLongDescr = (PCWSTR)el->text;
                if (!sMemberLongDescr.Trim().IsEmpty())
                {
                    sMemberName.Format(_T("<a href=\"%s\">%s</a>"), GenFileName(sName, sMemberName), sMemberName);
                }
                sHtml.AppendFormat(HTML_NAMETABLE_METHOD, sMemberName, sMemberDescr);
            }

            sHtml += HTML_NAMETABLE_END;
        }

        // props
        list = nodeRoot->selectNodes(L"sectiondef[@kind='property']/memberdef");
        if (list->length > 0)
        {
            sHtml += _T("\n");
            sHtml += HTML_PROPERTIES;
            sHtml += HTML_NAMETABLE_BEGIN;

            strlist.RemoveAll();
            while ((el = list->nextNode()) != NULL)
            {
                Link link;

                el2 = el->selectSingleNode(L"name");
                PCTSTR pszPropName = (PCTSTR)el2->text;
                if (_tcscmp(pszPropName, _T("_NewEnum")) == 0)
                {
                    continue;
                }
                strlist.Insert(pszPropName);
                link.sName = (PCTSTR)el2->text;
                link.sName += _T(" Property");

                sxpath.Format(L"sectiondef[@kind='property']/memberdef[name='%s']/detaileddescription", (PCTSTR)el2->text);
                el2 = nodeRoot->selectSingleNode((PCWSTR)sxpath);
                sMemberLongDescr = (PCWSTR)el2->text;
                if (!sMemberLongDescr.Trim().IsEmpty())
                {
                    if (!ProcessObject(el, sName, TypeProperty))
                    {
                        return false;
                    }

                    link.sHref = GenFileName(sName, link.sName);
                    link.bLong = true;
                }
                else
                {
                    link.sHref = GenFileName(sBase, sName);
                }

                index.Insert(link);
            }

            pos = strlist.GetHeadPosition();
            while (pos)
            {
                sMemberName = strlist.GetNext(pos);

                sxpath.Format(L"sectiondef[@kind='property']/memberdef[name='%s']/briefdescription", sMemberName);
                el = nodeRoot->selectSingleNode((PCWSTR)sxpath);
                sMemberDescr = DoxyParaToHtml(el, sName, refMap).Trim();

                sxpath.Format(L"sectiondef[@kind='property']/memberdef[name='%s']/detaileddescription", sMemberName);
                el = nodeRoot->selectSingleNode((PCWSTR)sxpath);
                sMemberLongDescr = (PCWSTR)el->text;
                if (!sMemberLongDescr.Trim().IsEmpty())
                {
                    sMemberName.Format(_T("<a href=\"%s\">%s</a>"), GenFileName(sName, sMemberName), sMemberName);
                }
                sHtml.AppendFormat(HTML_NAMETABLE_PROPERTY, sMemberName, sMemberDescr);
            }

            sHtml += HTML_NAMETABLE_END;
        }
    }

    // syntax
    int cParams = 0;
    if (type == TypeMethod)
    {
        CString sSyntax;
        int cArgs = 0;

        sSyntax = (PCTSTR)nodeRoot->selectSingleNode(L"name")->text;
        sSyntax += _T("(");

        list = nodeRoot->selectNodes(L"param");
        while ((el = list->nextNode()) != NULL)
        {
            try
            {
                CString sAttrs = (PCTSTR)el->selectSingleNode(L"attributes")->text;
                if (sAttrs.Find(_T("retval")) < 0)
                {
                    if (cArgs++ > 0)
                    {
                        sSyntax += _T(", ");
                    }

                    bool bOptional = sAttrs.Find(_T("optional")) >= 0;
                    if (bOptional)
                    {
                        sSyntax += _T("[");
                    }

                    PCTSTR pszName = el->selectSingleNode(L"declname")->text;
                    while (*pszName == _T('_'))
                    {
                        ++pszName;
                    }

                    sSyntax += _T("<i>");
                    sSyntax += pszName;
                    sSyntax += _T("</i>");

                    if (bOptional)
                    {
                        sSyntax += _T("]");
                    }
                }
                ++cParams;
            }
            catch (_com_error e)
            {
                // probably void argument list
                break;
            }
        }

        sSyntax += _T(")");

        sHtml.AppendFormat(HTML_METHOD_SYNTAX, sSyntax);
    }

    // parameters
    if (type == TypeMethod && cParams > 0)
    {
        list = nodeRoot->selectNodes(L"param");
        if (list->length > 0)
        {
            CString sParamDescr;
            sHtml += HTML_METHODPARAMS;

            long c = list->length;
            for (long i = 0; i < c; i++)
            {
                el = list->Getitem(i);

                try
                {
                    CString sAttrs = (PCTSTR)el->selectSingleNode(L"attributes")->text;
                    if (sAttrs.Find(_T("retval")) < 0)
                    {
                        CString sOrigName = (PCTSTR)el->selectSingleNode(L"declname")->text;
                        PCTSTR pszName = sOrigName;
                        while (*pszName == _T('_'))
                        {
                            ++pszName;
                        }

                        sxpath.Format(L"detaileddescription/para/parameterlist/parameteritem[parameternamelist/parametername='%s']/parameterdescription", sOrigName);
                        el2 = nodeRoot->selectSingleNode((PCWSTR)sxpath);
                        sParamDescr = DoxyParaToHtml(el2, sBase, refMap, WantsPara).Trim();

                        sHtml.AppendFormat(HTML_METHODPARAM, pszName, sParamDescr);
                    }
                }
                catch (_com_error e)
                {
                    // probably void argument list
                    break;
                }
            }
        }
    }

    // (return) values
    if (type != TypeObject)
    {
        el = nodeRoot->selectSingleNode(L"detaileddescription/para/simplesect[@kind='return']");
        if (el)
        {
            if (type == TypeMethod)
            {
                sHtml += HTML_RETURNVALUES;
            }
            else
            {
                sHtml += HTML_PROPERTYVALUES;
            }

            sHtml += DoxyParaToHtml(el, sBase, refMap, WantsPara).Trim();
        }
    }

    // remarks
    el = nodeRoot->selectSingleNode(L"detaileddescription/para/simplesect[@kind='remark']");
    if (el)
    {
        sHtml += HTML_REMARKS;
        sHtml += DoxyParaToHtml(el, sBase, refMap, WantsPara).Trim();
    }

    // see also
    list = nodeRoot->selectNodes(L"detaileddescription/para/simplesect[@kind='see']/para/anchor");
    while ((el = list->nextNode()) != NULL)
    {
        ++refMap[el->attributes->getNamedItem(L"id")->text];
    }

    if (!refMap.IsEmpty() || type != TypeObject)
    {
        sHtml += HTML_SEEALSO;

        if (type != TypeObject)
        {
            // backlink to the object
            --refMap[sBase].i;
        }

        CSortedAtlList<Link> links;
        pos = refMap.GetStartPosition();
        while (pos)
        {
            const CReferenceMap::CPair* pair = refMap.GetNext(pos);
            const CString& sRef = pair->m_key;
            CString sTrueRef = TrueRef(sRef);
            links.Insert(Link(GenFileName(_T(""), sTrueRef), pair->m_value.i > 0 ? MakePretty(sTrueRef) : pair->m_key));
        }

        pos = links.GetHeadPosition();
        while (pos)
        {
            const Link& link = links.GetNext(pos);
            sHtml.AppendFormat(HTML_SEEALSO_LINK, link.sHref, link.sName);
        }
    }

    // footer
    sHtml += HTML_FOOTER;

    if (!SaveToFile(sHtml, sBase, sName))
    {
        return false;
    }

    if (type == TypeObject)
    {
        if (!SaveIndex(index, sBase, sName))
        {
            return false;
        }

        if (!SaveContents(index, sBase, sName))
        {
            return false;
        }
    }

    return true;
}

bool ProcessFile(const CPath& path)
{
    HRESULT hr;
    MSXML2::IXMLDOMDocument2Ptr doc;
    _bstr_t url = L"file:///";
    /*MSHTML::IHTMLDocument3Ptr index;
	IPersistStreamInitPtr streamInit;
	IStreamPtr indexstream;
	MSHTML::IHTMLElement5Ptr bodyel;
	MSHTML::IHTMLElementCollectionPtr bodycoll;
	MSHTML::IHTMLElementCollectionPtr ulcoll;*/

    url += (PCWSTR)path;

    hr = doc.CreateInstance(__uuidof(MSXML2::DOMDocument60));
    if (FAILED(hr))
    {
        _com_issue_error(hr);
    }

    doc->preserveWhiteSpace = VARIANT_TRUE;
    doc->async = VARIANT_FALSE;

    if (doc->load(url) != VARIANT_TRUE)
    {
        return false;
    }

    /*hr = index.CreateInstance(__uuidof(MSHTML::HTMLDocument));
	if (FAILED(hr))
	{
		_com_issue_error(hr);
	}

	streamInit = index;
	hr = SHCreateStreamOnFile(g_sIndexHHK, STGM_READWRITE, &indexstream);
	if (FAILED(hr))
	{
		_com_issue_error(hr);
	}

	hr = streamInit->InitNew();
	if (FAILED(hr))
	{
		_com_issue_error(hr);
	}

	hr = streamInit->Load(indexstream);
	if (FAILED(hr))
	{
		_com_issue_error(hr);
	}

	
	MSHTML::IHTMLDocument2Ptr index2 = index;
	for (;;)
	{
		_bstr_t bstrReady;
		bstrReady = index2->readyState;
		if (wcscmp(bstrReady, L"loading") != 0)
			break;
		//while( doc.readyState != "complete" )

		MSG msg;
		while ( ::PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
		{
			::GetMessage(&msg, NULL, 0, 0);
			::DispatchMessage(&msg);
		}
	}

	bodycoll = index->getElementsByTagName(L"body");
	long cbody = bodycoll->length;
	bodyel = bodycoll->item(vtMissing, 0);*/

    MSXML2::IXMLDOMNodeListPtr objects = doc->selectNodes(L"/doxygen/compounddef");
    MSXML2::IXMLDOMNodePtr nodeObj;
    while ((nodeObj = objects->nextNode()) != NULL)
    {
        if (!ProcessObject(nodeObj, _T(""), TypeObject /*, bodyel*/))
        {
            return false;
        }
    }

    /*	hr = streamInit->Save(indexstream, TRUE);
	if (FAILED(hr))
	{
		_com_issue_error(hr);
	}*/

    return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
    int res = 1;

    g_sIndexHHK.m_strPath = _T("automation.hhk");

    if (argc > 1)
    {
        CPath path = argv[1];

        if (path.FileExists())
        {
            CoInitialize(NULL);

            if (ProcessFile(path))
            {
                res = 0;
            }

            CoUninitialize();
        }
        else
        {
            _ftprintf(stderr, _T("'%s' not found.\n"));
        }
    }
    else
    {
        _ftprintf(stderr, _T("No input.\n"));
        res = 1;
    }

    return res;
}
