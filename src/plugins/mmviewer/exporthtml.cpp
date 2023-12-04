// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mmviewer.rh"
#include "mmviewer.rh2"
#include "lang\lang.rh"
#include "output.h"
#include "renderer.h"
#include "mmviewer.h"
#include "auxtools.h"

static const char* entities[128] =
    {
        NULL,     /* 000 */
        NULL,     /* 001 */
        NULL,     /* 002 */
        NULL,     /* 003 */
        NULL,     /* 004 */
        NULL,     /* 005 */
        NULL,     /* 006 */
        NULL,     /* 007 */
        NULL,     /* 008 */
        NULL,     /* 009 */
        NULL,     /* 010 */
        NULL,     /* 011 */
        NULL,     /* 012 */
        NULL,     /* 013 */
        NULL,     /* 014 */
        NULL,     /* 015 */
        NULL,     /* 016 */
        NULL,     /* 017 */
        NULL,     /* 018 */
        NULL,     /* 019 */
        NULL,     /* 020 */
        NULL,     /* 021 */
        NULL,     /* 022 */
        NULL,     /* 023 */
        NULL,     /* 024 */
        NULL,     /* 025 */
        NULL,     /* 026 */
        NULL,     /* 027 */
        NULL,     /* 028 */
        NULL,     /* 029 */
        NULL,     /* 030 */
        NULL,     /* 031 */
        NULL,     /* 032 */
        NULL,     /* 033 */
        "&quot;", /* 034 */
        NULL,     /* 035 */
        NULL,     /* 036 */
        NULL,     /* 037 */
        "&amp;",  /* 038 */
        NULL,     /* 039 */
        NULL,     /* 040 */
        NULL,     /* 041 */
        NULL,     /* 042 */
        NULL,     /* 043 */
        NULL,     /* 044 */
        NULL,     /* 045 */
        NULL,     /* 046 */
        NULL,     /* 047 */
        NULL,     /* 048 */
        NULL,     /* 049 */
        NULL,     /* 050 */
        NULL,     /* 051 */
        NULL,     /* 052 */
        NULL,     /* 053 */
        NULL,     /* 054 */
        NULL,     /* 055 */
        NULL,     /* 056 */
        NULL,     /* 057 */
        NULL,     /* 058 */
        NULL,     /* 059 */
        "&lt;",   /* 060 */
        NULL,     /* 061 */
        "&gt;",   /* 062 */
        NULL,     /* 063 */
        NULL,     /* 064 */
        NULL,     /* 065 */
        NULL,     /* 066 */
        NULL,     /* 067 */
        NULL,     /* 068 */
        NULL,     /* 069 */
        NULL,     /* 070 */
        NULL,     /* 071 */
        NULL,     /* 072 */
        NULL,     /* 073 */
        NULL,     /* 074 */
        NULL,     /* 075 */
        NULL,     /* 076 */
        NULL,     /* 077 */
        NULL,     /* 078 */
        NULL,     /* 079 */
        NULL,     /* 080 */
        NULL,     /* 081 */
        NULL,     /* 082 */
        NULL,     /* 083 */
        NULL,     /* 084 */
        NULL,     /* 085 */
        NULL,     /* 086 */
        NULL,     /* 087 */
        NULL,     /* 088 */
        NULL,     /* 089 */
        NULL,     /* 090 */
        NULL,     /* 091 */
        NULL,     /* 092 */
        NULL,     /* 093 */
        NULL,     /* 094 */
        NULL,     /* 095 */
        NULL,     /* 096 */
        NULL,     /* 097 */
        NULL,     /* 098 */
        NULL,     /* 099 */
        NULL,     /* 100 */
        NULL,     /* 101 */
        NULL,     /* 102 */
        NULL,     /* 103 */
        NULL,     /* 104 */
        NULL,     /* 105 */
        NULL,     /* 106 */
        NULL,     /* 107 */
        NULL,     /* 108 */
        NULL,     /* 109 */
        NULL,     /* 110 */
        NULL,     /* 111 */
        NULL,     /* 112 */
        NULL,     /* 113 */
        NULL,     /* 114 */
        NULL,     /* 115 */
        NULL,     /* 116 */
        NULL,     /* 117 */
        NULL,     /* 118 */
        NULL,     /* 119 */
        NULL,     /* 120 */
        NULL,     /* 121 */
        NULL,     /* 122 */
        NULL,     /* 123 */
        NULL,     /* 124 */
        NULL,     /* 125 */
        NULL,     /* 126 */
        NULL      //,		/* 127 */
#if 0
  NULL,		/* 128 */
  NULL,		/* 129 */
  NULL,		/* 130 */
  NULL,		/* 131 */
  NULL,		/* 132 */
  NULL,		/* 133 */
  NULL,		/* 134 */
  NULL,		/* 135 */
  NULL,		/* 136 */
  NULL,		/* 137 */
  NULL,		/* 138 */
  NULL,		/* 139 */
  NULL,		/* 140 */
  NULL,		/* 141 */
  NULL,		/* 142 */
  NULL,		/* 143 */
  NULL,		/* 144 */
  NULL,		/* 145 */
  NULL,		/* 146 */
  NULL,		/* 147 */
  NULL,		/* 148 */
  NULL,		/* 149 */
  NULL,		/* 150 */
  NULL,		/* 151 */
  NULL,		/* 152 */
  NULL,		/* 153 */
  NULL,		/* 154 */
  NULL,		/* 155 */
  NULL,		/* 156 */
  NULL,		/* 157 */
  NULL,		/* 158 */
  NULL,		/* 159 */
  NULL, //nbsp;",	/* 160 */
  NULL, //iexcl;",	/* 161 */
  NULL, //cent;",	/* 162 */
  NULL, //pound;",	/* 163 */
  NULL, //curren;",	/* 164 */
  NULL, //yen;",	/* 165 */
  NULL, //brvbar;",	/* 166 */
  NULL, //sect;",	/* 167 */
  NULL, //uml;",	/* 168 */
  NULL, //copy;",	/* 169 */
  NULL, //ordf;",	/* 170 */
  NULL, //laquo;",	/* 171 */
  NULL, //not;",	/* 172 */
  NULL, //shy;",	/* 173 */
  NULL, //reg;",	/* 174 */
  NULL, //macr;",	/* 175 */
  NULL, //deg;",	/* 176 */
  NULL, //plusmn;",	/* 177 */
  NULL, //sup;",	/* 178 */
  NULL, //sup;",	/* 179 */
  NULL, //acute;",	/* 180 */
  NULL, //micro;",	/* 181 */
  NULL, //para;",	/* 182 */
  NULL, //middot;",	/* 183 */
  NULL, //cedil;",	/* 184 */
  NULL, //sup;",	/* 185 */
  NULL, //ordm;",	/* 186 */
  NULL, //raquo;",	/* 187 */
  NULL, //frac;",	/* 188 */
  NULL, //frac;",	/* 189 */
  NULL, //frac;",	/* 190 */
  NULL, //iquest;",	/* 191 */
  NULL, //Agrave;",	/* 192 */
  NULL, //Aacute;",	/* 193 */
  NULL, //Acirc;",	/* 194 */
  NULL, //Atilde;",	/* 195 */
  NULL, //Auml;",	/* 196 */
  NULL, //Aring;",	/* 197 */
  NULL, //AElig;",	/* 198 */
  NULL, //Ccedil;",	/* 199 */
  NULL, //Egrave;",	/* 200 */
  NULL, //Eacute;",	/* 201 */
  NULL, //Ecirc;",	/* 202 */
  NULL, //Euml;",	/* 203 */
  NULL, //Igrave;",	/* 204 */
  NULL, //Iacute;",	/* 205 */
  NULL, //Icirc;",	/* 206 */
  NULL, //Iuml;",	/* 207 */
  NULL, //ETH;",	/* 208 */
  NULL, //Ntilde;",	/* 209 */
  NULL, //Ograve;",	/* 210 */
  NULL, //Oacute;",	/* 211 */
  NULL, //Ocirc;",	/* 212 */
  NULL, //Otilde;",	/* 213 */
  NULL, //Ouml;",	/* 214 */
  NULL, //times;",	/* 215 */
  NULL, //Oslash;",	/* 216 */
  NULL, //Ugrave;",	/* 217 */
  NULL, //Uacute;",	/* 218 */
  NULL, //Ucirc;",	/* 219 */
  NULL, //Uuml;",	/* 220 */
  NULL, //Yacute;",	/* 221 */
  NULL, //THORN;",	/* 222 */
  NULL, //szlig;",	/* 223 */
  NULL, //agrave;",	/* 224 */
  NULL, //aacute;",	/* 225 */
  NULL, //acirc;",	/* 226 */
  NULL, //atilde;",	/* 227 */
  NULL, //auml;",	/* 228 */
  NULL, //aring;",	/* 229 */
  NULL, //aelig;",	/* 230 */
  NULL, //ccedil;",	/* 231 */
  NULL, //egrave;",	/* 232 */
  NULL, //eacute;",	/* 233 */
  NULL, //ecirc;",	/* 234 */
  NULL, //euml;",	/* 235 */
  NULL, //igrave;",	/* 236 */
  NULL, //iacute;",	/* 237 */
  NULL, //icirc;",	/* 238 */
  NULL, //iuml;",	/* 239 */
  NULL, //eth;",	/* 240 */
  NULL, //ntilde;",	/* 241 */
  NULL, //ograve;",	/* 242 */
  NULL, //oacute;",	/* 243 */
  NULL, //ocirc;",	/* 244 */
  NULL, //otilde;",	/* 245 */
  NULL, //ouml;",	/* 246 */
  NULL, //divide;",	/* 247 */
  NULL, //oslash;",	/* 248 */
  NULL, //ugrave;",	/* 249 */
  NULL, //uacute;",	/* 250 */
  NULL, //ucirc;",	/* 251 */
  NULL, //uuml;",	/* 252 */
  NULL, //yacute;",	/* 253 */
  NULL, //thorn;",	/* 254 */
  NULL, //yuml;"	/* 255 */
#endif
};

