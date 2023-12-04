/*
	Make List (PHP).phps
	Creates the CSV list from the selected items.

	Sample Open Salamander Automation Script.

	www.manison.cz
*/

// Pick the collection of items to make the list from.
if ($Salamander->SourcePanel->SelectedItems->Count == 0)
{
	if ($Salamander->MsgBox('No items are selected. Do you want to make list from all items in the panel?', 4, 'Question') != 6)
	{
		exit;
	}
	$Items = $Salamander->SourcePanel->Items;
}
else
{
	$Items = $Salamander->SourcePanel->SelectedItems;
}

// Open the target file.
$ListName = getenv('TEMP') . '\\FileList.csv';
$File = fopen($ListName, 'w');

// Write down the header.
fprintf($File, "Name;Size;Date;Attributes\n");

// Iterate through the item collection and record info about each of them into the file.
foreach ($Items as $Item)
{
	if ($Item->Name != '..')
	{
		fprintf($File, "$Item->Name;$Item->Size;$Item->DateLastModified;$Item->Attributes\n");
	}
}

// Close the file.
fclose($File);

// Deselect all items that could be eventually selected.
$Salamander->SourcePanel->DeselectAll();

// Focus created file in the other panel.
$Salamander->TargetPanel->Path = $ListName;

if ($Salamander->MsgBox("List $ListName was successfully created. Do you want to open it now?", 4, 'Finished') == 6)
{
	// Open newly created list in the associated application.
	shell_exec($ListName);
}
