var fm = Salamander.Forms.Form("Forms test");
fm.chk1 = Salamander.Forms.CheckBox("Ahoj");
fm.chk2 = Salamander.Forms.CheckBox("svete");
fm.chk3 = Salamander.Forms.CheckBox("1234");
fm.chk2.Checked = true;
fm.lbl1 = Salamander.Forms.Label("Enter some &text:");
fm.edt1 = Salamander.Forms.TextBox();
fm.okbtn = Salamander.Forms.Button("Okay", 1);
fm.cancelbtn = Salamander.Forms.Button("Dismiss", 2);
fm.btn3 = Salamander.Forms.Button("3rd button");
var res = fm.Execute();
if (res == 1)
{
	var s = "";
	if (fm.chk1.Checked)
	{
		s += fm.chk1.Text + "\n";
	}
	if (fm.chk2.Checked)
	{
		s += fm.chk2.Text + "\n";
	}
	if (fm.chk3.Checked)
	{
		s += fm.chk3.Text + "\n";
	}
	Salamander.MsgBox(s + "\n" + fm.edt1.Text);
}
else if (res == 2)
{
	Salamander.MsgBox("Cancelled");
}
else
{
	Salamander.MsgBox("Unknown");
}