static const char* HTMLConvertToUTF8(const char* chars, bool bUTF8 = false)
{
    static TDirectArray<char> text(1024, 1024);
    int len = (int)strlen(chars);

    text.Count = 0;

    const char* str = bUTF8 ? chars : AnsiToUTF8(chars, len);

    int strLen = (int)strlen(str);

    int i;
    for (i = 0; i < strLen; i++)
    {
        unsigned char c = str[i];

        if ((c >= 128) || (entities[c] == NULL)) // We are in UTF8 -> do not convert >= 128!!
            text.Add(c);
        else
            text.Add(entities[c], (int)strlen(entities[c]));
    }

    text.Add('\0');

    return &text[0];
}

static void AppendText(TDirectArray<char>& output, const char* text)
{
    int len = (int)strlen(text);

    if (len)
        output.Add(text, len);
}

static void WriteHTMLHeader(TDirectArray<char>& output, const char* title)
{
    AppendText(output,
               "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
               "<html>\n"
               "<head>\n"
               "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />\n"
               "<meta name=\"description\" content=\"Audio file information\" />\n"
               "<meta name=\"author\" content=\"Generated by Open Salamander's MultiMedia Viewer (https://www.altap.cz). Author: Tomas Jelinek (http://www.tjelinek.com)\" />\n"
               "<title>");

    AppendText(output, HTMLConvertToUTF8(title));

    AppendText(output,
               "</title>\n"
               "<style type=\"text/css\">\n"
               "* { font-family: Tahoma, Arial, Verdana, Geneva, Helvetica, sans-serif; }\n"
               "a:link { color : #A8062b; }\n"
               "a:visited { color : #000053; }\n"
               "a:active { color : #FF6666; }\n"
               "a:hover { text-decoration : underline; color : #D25102; }\n"
               "body { color: #000000; background-color: #ffffff; margin: 5px; font-size: 11px; }\n"
               "div { padding: 0px; margin: 0px; }\n"
               "td, th { color: #000000; padding: 1px 5px 1px 5px; font-size: 11px; table-layout: fixed; }\n"
               "table { margin-left: auto; margin-right: auto; }\n"
               "table.infoTable { border: 1px solid #acb8bb; margin-bottom: 20px; }\n"
               "table.hlTable { border: 1px solid #acb8bb; margin-top: 10px; margin-bottom: 20px; width: 90%; }\n"
               ".hlTable th { background-color: #f0dfd3; font-size: 13px; text-align: center; padding: 1px 5px 1px 5px; font-weight: 600; margin-bottom: 1px; }\n"
               ".infoTable th { background-color: #ecdfd3; margin-bottom: 1px; }\n"
               ".infoTable td { background-color: #c8dff3; }\n"
               ".infoTable .value { background-color: #ece9d8; color: #000000; font-weight: 600; }\n"
               "h4 { font-size: 14px; text-align: center; }\n"
               "</style>\n"
               "</head>\n");
}

