Dim TotalSize
TotalSize = 0
for each Item in Salamander.SourcePanel.Items
	TotalSize = TotalSize + Item.Size
next
Salamander.MsgBox("Total size of all items in the panel: " + CStr(TotalSize) + " bytes")
