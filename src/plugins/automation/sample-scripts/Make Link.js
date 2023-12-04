/*
	Make Link.js
	Creates a symbolic link.

	Sample Open Salamander Automation Script.
	Requires Windows Vista or later.

	www.manison.cz
*/

var LinkTarget = Salamander.SourcePanel.FocusedItem.Path;
var TargetIsDir = Salamander.SourcePanel.FocusedItem.Attributes & 0x10;
var LinkPath = Salamander.TargetPanel.Path + "\\" + Salamander.SourcePanel.FocusedItem.Name;
var LinkForm = Salamander.Forms.Form("Make Link");
LinkForm.lbLinkTo = Salamander.Forms.Label("Enter name of the link to " + LinkTarget + ":");
LinkForm.edLinkTo = Salamander.Forms.TextBox(LinkPath);
LinkForm.chHardLink = Salamander.Forms.CheckBox("Hardlink");
LinkForm.chJunction = Salamander.Forms.CheckBox("Junction");
LinkForm.btOk = Salamander.Forms.Button("OK", 1);
LinkForm.btCancel = Salamander.Forms.Button("Cancel", 2);
if (LinkForm.Execute() == 1)
{
	LinkPath = LinkForm.edLinkTo.Text;
}
else
{
	LinkPath = "";
}

if (LinkPath != "")
{
	var ShellApp = new ActiveXObject("Shell.Application");
	var Cmd = "cmd.exe";
	var Args = "/C mklink ";
	
	if (TargetIsDir)
	{
		if (LinkForm.chJunction.Checked)
		{
			Args += "/J ";
		}
		else
		{
			Args += "/D ";
		}
	}
	
	if (LinkForm.chHardLink.Checked)
	{
		Args += "/H ";
	}
	
	Args += "\"" + LinkPath + "\" \"" + LinkTarget + "\"";
	
	//Salamander.MsgBox(Args);
	
	ShellApp.ShellExecute(Cmd, Args, "", "runas");
}
