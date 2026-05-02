if ($Salamander->SourcePanel->SelectedItems->Count == 0)
{
	if ($Salamander->MsgBox('No items are selected. Do you want to make list from all the items?', 4, 'Question') != 6)
	{
		exit;
	}
	$Items = $Salamander->SourcePanel->Items;
}
else
{
	$Items = $Salamander->SourcePanel->SelectedItems;
}
