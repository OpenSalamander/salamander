<?

function parse_ini_array($lines, $process_sections = false)
{
  $ini_array = array();
  $sec_name = NULL;
  $linenum = 1;

  foreach ($lines as $line)
  {
    $line = trim($line);

    if (!empty($line) && ($line[0] != ";"))
    {
      if (($line[0] == "[") && ($line[strlen($line) - 1] == "]"))
      {
        $sec_name = trim(substr($line, 1, strlen($line) - 2));
        if (empty($sec_name))
        {
          echo "($linenum)";
          return $linenum;
        }

        if (empty($ini_array[$sec_name]))
        {
          $ini_array[$sec_name] = array();
        }
      }
      else
      {
        $pos = strpos($line, "=");
        if ($pos === false)
        {
          echo "($linenum)";
          return $linenum;
        }
        else
        {
          $property = trim(substr($line, 0, $pos));
          $value = trim(substr($line, $pos + 1));

          if (!empty($value) &&
              ($value[0] == "\"") && ($value[strlen($value)-1] == "\""))
          {
            $value = substr($value, 1, strlen($value)-2);
          }

          if (empty($property))
          {
            echo "($linenum)";
            return $linenum;
          }

          if ($process_sections)
          {
            if (empty($sec_name))
            {
              echo "($linenum)";
              return $linenum;
            }

            $ini_array[$sec_name][$property] = $value;
          }
          else
          {
            $ini_array[$property] = $value;
          }
        }
      }
    }
    $linenum++;
  }

  return $ini_array;
}

function parse_ini_file_fix($filename, $process_sections = false)
{
  $lines = file($filename);

  return parse_ini_array($lines, $process_sections);
}

function parse_ini_file_ext($filename)
{
    $config = parse_ini_file_fix($filename, true);

    if (!empty($config["Include"]) && is_array($config["Include"]))
    {
        foreach ($config["Include"] as $include_file)
        {
            $config = array_merge_recursive($config, parse_ini_file_fix(
                $include_file, true));
        }
    }

    return $config;
}

?>