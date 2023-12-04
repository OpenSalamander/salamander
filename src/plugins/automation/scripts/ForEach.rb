TotalSize = 0
Salamander.SourcePanel.Items.each { |Item| TotalSize += Item.Size }
Salamander.MsgBox("Total size of all items in the panel: " + TotalSize.to_s + " bytes")