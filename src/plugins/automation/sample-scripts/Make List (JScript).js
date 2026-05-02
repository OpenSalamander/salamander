/*
	Make List (JScript).js
	Creates the CSV list from the selected items.

	Sample Open Salamander Automation Script.

	www.manison.cz
*/

var Items = null;

// Pick the collection of items to make the list from.
if (Salamander.SourcePanel.SelectedItems.Count == 0)
{
	if (Salamander.MsgBox("No items are selected. Do you want to make list from all items in the panel?", 4, "Question") == 6)
	{
  		Items = Salamander.SourcePanel.Items;
	}
}
else
{
	Items = Salamander.SourcePanel.SelectedItems;
}

if (Items != null)
{
        var Fso = new ActiveXObject("Scripting.FileSystemObject");
	var WshShell = new ActiveXObject("WScript.Shell");

	// Open the target file.
	var ListName = WshShell.ExpandEnvironmentStrings("%TEMP%\\FileList.csv");
	var File = Fso.CreateTextFile(ListName, true);

	// Write down the header.
	File.WriteLine("Name;Size;Date;Attributes");

	// Iterate through the item collection and record info about each of them into the file.
	var e = new Enumerator(Items);
	for (; !e.atEnd(); e.moveNext())
	{
		var Item = e.item();
		if (Item.Name != "..")
		{
			File.WriteLine(Item.Name + ";" + Item.Size + ";" + Item.DateLastModified + ";" + Item.Attributes);
		}
	}

	// Close the file.
	File.Close();

	// Deselect all items that could be eventually selected.
	Salamander.SourcePanel.DeselectAll();

	// Focus created file in the other panel.
	Salamander.TargetPanel.Path = ListName;

	if (Salamander.MsgBox("List " + ListName + " was successfully created. Do you want to open it now?", 4, "Finished") == 6)
	{
		// Open newly created list in the associated application.
		WshShell.Run(ListName);
	}
}
