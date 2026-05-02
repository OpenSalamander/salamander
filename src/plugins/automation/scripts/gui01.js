var fm = Salamander.Forms.Form();
//fm.Text = "Zkouska";
//fm.edit = null;
//fm.edit = 3;
//fm.edit = Salamander.Forms.Form();
fm.chk1 = Salamander.Forms.CheckBox();
fm.chk1.Text = "Ahoj";
fm.chk2 = Salamander.Forms.CheckBox();
fm.chk3 = Salamander.Forms.CheckBox();
fm.chk2.Text = "svete";
fm.chk2.Checked = true;
fm.chk3.Text = "1234";
fm.lbl1 = Salamander.Forms.Label();
fm.lbl1.Text = "Enter some &text:";
fm.edt1 = Salamander.Forms.TextBox();
fm.okbtn = Salamander.Forms.Button();
fm.okbtn.Text = "Okay";
fm.okbtn.DialogResult = 1;
fm.cancelbtn = Salamander.Forms.Button();
fm.cancelbtn.Text = "Dismiss";
fm.cancelbtn.DialogResult = 2;
fm.btn3 = Salamander.Forms.Button();
fm.btn3.Text = "3rd button";
fm.btn3.DialogResult = 3;
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