static void WriteHTMLBodyStart(TDirectArray<char>& output)
{
    AppendText(output,
               "<body>\n");
}

static void WriteHTMLBodyEnd(TDirectArray<char>& output)
{
    AppendText(output,
               "</body>\n"
               "</html>\n");
}

static void WriteHTMLSignature(TDirectArray<char>& output)
{
    AppendText(output,
               "<div style=\"text-align: center; font-size: 90%;\">Generated by <b>Open Salamander's MultiMedia Viewer</b> (<a href=\"https://www.altap.cz\">https://www.altap.cz</a>)</div>\n");
}

static void WriteHTMLTableStart(TDirectArray<char>& output, const char* title, BOOL superHeader = FALSE)
{
    superHeader ? AppendText(output, "<table class=\"hlTable\">\n") : AppendText(output, "<table class=\"infoTable\">\n");

    if (title)
    {
        superHeader ? AppendText(output, "<tr><th colspan=\"2\">") : AppendText(output, "<tr><th colspan=\"2\">");

        AppendText(output, HTMLConvertToUTF8(title));
        AppendText(output, "</th></tr>\n");
    }
}

static void WriteHTMLTableLine(TDirectArray<char>& output, const char* name, const char* value, bool bUTF8)
{
    AppendText(output, "<tr><td>");
    AppendText(output, HTMLConvertToUTF8(name));
    AppendText(output, "</td><td class=\"value\">");
    AppendText(output, HTMLConvertToUTF8(value, bUTF8));
    AppendText(output, "</td></tr>\n");
}

