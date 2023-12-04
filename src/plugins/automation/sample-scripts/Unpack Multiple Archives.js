// =======================================================
//
// Unpack Multiple Archives using 7-Zip archiver
//
// Microsoft JScript for Open Salamander Automation plugin
// Contact us on forum.altap.cz
//
// Copyright (c) 2010-2023 Open Salamander Authors
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// =======================================================
//
// Change History
// 01/19/2010 v0.1  Initial version
// 01/20/2010 v0.2  7-Zip archiver directory detection
//                  Archives with space in filename are handled properly
//                  String.AppendPath method for proper last backslash handling
//                  Fixed test for DIRECTORY attribute
//                  Archives with nested directories are supported
//                  Included note about 7-Zip wildcard usage (use * for all name instead of *.*)
//                  It is possible to unpack to source or target panel, see 'unpackToSource'
//                  Windows 2000 compatible, removed dependency on WScript.Shell.CurrentDirectory; 
//                  Works on UNC paths
//                  Safe defaults (do not overwrite without asking, unpack to source panel)
//                  7-Zip return code handling
//                  Creating log in TEMP dir, should be displayed when operation is finished
//                  Global WScript.Shell and Scripting.FileSystemObject object: objWSH and objFSO
//                  Switched from own implementation StripFileExt() -> objFSO.GetBaseName()
//                  Switched from own implementation GetFullTempFileName() -> objFSO.GetTempName()
// 01/21/2010 v0.3  Confirmation before extracting is started with options summary
//                  Show log file when finished
// 01/26/2010 v0.4  To specify unpacked archives mask set 'archivesMask' variable
// 01/31/2010 v0.5  Important global variables moved at the beginning of script.
//
// =======================================================
var scriptVersion = "v0.5"; // version of this script

// =======================================================
// Initialize & Configure Global Variables
// =======================================================
var scriptName = "Unpack Multiple Archives " + scriptVersion; // script name
//var pathTo7Zip = "C:\\Program Files (x86)\\7-Zip\\7z.exe";  // uncomment and specify this line if 7-Zip is not installed and cannot be located using Registry key HKCU\Software\7-Zip\Path
var overwriteFiles = false;                    // set 'true' to overwrite existing files without asking
var unpackToSubdir = true;                     // set 'true' to create subdirectories named by archive name; set 'false' to unpack to the same directory
var unpackToSource = true;                     // set 'true' to unpack to source panel path; set 'false' to unpack to the target panel path
var unpackFiles = "*";                         // specify mask for unpacked files; NOTE from 7-Zip documentation: 7-Zip doesn't uses the system wildcard parser. 7-Zip doesn't follow the 
                                               // archaic rule by which *.* means any file. 7-Zip treats *.* as matching the name of any file that has an extension. To process all files,
                                               // you must use a * wildcard.
                                               
var archivesMask = "*";                        // only archives with name matching this mask will be unpacked; uses Salamander file mask syntax: "*.txt;*.htm" or "|*.old"
// =======================================================
// =======================================================

//
// Script description:
//   Unpack selected archives (and archives in selected directories) using open source 7-Zip archiver. 
//   This script can be easily adapted for other unpackers. Features: recursive directory walking, file 
//   masks matching, calling external application (7-Zip archiver), error states handling, output log 
//   with opearation summary.
//
// Installation and Requirements:
//   Open Salamander: https://www.altap.cz/
//   7-Zip: http://www.7-zip.org/ (download any Windows x86 or x64 version)
//   Install both applications
//   OPTIONAL: Set 'pathTo7Zip' variable to the 7z.exe.
//
// Usage:
//   Focus archive or directory with archives you want to extract.
//   Start this script from Plugins > Automation menu.
//


// ===========
// Append path
// ===========

String.prototype.AppendPath = function(newPath)
{
  // Use a regular expression to replace leading and trailing 
  // spaces with the empty string
  if (this.length == 0 || this.length > 0 && this.charAt(this.length - 1) == "\\")
    return this + newPath;
  else  
    return this + "\\" + newPath;
}

// =========================
// Get unique temp file name
// =========================

function GetFullTempFileName()
{
  var tmpDir = objFSO.GetSpecialFolder(2).Path;
  var tmpFile = objFSO.GetTempName();
  return tmpDir.AppendPath(tmpFile);
}

// ===========================
// Translate 7-Zip return code
// ===========================

