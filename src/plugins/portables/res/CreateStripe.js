// This script creates image stripe from the individual images specified in
// the 'sources' array. Each image must exist in three sizes - 16, 32 and 48
// pixels (e.g. if there is string 'mediaplayer' in the array, actually
// 'mediaplayer16.png', 'mediaplayer32.png' and 'mediaplayer48.png' files must
// exist. It is also good idea to run the resulting PNGs through PNGslim.

var sources = [
	"camera",
	"mediaplayer",
	"smartphone",
	"videocamera",
	"storage"
];

var outputFileNamePattern = "deviceicons";

var wsh = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject");

function CreateList(sources, baseDir, size) {
	var TemporaryFolder = 2;
	var tmpPath = fso.GetSpecialFolder(TemporaryFolder).Path;
	var fileName = fso.BuildPath(tmpPath, fso.GetTempName());
	//Salamander.MsgBox(fileName);
	var file = fso.CreateTextFile(fileName);
	for (var i = 0; i < sources.length; i++)
	{
		file.WriteLine(fso.BuildPath(baseDir, sources[i] + (size * 1.0) + ".png"));
	}
	file.Close();
	return fileName;
}

function Run(exe, args) {
	wsh.Run(exe + " " + args, 0, true);
}

function CreateStripe(size) {
	var baseDir = fso.GetParentFolderName(Salamander.Script.Path);
	var listFileName = CreateList(sources, baseDir, size);
	var sspackFileName = fso.BuildPath(baseDir, "SpriteSheetPacker\\sspack.exe");
	if (!fso.FileExists(sspackFileName)) {
		Salamander.MsgBox("Download Sprite Sheet Packer from http://spritesheetpacker.codeplex.com/");
		return false;
	}
	var outputFileName = fso.BuildPath(baseDir, outputFileNamePattern + (size * 1.0) + ".png");
	//Salamander.MsgBox(sspackFileName);
	Run(sspackFileName, "\"/image:" + outputFileName + "\" /pad:0 \"/il:" + listFileName + "\" /mh:" + (size * 1.0) + " /nosort");
	fso.DeleteFile(listFileName);
	return true;
}

function CheckVersions() {
  if (typeof(Salamander) == "undefined")
  {
    if (typeof(WScript) != "undefined")
    {
      var infText = "This script requires Open Salamander with Automation plugin installed.\n";
      infText += "Please download latest version of Open Salamander from www.altap.cz.\n";
      infText += "Install it and use menu Plugins > Automation > Run Focused Script to start this script.";
      try
      {
        var WshShell = WScript.CreateObject("WScript.Shell");
        WshShell.Popup(infText, 0, scriptName, 16);
      }
      catch (err)
      {
        WScript.Echo(infText);
      }
    }
    return false;
  }
  return true;
}

if (CheckVersions()) {
	var sizes = [16, 32, 48];
	for (var i = 0; i < sizes.length; i++) {
		if (!CreateStripe(sizes[i])) {
			break;
		}
	}
}
