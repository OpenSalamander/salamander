$TotalSize = 0;
foreach ($Salamander->SourcePanel->Items as $Item)
{
	$TotalSize += $Item->Size;
}
$Salamander->MsgBox("Total size of all items in the panel: $TotalSize bytes");