function Translate7ZipReturnCode(errorCode)
{
  switch (errorCode)
  {
    case 0: return "OK"; 
    case 1: return "Warning (non fatal error)"; 
    case 2: return "Fatal error"; 
    case 7: return "Command line error"; 
    case 8: return "Not enough memory for operation"; 
    case 255: return "User stopped the process"; 
    case -1073741510: return "Command prompt aborted by user";
    default: return "Unknown error " + errorCode;
  }
}

// ===================
// Extract one archive
// ===================

function ExtractArchive(subPath, itemName)
{
  if (Salamander.MatchesMask(itemName, archivesMask))
  {
    var archivePath = Salamander.SourcePanel.Path.AppendPath(subPath).AppendPath(itemName);
    var targetPath = (unpackToSource ? Salamander.SourcePanel.Path : Salamander.TargetPanel.Path);
    targetPath = targetPath.AppendPath(subPath);
    if (unpackToSubdir)
      targetPath = targetPath.AppendPath(objFSO.GetBaseName(itemName));
    
    var cmdLine = "\"" + unpackerPath + "\"" + " x -r " + "\"" + archivePath + "\"" + " " + "\"" + unpackFiles + "\"" + " -o" + "\"" + targetPath + "\"";
    if (overwriteFiles)
      cmdLine += " -y";
    
    try
    {
      var returnCode = objWSH.Run(cmdLine, 1, true); 
      LogWrite(archivePath + " - " + Translate7ZipReturnCode(returnCode));
      if (returnCode != 0) 
        unpackingErrors++;
    }
    catch(err)
    {
      LogWrite(archivePath + " - " + "Error executing 7-Zip! Terminating...");
      unpackingErrors++;
      Salamander.MsgBox("Cannot execute 7-Zip archiver.", 16, scriptName);
      return false; 
    } 
  }
  else
  {
    LogWrite(archivePath + " - " + "Skipped");
  }
  return true;
}

// ================
// Unpack directory
// ================

function UnpackDirectory(subPath)
{
  var fullPath = Salamander.SourcePanel.Path.AppendPath(subPath);
  var folder = objFSO.GetFolder(fullPath);

  // enum all subdirectories
  for (var dirsEnum = new Enumerator(folder.SubFolders); !dirsEnum.atEnd(); dirsEnum.moveNext()) 
  {
    var strDirName = dirsEnum.item();
    if (!UnpackDirectory(subPath.AppendPath(strDirName.Name)))
      return false;
  }

  // extract all files
  for (var filesEnum = new Enumerator(folder.Files); !filesEnum.atEnd(); filesEnum.moveNext()) 
  {
    var strFileName = filesEnum.item();
    if (!ExtractArchive(subPath, strFileName.Name))
      return false;
  }
  return true;
}

// =====================
// Unpack selected items
// =====================

function UnpackSelectedItems(items)
{
  // Iterate through the selected items
  for (var itemsEnum = new Enumerator(items); !itemsEnum.atEnd(); itemsEnum.moveNext())
  {
    var item = itemsEnum.item();
    if (item.Attributes & 16) 
    {
      if (!UnpackDirectory(item.Name)) // name is directory
        return false;
    }
    else
    {
      if (!ExtractArchive("", item.Name)) // name is file
        return false;
    }
  }
  return true;
}

// =====================
// Get items description
// =====================

function GetItemsDescription(items)
{
  var filesCount = 0;
  var dirsCount = 0;
  var item;
  for (var itemsEnum = new Enumerator(items); !itemsEnum.atEnd(); itemsEnum.moveNext())
  {
    item = itemsEnum.item();
    if (item.Attributes & 16) 
      dirsCount++;
    else
      filesCount++;
  }
  var description = "";
  if (filesCount + dirsCount == 1)
  {
    description += (filesCount == 1 ? "file" : "directory");
    description += " \"" + item.Name + "\"";
  }
  else
  {
    if (filesCount > 0)
      description += filesCount + (filesCount == 1 ? " file" : " files");
    if (dirsCount > 0)
    {
      if (filesCount > 0)
        description += " and ";
      description += dirsCount + (dirsCount == 1 ? " directory" : " directories");
    }
  }
  return description;
}

// =======================
// Get options description
// =======================

