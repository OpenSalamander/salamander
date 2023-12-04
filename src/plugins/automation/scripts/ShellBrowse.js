var shellApp = new ActiveXObject("Shell.Application");
var oF = shellApp.BrowseForFolder(0, "Choose the folder", 0);
Salamander.MsgBox(oF);
//for (;;) {}