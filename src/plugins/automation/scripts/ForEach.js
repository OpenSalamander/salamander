var TotalSize = 0;
var e = new Enumerator(Salamander.SourcePanel.Items);
for (; !e.atEnd(); e.moveNext())
{
	TotalSize += e.item().Size;
}
Salamander.MsgBox("Total size of all items in the panel: " + TotalSize + " bytes");