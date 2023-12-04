<?

include("inifiles.php");

function write_file($filename, &$content)
{
    if (file_exists($filename))
    {
        $h = fopen($filename, "r");
        $old_content = fread($h, strlen($content)+1);
        fclose($h);
    }
    else
    {
        $old_content = NULL;
    }

    if ($old_content != $content)
    {
        $h = fopen($filename, "w+");
        assert($h);
        $r = fwrite($h, $content);
        assert($r !== false);
        fclose($h);
    }
}

//ob_implicit_flush();

$EOL = "\r\n";
$TO_TRANSLATE = "<translate>";
$COMMENT = "<comment>";
$OBSOLETE = "Obsolete Strings";
$OVERRIDE = "<override>";

error_reporting(E_ALL);
assert_options(ASSERT_ACTIVE, 1);
assert_options(ASSERT_BAIL, 1);
assert_options(ASSERT_WARNING, 1);

if (empty($_SERVER["argv"][1]))
{
    die("Syntax: translate.php <inifile> [orig|(language1 language2 ...)] [projectdest] [version]\r\n");
}

$inifile = $_SERVER["argv"][1];

if (!file_exists($inifile))
{
    die("$inifile neexistuje\r\n");
}

$config = parse_ini_file_ext($inifile);

$project = $config["General"]["Project"];
$forms_dest = $config["General"]["Forms Dest"];
$resources_dest = $config["General"]["Resources Dest"];
$forms_source = $config["General"]["Forms Source"];
$resources_source = $config["General"]["Resources Source"];
$setup_source = $config["General"]["Setup Source"];
$setup_dest = $config["General"]["Setup Dest"];
$setup_dest_complete = $config["General"]["Setup Dest Complete"];
$setup_dest_incomplete = $config["General"]["Setup Dest Incomplete"];
$parameter_names = explode(",", $config["General"]["Parameters"]);
$default_codepage = $config["General"]["Default CodePage"];
$output_dir = $config["General"]["Output"];
$original_lang = $config["General"]["Original Lang"];
$ignored_file = $config["General"]["Ignored File"];
$translations_path = $config["General"]["Translations Path"];
$dest_filename = $config["General"]["Dest FileName"];
$product_version_str = $config["General"]["Version"];
$product_version = explode(".",
    preg_replace("/([0-9])/", "\$1.", str_pad($product_version_str, 4, "0")));

echo
  "Forms=$forms_source\r\n".
  "Resources=$resources_source\r\n".
  "Setup=$setup_source\r\n".
  "---\r\n";

$default_strings = array();
foreach ($config["Default strings"] as $default_file_key => $default_value)
{
    if (preg_match("/^([\w.]+)_(\w+)$/", $default_file_key, $matches))
    {
        $default_strings[$matches[1]][$matches[2]] = $default_value;;
    }
}

if (!empty($_SERVER["argv"][2]))
{
    if ($_SERVER["argv"][2] == "orig")
    {
        $langs = array(NULL);
    }
    else
    {
        $lang = $_SERVER["argv"][2];
        assert(!empty($config["Languages"][$lang]));
        $langs[] = $lang;
    }
}
else
{
    $langs = array_keys($config["Languages"]);
}

$project_dest = empty($_SERVER["argv"][3]) ? NULL : $_SERVER["argv"][3];

$translation_version =
    empty($_SERVER["argv"][4]) ? NULL : $_SERVER["argv"][4];

if (empty($project_dest))
{
    if ($langs != array(NULL))
    {
        $original_strings = array_merge_recursive(
            parse_ini_file_ext($output_dir . $original_lang.".ini", true),
            parse_ini_file_ext($output_dir . $ignored_file, true));
    }
}