static void WriteHTMLTableEnd(TDirectArray<char>& output)
{
    AppendText(output, "</table>\n");
}

int ExportToHTML(const char* fname, COutput& Output)
{
    TDirectArray<char> text(4096, 4096);

    WriteHTMLHeader(text, LoadStr(IDS_AUDIOINFO));
    WriteHTMLBodyStart(text);

    BOOL tableOpen = FALSE;

    int i;
    for (i = 0; i < Output.GetCount(); i++)
    {
        const COutputItem* item = Output.GetItem(i);

        if (item->Name)
        {
            if (item->Value)
            {
                if (!tableOpen)
                {
                    WriteHTMLTableStart(text, 0);
                    tableOpen = TRUE;
                }

                WriteHTMLTableLine(text, item->Name, item->Value, (item->Flags & OIF_UTF8) ? true : false);
            }
            else
            {
                //je to header
                if (tableOpen)
                {
                    WriteHTMLTableEnd(text);
                    tableOpen = FALSE;
                }

                WriteHTMLTableStart(text, item->Name, ((item->Flags & OIF_EMPHASIZE) == OIF_EMPHASIZE));
                tableOpen = TRUE;
            }
        }
        else
        {
            //je to separator - ignoruji
        }
    }

    if (tableOpen)
        WriteHTMLTableEnd(text);

    WriteHTMLSignature(text);
    WriteHTMLBodyEnd(text);

    //zapis do souboru
    BOOL r = FALSE;

    FILE* f = fopen(fname, "wt");
    if (f)
    {
        if (fwrite(&text[0], text.Count, 1, f) < 1)
            return -2;

        fclose(f);

        r = TRUE;
    }
    else
        return -1;

    return r;
}
