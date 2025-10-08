// Test case for ticket #25 - setting the text late
var fm = Salamander.Forms.Form();
fm.dst = Salamander.Forms.TextBox();
fm.Text = "aaa"; // put_Text
var textWas = fm.Text; // get_Text
fm.Execute();
Salamander.TraceI("Test case " + Salamander.Script.Name + ": text was set to \"" + textWas + "\" (must be \"aaa\")");
