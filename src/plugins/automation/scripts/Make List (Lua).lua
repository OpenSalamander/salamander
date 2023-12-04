--
--	Make List (Lua).lua
--	Creates the CSV list from the selected items.
--
--	Sample Open Salamander Automation Script.
--
--	www.manison.cz
--

-- Pick the collection of items to make the list from.
if Salamander.SourcePanel.SelectedItems.Count == 0 then
	if Salamander:MsgBox("No items are selected. Do you want to make list from all items in the panel?", 4, "Question") == 6 then
  		Items = Salamander.SourcePanel.Items
	end
else
	Items = Salamander.SourcePanel.SelectedItems
end

if Items ~= nil then
	-- Open the target file.
	local ListName = os.getenv("TEMP") .. "\\FileList.csv"
	local File = io.open(ListName, "w")

	-- Write down the header.
	File:write("Name;Size;Date;Attributes\n")

	-- Iterate through the item collection and record info about each of them into the file.
	for i = 0, Items.Count - 1 do
		local Item = Items:Item(i)
		if Item.Name ~= ".." then
			File:write(Item.Name, ";", Item.Size, ";", os.date("%c", os.time(Item.DateLastModified)), ";", Item.Attributes, "\n")
		end
	end

	-- Close the file.
	File:close()

	-- Deselect all items that could be eventually selected.
	Salamander.SourcePanel:DeselectAll()

	-- Focus created file in the other panel.
	Salamander.TargetPanel.Path = ListName

	if Salamander:MsgBox("List " .. ListName .. " was successfully created. Do you want to open it now?", 4, "Finished") == 6 then
		-- Open newly created list in the associated application.
		os.execute(ListName)
	end
end
