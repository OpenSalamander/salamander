// Tests automatic exit code calculation when the form has no exit button
var fm = Salamander.Forms.Form();
fm.lb = Salamander.Forms.Label();
fm.lb.Text = "I'm the only label on this form.";
fm.bt = Salamander.Forms.Button("Exit", 2);
var result = fm.Execute();
Salamander.MsgBox(result);
