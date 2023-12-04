Salamander.WaitWindow.Title = "Wait Window Test";
Salamander.WaitWindow.Text = "Please wait more 10s...";
Salamander.WaitWindow.Show();
for (i = 0; i < 10; i++)
{
	if (Salamander.WaitWindow.IsCancelled)
	{
		break;
	}
	Salamander.Sleep(1000);
	Salamander.WaitWindow.Text = "Please wait more "+(9-i)+"s...";
	if (Salamander.WaitWindow.IsCancelled)
	{
		break;
	}
}
Salamander.WaitWindow.Hide();

Salamander.ProgressDialog.Title = "Progress test";
Salamander.ProgressDialog.Style = 2;
//Salamander.ProgressDialog.
Salamander.ProgressDialog.Maximum = 100;
Salamander.ProgressDialog.TotalMaximum = 1000;
//Salamander.ProgressDialog.AddText("Prepare...");
Salamander.ProgressDialog.Show();
//Salamander.ProgressDialog.Style = 1;
for (i = 0; i < 10; i++)
{
	Salamander.ProgressDialog.Position = i*10;
	//Salamander.ProgressDialog.Maximum = 100 + 5*i;
	Salamander.ProgressDialog.Step(10);
	Salamander.ProgressDialog.TotalPosition = i*100;
	Salamander.ProgressDialog.AddText("Line " + i);
	Salamander.Sleep(100);
	if (Salamander.ProgressDialog.HasUserCancelled)
	{
		Salamander.MsgBox("Cancelled!");
		Salamander.AbortScript();
	}
}
