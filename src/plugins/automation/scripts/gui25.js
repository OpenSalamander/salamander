// test case pro tiket #25 - pozdni nastaveni textu
var fm = Salamander.Forms.Form();
fm.dst = Salamander.Forms.TextBox();
fm.Text = "aaa"; // put_Text
var textWas = fm.Text; // get_Text
fm.Execute();
Salamander.TraceI("Test case " + Salamander.Script.Name + ": text was set to \"" + textWas + "\" (must be \"aaa\")");