foreach ($langs as $lang)
{
    if ($lang)
    {
        $used_strings = array();

        unset($lang_parameters);
        $parameter_values = explode(",", $config["Languages"][$lang]);
        foreach ($parameter_names as $index => $parameter_name)
        {
            $lang_parameters[$parameter_name] = $parameter_values[$index];
        }
        krsort($lang_parameters);
        if (empty($translation_version))
        {
            $lang_version = $config["Version"][$lang];
        }
        $lang_author = explode(",", $config["Author"][$lang]);

        if (empty($project_dest))
        {
            unset($translated_strings);
            $lang_file_path = $translations_path.$lang.".ini";
            if (file_exists($lang_file_path))
            {
                $translated_strings = parse_ini_file_ext($lang_file_path, true);
            }
            else
            {
                $translated_strings = array();
            }

            foreach ($config["Copy"] as $copy_dest => $copy_source)
            {
                $r = preg_match("/^(.*?(_DRC\.rc)?)_(.*)$/", $copy_dest, $matches);
                assert($r);
                $copy_dest_file = $matches[1];
                $copy_dest_key = $matches[3];

                $r = preg_match("/^(.*?(_DRC\.rc)?)_(.*)$/", $copy_source, $matches);
                assert($r);
                $copy_source_file = $matches[1];
                $copy_source_key = $matches[3];

                if ((empty($translated_strings[$copy_dest_file][$copy_dest_key]) ||
                     ($translated_strings[$copy_dest_file][$copy_dest_key] == $TO_TRANSLATE)) &&
                    !empty($translated_strings[$copy_source_file][$copy_source_key]) &&
                    ($translated_strings[$copy_source_file][$copy_source_key] != $TO_TRANSLATE))
                {
                    $translated_strings[$copy_dest_file][$copy_dest_key] =
                        $translated_strings[$copy_source_file][$copy_source_key];
                }
            }

            foreach ($default_strings as $default_file => $default_file_strings)
            {
                foreach ($default_file_strings as $default_key => $default_value)
                {
                    foreach ($lang_parameters as $parameter_name => $parameter_value)
                    {
                        $default_value = str_replace($parameter_name,
                            $parameter_value, $default_value);
                    }

                    $translated_strings[$default_file][$default_key] = $default_value;
                }
            }
        }

        $lang = strtoupper($lang);
        $langl = strtolower($lang);

        if (empty($project_dest))
        {
            $lang_dir = $config["General"]["Output"].$lang."\\";

            if (file_exists($lang_dir))
            {
                $r = exec("rd $lang_dir /s /q");
                assert(!$r);
            }

            $r = mkdir($lang_dir);
            assert($r);

            $r = mkdir($lang_dir.$forms_dest);
            assert($r);

            $r = mkdir($lang_dir.$resources_dest);
            assert($r);

            $r = mkdir($lang_dir.$setup_dest);
            assert($r);

            if (!empty($config["Mkdir"]))
            {
                foreach ($config["Mkdir"] as $mkdir)
                {
                    $r = mkdir($lang_dir.$mkdir);
                    assert($r);
                }
            }
        }

        $lang_ext = "";
    }
    else if (!empty($project_dest))
    {
        $lang_author = array(2003, date("Y"), "Martin Prikryl");
        if (!empty($config["General"]["Original Parameters"]))
        {
          unset($lang_parameters);
          $parameter_values = explode(",", $config["General"]["Original Parameters"]);
          foreach ($parameter_names as $index => $parameter_name)
          {
              $lang_parameters[$parameter_name] = $parameter_values[$index];
          }
          krsort($lang_parameters);
        }
        else
        {
          $lang_parameters = array(
              "LangName" => "English"
          );
        }
        $lang_dir = $project_dest."\\";
        $lang_ext = ".lang";
    }

    if (empty($project_dest))
    {
        $stats = array(
            "new" => 0,
            "obsolete_old" => isset($translated_strings[$OBSOLETE]) ?
                count($translated_strings[$OBSOLETE]) : 0,
            "obsolete_new" => 0,
            "translated" => 0,
            "ignored" => 0,
            "override" => 0,
            "total" => 0,
            "to_translate" => 0,
        );
    }

    $resource_strings = NULL;

    $main_form_files = NULL;

    foreach ($config["Forms"] as $form_file => $form_desc)
    {
        $form_filename = $form_file;
        if (strrpos($form_filename, "\\"))
        {
            $form_filename =
                substr($form_filename, strrpos($form_filename, "\\") + 1);
        }

        if (empty($project_dest))
        {
            $resource_strings[$form_filename][$COMMENT] = $form_desc;

            $content = file($forms_source.$form_file);

            $context = array();
            $item_counter = 0;
            $array_counter = 0;
            $in_array = false;
            $multiline_string = NULL;

            $new_content = array();

            foreach ($content as $line)
            {
                $continue_multiline = false;

                $new_line = $line;
                $used_string = NULL;

                if (preg_match("/^\s*((object)|(inherited)|(inline)) ".
                        "((\w+):\s*)?(\w+)(\s*\[\d+\])?\s*$/i", $line, $matches))
                {
                    $name = !empty($matches[6]) ? $matches[6] : $matches[7];
                    assert(!empty($name));
                    $context[] = $name;
                    $item_counter = 0;
                }
                else if (preg_match("/^\s*item\s*$/i", $line))
                {
                    $context[] = "item".$item_counter;
                    $item_counter++;
                }
                else if (preg_match("/^\s*end>?\s*$/i", $line))
                {
                    assert(count($context));
                    $context = array_slice($context, 0, -1);
                }
                else if (preg_match("/^\s*([\w.]+)\s+=\s+\(\s*$/i",
                            $line, $matches))
                {
                    $context[] = $matches[1];
                    $array_counter = 0;
                    $in_array = true;
                }
                else if ($in_array &&
                    preg_match("/(\s*)\'(.*)\'(\s*(\))?\s*)$/", $line, $matches))
                {
                    $string_key = implode(".", array_slice($context, 1)).
                        ".".$array_counter;
                    $resource_strings[$form_filename][$string_key] = $matches[2];

                    if ($lang)
                    {
                        if (isset($translated_strings[$form_filename][$string_key]) &&
                            $translated_strings[$form_filename][$string_key] !=
                                $TO_TRANSLATE)
                        {
                            $value = $translated_strings[$form_filename][$string_key];
                        }
                        else if (isset($original_strings[$form_filename][$string_key]))
                        {
                            $value = $original_strings[$form_filename][$string_key];
                        }
                        else
                        {
                            $value = $resource_strings[$form_filename][$string_key];
                        }
                        $used_string = $string_key;
                        $new_line = $matches[1]."'$value'".$matches[3];
                    }

                    if (empty($matches[4]))
                    {
                        $array_counter++;
                    }
                    else
                    {
                        $context = array_slice($context, 0, -1);
                        $array_counter = 0;
                        $in_array = false;
                    }
                }
                else if (preg_match("/^(\s*([\w.]+)\s+=)(\s+'(.*)')?(\s*)$/i",
                            $line, $matches))
                {
                    assert(count($context));
                    $string_key = implode(".", array_slice($context, 1)).
                        (count($context) > 1 ? "." : NULL).$matches[2];

                    if (empty($matches[3]))
                    {
                        $multiline_string = $string_key;
                        $resource_strings[$form_filename][$multiline_string] = NULL;
                        $continue_multiline = true;
                    }
                    else
                    {
                        $resource_strings[$form_filename][$string_key] = $matches[4];
                    }

                    if ($lang)
                    {
                        if (isset($translated_strings[$form_filename][$string_key]) &&
                            $translated_strings[$form_filename][$string_key] !=
                                $TO_TRANSLATE)
                        {
                            $value = $translated_strings[$form_filename][$string_key];
                        }
                        else if (isset($original_strings[$form_filename][$string_key]))
                        {
                            $value = $original_strings[$form_filename][$string_key];
                        }
                        else
                        {
                            $value = $resource_strings[$form_filename][$string_key];
                        }
                        $used_string = $string_key;
                        $new_line = $matches[1]." '$value'".$matches[5];
                    }
                }
                else if (preg_match("/\s*(\'.*\'(#\d+)*)(\s*\+)?\s*$/", $line, $matches) &&
                    !empty($multiline_string))
                {
                    assert(strstr($matches[1], "\"") === false);
                    $continue_multiline = !empty($matches[3]);
                    $next_multiline = $matches[1];
                    if ($next_multiline[0] == "'")
                    {
                        $next_multiline = substr($next_multiline, 1);
                    }
                    if ($next_multiline[strlen($next_multiline) - 1] == "'")
                    {
                        $next_multiline = substr($next_multiline, 0, -1);
                    }
                    if ($continue_multiline && !empty($matches[2]))
                    {
                        $next_multiline .= "'";
                    }
                    $resource_strings[$form_filename][$multiline_string] .=
                        $next_multiline;
                    $new_line = NULL;
                }

                if ($in_array && preg_match("/[^\(]+\)\s*/", $line))
                {
                    $context = array_slice($context, 0, -1);
                    $array_counter = 0;
                    $in_array = false;
                }

                if (!$continue_multiline)
                {
                    $multiline_string = NULL;
                }

                if ($lang && isset($new_line))
                {
                    $new_content[] = $new_line;
                    if (!empty($used_string))
                    {
                        $used_strings[$form_filename][$used_string] = true;
                    }
                }
            }
        }

        if ($lang && empty($project_dest))
        {
            $h = fopen($lang_dir.$forms_dest.$form_file, "w+");
            assert($h);
            $r = fwrite($h, implode($new_content));
            assert($r !== false);
            fclose($h);
        }

        if ($lang || !empty($project_dest))
        {
            $forms_path_normalized =
                str_replace("\\", "\\\\",
                preg_replace("/\w+\\\\\\.\\.\\\\/", "", $forms_dest.$form_file));

            $main_form_files .=
                "#pragma resource \"$forms_path_normalized\"\r\n";
        }
    }

    $res_files = array();
    $project_resource_files = NULL;
    $resources_list = array_merge($config["Resources"],
        array(NULL => $config["General"]["DRC Comment"]));

    foreach ($resources_list as $resource_file => $resource_desc)
    {
        if (empty($resource_file))
        {
            $resource_filename = $project."_DRC.rc";
            $resource_path = $config["General"]["DRC Source"].$project.".drc";
            if ($lang)
            {
                $resource_path_dest = $lang_dir.$project."_DRC.rc";
            }
        }
        else
        {
            $resource_filename = $resource_file;
            if (strrpos($resource_filename, "\\"))
            {
                $resource_filename =
                    substr($resource_filename, strrpos($resource_filename, "\\") + 1);
            }
            $resource_path = $resources_source.$resource_file;
            if ($lang)
            {
                $resource_path_dest = $lang_dir.$resources_dest.$resource_file;
            }

            $resource_name = substr($resource_file, 0,
                strrpos($resource_file, "."));
            $res_file = $resources_dest.$resource_name.".res";
            $res_files[] = $res_file;
        }
        $resource_strings[$resource_filename][$COMMENT] = $resource_desc;

        if (empty($project_dest))
        {
            $content = file($resource_path);

            $context = 0;
            $multiline_string = NULL;

            $new_content = array();

            foreach ($content as $line)
            {
                $new_line = $line;
                $used_string = NULL;

                $continue_multiline = false;
                if (preg_match("/^STRINGTABLE\s*$/i", $line))
                {
                    assert($context == 0);
                    $context = 1;
                }
                if (preg_match("/^\w+\s+DIALOG\s/i", $line))
                {
                    assert($context == 0);
                    $context = 3;
                }
                else if (preg_match("/^BEGIN\s*$/i", $line))
                {
                    if ($context == 1)
                    {
                        $context = 2;
                    }
                    else if ($context == 3)
                    {
                        // noop
                    }
                    else
                    {
                        assert(false);
                    }
                }
                else if (preg_match("/^END\s*/i", $line))
                {
                    $context = 0;
                }
                else if ($context == 2)
                {
                    if (preg_match("/^(\s+(\w+),)(\s+\"(.*)\")?(\s*)$/", $line,
                            $matches))
                    {
                        if (empty($matches[3]))
                        {
                            $multiline_string = $matches[2];
                            $continue_multiline = true;
                            $resource_strings[$resource_filename][$matches[2]] =
                                NULL;
                        }
                        else
                        {
                            $resource_strings[$resource_filename][$matches[2]] =
                                $matches[4];
                        }

                        if ($lang)
                        {
                            if (isset($translated_strings[$resource_filename]
                                    [$matches[2]]) &&
                                $translated_strings[$resource_filename]
                                    [$matches[2]] != $TO_TRANSLATE)
                            {
                                $value = $translated_strings[$resource_filename]
                                    [$matches[2]];
                            }
                            else if (isset($original_strings[$resource_filename]
                                [$matches[2]]))
                            {
                                $value = $original_strings
                                    [$resource_filename][$matches[2]];
                            }
                            else
                            {
                                $value = $resource_strings
                                    [$resource_filename][$matches[2]];
                            }
                            $used_string = $matches[2];
                            $new_line = $matches[1]." \"$value\"".$matches[5];
                        }
                    }
                    else if (preg_match("/^\s*\"(.*)\"\s*$/", $line, $matches) &&
                        !empty($multiline_string))
                    {
                        $resource_strings[$resource_filename][$multiline_string] .=
                            $matches[1];
                        $continue_multiline = true;
                        $new_line = NULL;
                    }
                }
                else if ($context == 3)
                {
                    // noop
                }

                if (!$continue_multiline)
                {
                    $multiline_string = NULL;
                }

                if ($lang && isset($new_line))
                {
                    $new_content[] = $new_line;
                    if (!empty($used_string))
                    {
                        $used_strings[$resource_filename][$used_string] = true;
                    }
                }
            }

            // sort DRC file as order of its content is constantly changing
            if (empty($resource_file))
            {
                ksort($resource_strings[$resource_filename]);
            }

            if ($lang)
            {
                $h = fopen($resource_path_dest, "w+");
                assert($h);
                $r = fwrite($h, implode($new_content));
                assert($r !== false);
                fclose($h);
            }
        }
    }

    if (empty($project_dest))
    {
        $setup_complete = NULL;

        foreach ($config["Setup"] as $setup_file => $setup_desc)
        {
            //$resource_filename = $resource_file;
            $setup_path = $setup_source.$setup_file;
            $resource_strings[$setup_file][$COMMENT] = $setup_desc;

            $content = file($setup_path);
            $new_content = array();

            foreach ($content as $line)
            {
                $new_line = $line;
                $user_string = NULL;
                if (preg_match("/^(;)?(\w+)=(.*?)(\s+)$/", $line, $matches))
                {
                    assert(strstr($matches[3], "`") === false);
                    $resource_strings[$setup_file][$matches[2]] =
                        str_replace("\"", "`", $matches[3]);

                    if ($lang)
                    {
                        $value = NULL;
                        if (isset($translated_strings[$setup_file][$matches[2]]) &&
                            $translated_strings[$setup_file][$matches[2]] !=
                                $TO_TRANSLATE)
                        {
                            $value = $translated_strings[$setup_file][$matches[2]];
                        }
                        else if (empty($matches[1]))
                        {
                            if (isset($original_strings[$setup_file][$matches[2]]))
                            {
                                $value = $original_strings[$setup_file][$matches[2]];
                            }
                            else
                            {
                                $value = $resource_strings[$setup_file][$matches[2]];
                            }
                        }

                        if (isset($value))
                        {
                            $value = str_replace("`", "\"", $value);
                            $new_line = $matches[2]."=".$value.$matches[4];
                        }

                        $used_string = $matches[2];
                    }
                }

                $new_content[] = $new_line;

                if ($lang)
                {
                    if (!empty($used_string))
                    {
                        $used_strings[$setup_file][$used_string] = true;
                    }
                }
            }

            if ($lang)
            {
                $setup_path_dest = $lang_dir.$setup_dest.$setup_file;
                $h = fopen($setup_path_dest, "w+");
                assert($h);
                $r = fwrite($h, implode($new_content));
                assert($r !== false);
                fclose($h);
            }

            $setup_complete .= implode($new_content);
        }
    }

    if ($lang || !empty($project_dest))
    {
        if (!empty($config["General"]["Aggregate Resource"]))
        {
            $res_files_str = $config["General"]["Aggregate Resource"];
        }
        else
        {
            $res_files_all = $res_files;
            if (!empty($config["OtherResources"]))
            {
                $res_files_all = array_merge($res_files_all,
                    array_keys($config["OtherResources"]));
            }

            $res_files_str = NULL;
            foreach ($res_files_all as $res_file)
            {
                $res_file =
                    preg_replace("/\w+\\\\\\.\\.\\\\/", "", $res_file);
                if (!empty($res_files_str))
                {
                    $res_files_str .= " ";
                }
                $res_files_str .= $res_file;
            }
        }

        if ($lang && !empty($lang_parameters["CodePage"]))
        {
            $codepage = $lang_parameters["CodePage"];
        }
        else
        {
            $codepage = $default_codepage;
        }

        if ($lang && !empty($lang_parameters["LangId"]))
        {
            $langid = $lang_parameters["LangId"];
        }
        else
        {
            $langid = $config["General"]["Original LangId"];
        }

        if ($lang)
        {
            $dll_name = $project.".".$langl;
            $dll_path = $dll_name;
            $lang_code = strtolower($lang);
        }
        else
        {
            $dll_name = "english.slg";
            $dll_path = "lang\\".$dll_name;
            $lang_code = "en";
        }

        $dest_dll_name = $dest_filename;
        foreach ($lang_parameters as $parameter_name => $parameter_value)
        {
            $dest_dll_name = str_replace($parameter_name,
                strtolower($parameter_value), $dest_dll_name);
        }

        $dest_dll_name = str_replace("LangCode",
            $lang_code, $dest_dll_name);

        $lang_author_names =
            (count($lang_author) > 3 ?
                implode(", ", array_slice($lang_author, 2, -1))." and " : NULL).
                $lang_author[count($lang_author) - 1];
        $copyright_file = sprintf("(c) %s %s",
            ($lang_author[0] == $lang_author[1] ? $lang_author[0] :
                sprintf("%d-%d", $lang_author[0], $lang_author[1])),
            $lang_author_names);

        $version_custom = NULL;
        if (!empty($config["FileInfo"]))
        {
            foreach ($config["FileInfo"] as $version_key => $version_value)
            {
                foreach ($lang_parameters as $parameter_name => $parameter_value)
                {
                    $version_value = str_replace($parameter_name,
                        $parameter_value, $version_value);
                }
                $version_value = str_replace("AuthorName",
                    $lang_author_names, $version_value);
                
                $version_custom .=
                    "            VALUE \"$version_key\", \"$version_value\\0\"\n";
            }
            $version_custom = rtrim($version_custom);
        }

        if (!empty($translation_version))
        {
            $file_version = str_replace(".", ",", $translation_version);
            $file_version_value = $translation_version;
        }
        else
        {
            $file_version = "1,$lang_version,0,0";
            $file_version_value = "1.$lang_version";
        }

        $resource_tag =
            !empty($config["General"]["Resource Tag"]) ?
            $config["General"]["Resource Tag"] :
            ($langid . str_pad(dechex($codepage), 4, "0", STR_PAD_LEFT));

        $lang_id = empty($lang) ? $original_lang : $lang;

        $version_file = <<<RCEND
1 VERSIONINFO
 FILEVERSION $file_version
 PRODUCTVERSION $product_version[0],$product_version[1],$product_version[2],$product_version[3]
 FILEFLAGSMASK 0x0L
 FILEFLAGS 0x0L
 FILEOS 0x4L
 FILETYPE 0x2L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "$resource_tag"
        BEGIN
            VALUE "CompanyName", "Martin Prikryl\\0"
            VALUE "FileDescription", "$lang_parameters[LangName] translation of WinSCP ($lang_id)\\0"
            VALUE "FileVersion", "$file_version_value\\0"
            VALUE "LegalCopyright", "$copyright_file\\0"
            VALUE "OriginalFilename", "$dest_dll_name\\0"
            VALUE "ProductVersion", "$product_version[0].$product_version[1].$product_version[2].$product_version[3]\\0"
            VALUE "WWW", "http://winscp.net/\\0"
            VALUE "LangName", "$lang_parameters[LangName]\\0"
$version_custom
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x$langid, $codepage
    END
END
RCEND;

        $h = fopen($lang_dir.$project.$lang_ext.".rc", "w+");
        assert($h);
        $r = fwrite($h, $version_file);
        assert($r !== false);
        fclose($h);

        $main_form_files = rtrim($main_form_files);

        if (!empty($config["General"]["DllMain"]))
        {
            $dll_main = <<<END
#include <windows.h>
extern "C" int APIENTRY
DllMain(HINSTANCE /*HInstance*/, DWORD /*Reason*/, LPVOID /*Reserved*/)
{
  return 1;   // ok
}

END;

        }
        else
        {
            $dll_main = "#define DllEntryPoint";
        }

        $main_file = <<<END
//---------------------------------------------------------------------------
#pragma hdrstop
int _turboFloat;
//---------------------------------------------------------------------------
$main_form_files
//---------------------------------------------------------------------------
$dll_main
END;

        $h = fopen($lang_dir.$project.$lang_ext.".cpp", "w+");
        assert($h);
        $r = fwrite($h, $main_file);
        assert($r !== false);
        fclose($h);
    }

    if (empty($project_dest))
    {
        $ignored = NULL;
        $all = NULL;
        $to_translate = NULL;

        /*if (file_exists($output_ini))
        {
            $r = copy($output_ini, $config["General"]["Backup Dir"].$output_ini);
            assert($r);
        } */

        unset($file_strings);
        if (!$lang)
        {
            $original_strings = $resource_strings;
            $files = array_keys($original_strings);
        }
        else
        {
            $files = array_keys(array_merge($original_strings,
                $translated_strings));
        }

        $output_content = NULL;

        $obsolete = array();
        foreach ($files as $file)
        {
            if ($file == $OBSOLETE)
            {
                continue;
            }

            $file_header = "; ".
                (isset($resource_strings[$file][$COMMENT]) ?
                    $resource_strings[$file][$COMMENT] : "Unknown file").$EOL.
                "[$file]".$EOL;
            $ignored_lines = NULL;
            $all_lines = NULL;

            $first = true;
            $first_to_translate = true;

            $strings = array();
            if (!empty($original_strings[$file]))
            {
                $strings = $original_strings[$file];
            }
            if ($lang && !empty($translated_strings[$file]))
            {
                $strings = array_merge($strings,
                    $translated_strings[$file]);
            }

            foreach (array_keys($strings) as $key)
            {
                if ($key != $COMMENT)
                {
                    $translated = false;
                    $stat_key = NULL;
                    if ($lang)
                    {
                        if (!isset($translated_strings[$file][$key]) ||
                            $translated_strings[$file][$key] == $TO_TRANSLATE)
                        {
                            if (!isset($translated_strings[$file][$key]))
                            {
                                $stat_key = "new";
                            }
                            $value = $TO_TRANSLATE;
                            $string = NULL;
                        }
                        else if (!isset($default_strings[$file][$key]))
                        {
                            $value = "\"".$translated_strings[$file][$key]."\"";
                            $string = $translated_strings[$file][$key];
                            $translated = true;
                            $stats["translated"]++;
                        }
                        $ivalue = $value;
                    }
                    else
                    {
                        $value = "\"".$original_strings[$file][$key]."\"";
                        $ivalue = "\""./*str_replace("\"", "'",*/
                            $original_strings[$file][$key]/*)*/."\"";
                        $string = $original_strings[$file][$key];
                    }
                    $line = "$key=$value";
                    $all_line = NULL;
                    $ignore = false;
                    if (isset($original_strings[$file][$key]))
                    {
                        $match_line = $file."|"."$key=\"".
                            $original_strings[$file][$key]."\"";
                        foreach ($config["Ignore Strings"] as $ignore_string)
                        {
                            if (preg_match($ignore_string, $match_line))
                            {
                                $ignore = true;
                                break;
                            }
                        }
                        if ($ignore)
                        {
                            foreach ($config["Ignore Exception Strings"] as
                                $ignore_string)
                            {
                                if (preg_match($ignore_string, $match_line))
                                {
                                    $ignore = false;
                                    break;
                                }
                            }
                        }
                    }
                    if ($ignore)
                    {
                        $stats["ignored"]++;
                    }
                    if ((!$ignore || $translated) &&
                        !isset($default_strings[$file][$key]))
                    {
                        if (!empty($stat_key))
                        {
                            $stats[$stat_key]++;
                        }
                        if ($lang && empty($used_strings[$file][$key]))
                        {
                            if ($value != $TO_TRANSLATE)
                            {
                                $obsolete["${file}_$key"] = $string;
                                $stats["translated"]--;
                            }
                        }
                        else
                        {
                            if ($first)
                            {
                                $output_content .= $file_header;
                                $first = false;
                            }
                            //assert(strstr($string, "\"") === false);
                            $comment = NULL;
                            if ($lang)
                            {
                                $comment = "; \"".
                                    $original_strings[$file][$key]."\"".$EOL;
                                if ($ignore)
                                {
                                    assert($translated);
                                    $comment .= "; $OVERRIDE".$EOL;
                                    $stats["override"]++;
                                }
                            }
                            $stats["total"]++;
                            if ($value == $TO_TRANSLATE)
                            {
                                $stats["to_translate"]++;
                                if ($first_to_translate)
                                {
                                    $to_translate .= $file_header;
                                    $first_to_translate = false;
                                }
                                $to_translate .= $comment.$line.$EOL;
                            }
                            $output_content .= $comment.$line.$EOL;
                            $all_line = "$key=1";
                        }
                    }
                    else if (!$lang)
                    {
                        $ignored_lines .= "$key=$ivalue".$EOL;
                        $all_line = "$key=0";
                    }

                    if (!$lang && !empty($all_line))
                    {
                        $all_lines .= $all_line.$EOL;
                    }
                }
            }

            if (!$first)
            {
                $output_content .= $EOL;
            }

            if (!$first_to_translate)
            {
                $to_translate .= $EOL;
            }

            if (!empty($ignored_lines))
            {
                assert(!$lang);
                $ignored .= $file_header.$ignored_lines.$EOL;
            }

            if (!empty($all_lines))
            {
                assert(!$lang);
                $all .= $file_header.$all_lines.$EOL;
            }
        }

        if ($lang)
        {
            if (!empty($translated_strings[$OBSOLETE]))
            {
                $obsolete = array_merge($translated_strings[$OBSOLETE], $obsolete);
            }

            if (!empty($obsolete))
            {
                $output_content .=
                    "; Translated strings that are no longer used in ".
                    "current version".$EOL.
                    "[$OBSOLETE]".$EOL;

                foreach ($obsolete as $key => $value)
                {
                    $output_content .= "$key=\"$value\"".$EOL;
                }
            }
        }

        if (!$lang)
        {
            $output_ini = $original_lang.".ini";
            write_file($output_dir . $output_ini, $output_content);
        }
        unset($output_content);

        if (!$lang)
        {
            write_file($output_dir . $ignored_file, $ignored);
            unset($ignored);
        }

        if (!$lang)
        {
            write_file($output_dir . $config["General"]["All File"], $all);
            unset($all);
        }

        $incomplete = !empty($stats["to_translate"]);

        if (!empty($setup_source))
        {
            $setup_lang = strtolower($lang ? $lang : $original_lang);
            $setup_filename = $project.".".$setup_lang.".isl";
            $setup_path_dest_complete = $setup_dest_complete.$setup_filename;
            $setup_path_dest_incomplete = $setup_dest_incomplete.$setup_filename;
            if (file_exists($setup_path_dest_complete))
            {
                $r = unlink($setup_path_dest_complete);
                assert($r);
            }
            if (file_exists($setup_path_dest_incomplete))
            {
                $r = unlink($setup_path_dest_incomplete);
                assert($r);
            }
            $h = fopen($incomplete ? $setup_path_dest_incomplete : $setup_path_dest_complete, "w+");
            assert($h);
            $r = fwrite($h, $setup_complete);
            assert($r !== false);
            fclose($h);
        }

        if ($lang)
        {
            if (!empty($config["Copy files"]))
            {
                foreach ($config["Copy files"] as $source_file => $dest_file)
                {
                    copy($source_file, $lang_dir.$dest_file);
                }
            }
        }
    }

    if ($lang || !empty($project_dest))
    {
        if (!empty($config["General"]["DllMain"]))
        {
            $make_file_obj = "c0d32.obj";
            $make_file_lib = "import32.lib cw32mt.lib";
        }
        else
        {
            $make_file_obj = "";
            $make_file_lib = "";
        }

        $lflags = !empty($config["General"]["LFlags"]) ?
            " ".$config["General"]["LFlags"] : NULL;

        $make_file = <<<END
# ---------------------------------------------------------------------------
PROJECT = $dll_path
BCB = $(MAKEDIR)\..
OBJFILES = {$project}${lang_ext}.obj
RESFILES = {$project}_DRC.res ${project}${lang_ext}.res $res_files_str
# ---------------------------------------------------------------------------
LFLAGS = -D"" -aa -Tpd -x -Gn${lflags}
INCLUDE = $(BCB)\include;$(BCB)\include\mfc
# ---------------------------------------------------------------------------
ALLOBJ = $(OBJFILES) $make_file_obj
ALLLIB = $make_file_lib
# ---------------------------------------------------------------------------
$(PROJECT): $(RESFILES) $(OBJFILES)
 $(MAKEDIR)\ilink32 @&&!
 $(LFLAGS) $(ALLOBJ), $(PROJECT),, $(ALLLIB),, $(RESFILES)
!
# ---------------------------------------------------------------------------
.cpp.obj:
 $(MAKEDIR)\bcc32 -c -n$(@D) {\$< }

.rc.res:
 $(MAKEDIR)\brcc32 -c$codepage -fo$@ -I$(INCLUDE) \$<
# ---------------------------------------------------------------------------
END;

        $h = fopen($lang_dir."makefile".$lang_ext, "w+");
        assert($h);
        $r = fwrite($h, $make_file);
        assert($r !== false);
        fclose($h);
    }

    if ($lang)
    {
        exec("maketran.bat ${output_dir}${lang}", $dump, $r);
        if ($r || !file_exists($lang_dir.$dll_name) ||
            !empty($config["General"]["Output Translation Build"]))
        {
            echo implode($EOL, $dump);
        }
        assert(!$r);

        $tds_name = $lang_dir.$project.".tds";
        $r = unlink($tds_name);
        assert($r);

        $dll_dest_path = $config["General"]["DLL Dest Complete"].$dest_dll_name;
        $dll_dest_path_incomplete = $config["General"]["DLL Dest Incomplete"].$dest_dll_name;
        if (file_exists($dll_dest_path))
        {
            $r = unlink($dll_dest_path);
            assert($r);
        }
        if (file_exists($dll_dest_path_incomplete))
        {
            $r = unlink($dll_dest_path_incomplete);
            assert($r);
        }

        if (!$incomplete)
        {
            $r = rename($lang_dir.$dll_name, $dll_dest_path);
        }
        else
        {
            $r = rename($lang_dir.$dll_name, $dll_dest_path_incomplete);
        }
        assert($r);

        $obsolete = count($obsolete);
        $obsolete_new = $obsolete - $stats["obsolete_old"];
        echo "$lang language:\r\n";

        foreach ($lang_parameters as $parameter_name => $parameter_value)
        {
            echo "  $parameter_name: $parameter_value\r\n";
        }

        echo
             "  Total: $stats[total] (incl. overrides)\r\n".
             "  Ignored: $stats[ignored] ($stats[override] overides)\r\n".
             "  Obsolete: $obsolete ($stats[obsolete_old] old, $obsolete_new new)\r\n".
             "  Translated: $stats[translated] (incl. overrides)\r\n".
             "  To translate: $stats[to_translate] ($stats[new] new)\r\n".
             "  Copyright: $copyright_file\r\n".
             (!empty($lang_version) ? "  Version: 1.$lang_version\r\n" : "");

        if (empty($config["General"]["Keep Translated Project"]))
        {
            $r = exec("rd $lang_dir /s /q");
            assert(!$r);
        }
        else
        {
            write_file($lang_dir . "to_translate.ini", $to_translate);
        }
    }
    else if (empty($project_dest))
    {
        echo "Original language:\r\n".
             "  Total: $stats[total]\r\n".
             "  Ignored: $stats[ignored]\r\n";
    }
    echo "---\r\n";
    //flush();
}

?>