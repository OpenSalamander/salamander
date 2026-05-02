'
'	Make List (VBScript).vbs
'	Creates the CSV list from the selected items.
'
'	Sample Open Salamander Automation Script.
'
'	www.manison.cz
'

Dim Items

' Pick the collection of items to make the list from.
If Salamander.SourcePanel.SelectedItems.Count = 0 Then
	If Salamander.MsgBox("No items are selected. Do you want to make list from all items in the panel?", 4, "Question") = 6 Then
   		Set Items = Salamander.SourcePanel.Items
	End If
Else
	Set Items = Salamander.SourcePanel.SelectedItems
End If

If VarType(Items) <> 0 Then ' vbEmpty
        Dim Fso, WshShell, ListName, Item, File

	Set Fso = CreateObject("Scripting.FileSystemObject")
	Set WshShell = CreateObject("WScript.Shell")

	' Open the target file.
	ListName = WshShell.ExpandEnvironmentStrings("%TEMP%\FileList.csv")
	Set File = Fso.CreateTextFile(ListName, True)

	' Write down the header.
	File.WriteLine("Name;Size;Date;Attributes")

	' Iterate through the item collection and record info about each of them into the file.
	For Each Item In Items
		If Item.Name <> ".." Then
			File.WriteLine(Item.Name & ";" & Item.Size & ";" & Item.DateLastModified & ";" & Item.Attributes)
		End If
	Next

	' Close the file.
	File.Close

	' Deselect all items that could be eventually selected.
	Salamander.SourcePanel.DeselectAll

	' Focus created file in the other panel.
	Salamander.TargetPanel.Path = ListName

	If Salamander.MsgBox("List " & ListName & " was successfully created. Do you want to open it now?", 4, "Finished") = 6 Then
		' Open newly created list in the associated application.
		WshShell.Run(ListName)
	End If
End If