function GetOptionsDescription()
{
  var description = "-------- Options Overview --------\n";
  description += "Unpack to: " + (unpackToSource ? "SOURCE" : "TARGET") + " directory\n";
  description += "Overwrite files without asking: " + (overwriteFiles ? "YES" : "NO") + "\n";
  description += "Unpack to subdirectory by archive name: " + (unpackToSubdir ? "YES" : "NO") + "\n";
  description += "Unpack following files from archives: " + unpackFiles + "\n";
  description += "Show output log when finished: " + (logToFile ? "YES" : "NO");
  return description;
}

// =========
// Main code
// =========

function Main()
{
  var items = null;
  // disk paths only (no archive or plugin file system paths)
  if (Salamander.SourcePanel.PathType == 1) 
  {
    if (Salamander.SourcePanel.SelectedItems.Count > 0)
    {
      items = Salamander.SourcePanel.SelectedItems;
    }
    else
    {
      if (Salamander.SourcePanel.FocusedItem && Salamander.SourcePanel.FocusedItem.Name != "..")
      {
        items = new Array();
        items[0] = Salamander.SourcePanel.FocusedItem;
      }
    }
  }
  if (items != null)
  {
    var msg = "Are you sure you want to unpack " + GetItemsDescription(items) + "?";
    msg += "\n\n" + GetOptionsDescription();
    if (Salamander.MsgBox(msg, 33, scriptName) == 1)
    {
      LogOpen();
      LogWrite("Unpacking log:\n"); 
      UnpackSelectedItems(items);
      LogWrite("\nUnpacking summary: " + unpackingErrors + " error(s)"); 
      LogClose(); 
      Salamander.SourcePanel.StoreSelection();
      Salamander.SourcePanel.DeselectAll(true);
    }
  }
  else
    Salamander.MsgBox("Please select archives or directories with archives to unpack.", 64, scriptName);
}

// =====================================
// Logging to text file and trace server
// =====================================

function LogOpen()
{
  if (logToFile)
  {
    try
    {
      logFilePath = GetFullTempFileName();
      logFile = objFSO.CreateTextFile(logFilePath, true); // true = overwrite
    }
    catch(err)
    {
      Salamander.MsgBox("Unable to create a log file " + logFilePath + "\nError: " + err.description, 64, scriptName);
    }    
  }
}

function LogWrite(text)
{
  if (logToFile && typeof(logFile) != "undefined")
  {
    logFile.WriteLine(text);
  }
}

function LogClose()
{
  if (typeof(logFile) != "undefined")
  {
    logFile.Close();
    Salamander.ViewFile(logFilePath, 1);
    objFSO.DeleteFile(logFilePath); 
  }
}

// =================
// Get unpacker path
// =================

function GetUnpackerPath()
{
  var fullPath = null;
  if (typeof(pathTo7Zip) != "undefined")
    fullPath = pathTo7Zip;
  else
  {
    try
    {
      fullPath = objWSH.RegRead("HKCU\\Software\\7-Zip\\Path");
      fullPath = fullPath.AppendPath("7z.exe");
    }
    catch (err)
    {
      Salamander.MsgBox("This script requires 7-Zip archiver. Please download it from http://www.7-zip.org/ or uncomment and specify the \'pathTo7Zip\' variable.", 16, scriptName);
      return null;
    }
  }
  if (fullPath != null && !objFSO.FileExists(fullPath))
    Salamander.MsgBox("Unable to find 7-Zip archiver.\nPath: " + unpackerPath, 16, scriptName);
  return fullPath;
}

// =========================================
// Check version of OS and Automation plugin
// =========================================

function CheckVersions()
{
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
  if (Salamander.AutomationVersion < 10)
  {
    Salamander.MsgBox("Automation plugin version 1.0 or later is required by this script.", 16, scriptName);
    return false;
  }
  if (Salamander.WindowsVersion < 51)
  {
    Salamander.MsgBox("Windows XP or later is required by this script.", 16, scriptName);
    return false;
  }
  return true;
}

// ======================
// ======================
// JavaScript Entry Point
// ======================
// ======================

// Initialize Global Variables
var unpackerPath;

var objWSH; // WScript.Shell
var objFSO; // Scripting.FileSystemObject

var logToFile = true;                          // set 'true' to create and show log messages
var logFilePath;
var logFile;

var unpackingErrors = 0;

// debugger; // break in debugger

if (CheckVersions())
{
  objWSH = new ActiveXObject("WScript.Shell");
  objFSO = new ActiveXObject("Scripting.FileSystemObject");

  unpackerPath = GetUnpackerPath();                // find 7-Zip archiver
  if (unpackerPath != null)
    Main();
}
