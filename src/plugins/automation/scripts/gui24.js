// testuje vicenasobne prirazeni controlu (tiket #24)
var fm = Salamander.Forms.Form();
fm.dst = Salamander.Forms.TextBox("aaa");
fm.dst = Salamander.Forms.TextBox("bbb");
fm.Execute();
