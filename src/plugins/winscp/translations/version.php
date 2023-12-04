<?

$s = file_get_contents("..\\Salamand.rc");
if (!preg_match("/\"FileVersion\", \"(\d+.\d+.\d+.\d+)\\\\0/s", $s, $m))
{
    echo "error";
}
echo $m[1];

?>