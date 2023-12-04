// =======================================================
//
// Convert Images using ImageMagick convert
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
// 01/27/2010 v0.1  Initial version
// 01/31/2010 v0.2  Important global variables moved at the beginning of script.
//                  EnsureDirectoryExist() can recursively create whole path.
//                  Another example of 'imageMagickParams'
//                  File overwrite confirmation (can be suppressed with 'overwriteFilesWithoutAsking' variable)
//                  Target from target panel with optional 'targetPathOverride' variable
// 02/09/2010 v0.3  Using new Salamander.OverwriteDialog() with Yes, All, Skip, Skip All and Cancel buttons.
// 02/20/2010 v0.4  Initial confirmation is using new Form object.
//                  Initial support for relative destination paths (..\..\directory) and absolute paths without root (\directory)
//                  More robust AppendPath(): whitespace stripping; paths starting with '\\' are handled properly
// 02/23/2010 v0.5  Using SetPersistentVal() and GetPersistentVal() for configuration storage.
//                  Support for relative and absolute target paths using new Salamander.GetFullPath method. Supported paths: "..", "..\..\dst", "d:", "d:dst", etc.
//                  
// =======================================================
var scriptVersion = "v0.5"; // version of this script

// =======================================================
// Initialize & Configure Global Variables
// =======================================================
var scriptName = "Convert Images " + scriptVersion;               // script name
var imageMagickPath = "C:\\Program Files\\ImageMagick-6.5.9-Q16"; // path to the ImageMagick (x86 or x64 version) home directory 
var imageMagickParams = "-resample 100x100 -quality 80"; /*-trim -colors 128*/ // unify images resolution to 100DPI; output quality 80; optional: trim white space, reduce image to 128 colors
//var imageMagickParams = "-resize 900x900\> -quality 80";        // fit image (only shrink) to box 900x900 pixels; output quality 80
var imagesMask = "*.jpg;*.png";                                   // only images with name matching this mask will be converted; uses Salamander file mask syntax: "*.jpg;*.png" or "|*.txt"
var overwriteFilesWithoutAsking = false;                          // set 'true' to overwrite existing files without asking
var targetPathOverride; // = "\\\\computer\\c\\tmp\\";            // uncomment this line to specify target directory; directory from the target panel will be ignored
var logToFile = true;                                             // set 'true' to create and show log messages
// =======================================================
// =======================================================

//
// Script description:
//   Converts selected images (and images in selected directories) using open source ImageMagick tools.
//   This script can be easily adapted for other, typically conversion, operations with files. 
//   Features: recursive directory walking, file masks matching, calling external application 
//   ImageMagick convert command), progress dialog box, overwrite dialog box with Yes, All, Skip,
//   Skip All and Cancel buttons, error states handling, output log with opearation summary.
//
// Installation and Requirements:
//   Open Salamander: https://www.altap.cz/
//   ImageMagick: http://www.imagemagick.org/ (download any Windows precompiled x86 or x64 version)
//   Install both applications
//
// Usage:
//   In Open Salamander panel select files (or directories) you need to convert
//   Inactive panel will be used as target path
//   Start this script using Plugins > Automation menu
//

// ===========
// Append path
// ===========

String.prototype.AppendPath = function(path)
{
  // Use a regular expression to replace leading and trailing 
  // whitespaces with the empty string
  var stripped = (path.replace(/^\s+/,"")).replace(/\s+$/,"");
  // strip initial backslash
  stripped = stripped.replace(/^\\/,"");
  if (this.length == 0 || this.length > 0 && this.charAt(this.length - 1) == "\\")
    return this + stripped;
  else  
    return this + "\\" + stripped;
}

// =============
// Get Root Path 
// =============

function GetRootPath(path)
{
  return objFSO.GetDriveName(path);
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

// ===========
// Get IM path
// ===========

function GetIMPath(component)
{
  var path = "";
  if (typeof(imageMagickPath) != "undefined")
    path = imageMagickPath.AppendPath(component);
  else
    path = component;
  return path;
}

// =============
// Get file size
// =============

function GetFileSize(file)
{
  var f = objFSO.GetFile(file);
  return f.size;
}

// ===============
// Get target path
// ===============

function GetTargetPath()
{
  if (typeof(targetPathOverride) != "undefined")
    return targetPathOverride;
  else
    return Salamander.TargetPanel.Path;
}

// ======================
// Ensure directory exist
// ======================

function EnsureDirectoryExist(path)
{
  var parent = objFSO.GetParentFolderName(path);
  if (parent != "")
  {
    if (!objFSO.FolderExists(objFSO.GetParentFolderName(path)))
      EnsureDirectoryExist(objFSO.GetParentFolderName(path));
    try
    {
      if (!objFSO.FolderExists(path))
        objFSO.CreateFolder(path);
    }
    catch (err)
    {
      LogWrite("Cannot copy create directory " + path + ". Error: " + err.description);
    }
  }
}

// ====================
// Call convert command
// ====================

function CallConvertCommand(sourceFile, targetFile)
{
  // there is some problem with image magick handling of diacritics  
  // in names so we will use temporary file with ansi-safe names
  var sourceFileTmp = GetFullTempFileName();
  var targetFileTmp = GetFullTempFileName();
  try
  {
    objFSO.CopyFile(sourceFile, sourceFileTmp, true);
  }
  catch (err)
  {
    LogWrite(sourceFile + " - " + "Cannot copy file to TEMP directory. Error: " + err.description);
    convertingErrors++;
    return;
  }
  
  var converter = GetIMPath("convert.exe");
  var cmdLine = "\"" + converter + "\"" + " " + imageMagickParams + " " + "\"" + sourceFileTmp + "\"" + " " + "\"" + targetFileTmp + "\"";
  var returnCode = objWSH.Run(cmdLine, 0, true); 
  if (returnCode == 0)
  {
    try
    {
      objFSO.CopyFile(targetFileTmp, targetFile, true);
      LogWrite(sourceFile + " - " + (returnCode == 0 ? "OK" : "Error"));
    }
    catch (err)
    {
      LogWrite(sourceFile + " - " + "Cannot copy file from TEMP directory. Error: " + err.description);
      convertingErrors++;
    }
  }
  else
    convertingErrors++;
  
  // clenaup  
  objFSO.DeleteFile(sourceFileTmp);
  objFSO.DeleteFile(targetFileTmp);
}

// =============
// Convert image
// =============

function ConvertImage(calcOnly, subPath, itemName)
{
  if (Salamander.MatchesMask(itemName, imagesMask))
  {
    var sourceFile = Salamander.SourcePanel.Path.AppendPath(subPath).AppendPath(itemName);
    convertedFilesSize += GetFileSize(sourceFile);
    if (!calcOnly)
    {
      var targetFile = GetTargetPath().AppendPath(subPath).AppendPath(itemName);

      var copyFile = false; 
      var targetExists = objFSO.FileExists(targetFile);
      
      if (!overwriteFilesWithoutAsking && !clickedSkipAll && targetExists)
      {
        var overwriteRet = Salamander.OverwriteDialog(objFSO.GetFile(targetFile), objFSO.GetFile(sourceFile), 4);
        switch (overwriteRet)
        {
          case 18: overwriteFilesWithoutAsking = true;// All
          case 6: copyFile = true; break;             // Yes
          case 17: clickedSkipAll = true; break;      // Skip All
          case 2: conversionCancelled = true; break;  // Cancel
        }
      }
      if (!targetExists || overwriteFilesWithoutAsking)
        copyFile = true;

      if (copyFile)
      {
        Salamander.ProgressDialog.AddText("Converting image " + subPath.AppendPath(itemName));
        CallConvertCommand(sourceFile, targetFile);
        Salamander.ProgressDialog.Position = convertedFilesSize;
        if (Salamander.ProgressDialog.IsCancelled)
          conversionCancelled = true;
      }
      else
      {
        LogWrite(sourceFile + " - " + "Skipped");
        skippedFiles++;
      }
      
      
      if (conversionCancelled)
          LogWrite("Aborting conversion...");
    }
  }
  return true;
}


// =================
// Convert directory
// =================

function ConvertDirectory(calcOnly, subPath)
{
  var fullPath = Salamander.SourcePanel.Path + "\\" + subPath;
  var folder = objFSO.GetFolder(fullPath);

  if (!calcOnly)
    EnsureDirectoryExist(GetTargetPath().AppendPath(subPath));

  // enum all subdirectories
  for (var dirsEnum = new Enumerator(folder.SubFolders); !conversionCancelled && !dirsEnum.atEnd(); dirsEnum.moveNext()) 
  {
    var strDirName = dirsEnum.item();
    ConvertDirectory(calcOnly, subPath.AppendPath(strDirName.Name));
  }

  // extract all files
  for (var filesEnum = new Enumerator(folder.Files); !conversionCancelled && !filesEnum.atEnd(); filesEnum.moveNext()) 
  {
    var strFileName = filesEnum.item();
    ConvertImage(calcOnly, subPath, strFileName.Name);
  }
}

// ======================
// Convert selected items
// ======================

function ConvertSelectedItems(items)
{
  EnsureDirectoryExist(GetTargetPath());

  // Iterate through the selected items in two passes (0: calculate total size for progress dialog box; 1: do convert)
  for (var pass = 0; !conversionCancelled && pass < 2; pass++)
  {
    var calcOnly = (pass == 0); // only calculate files count
    
    if (pass == 0) 
    {
      // show wait window in first pass
      Salamander.WaitWindow.Title = scriptName;
      Salamander.WaitWindow.Text = "Please wait, script is reading directory tree...";
      Salamander.WaitWindow.Show();    
    }
    else
    {
      // show progress dialog box
      Salamander.ProgressDialog.Title = scriptName;
      Salamander.ProgressDialog.Style = 1;
      Salamander.ProgressDialog.Maximum = convertedFilesSize;
      Salamander.ProgressDialog.Show();
    }
    
    convertedFilesSize = 0;
    
    for (var itemsEnum = new Enumerator(items); !conversionCancelled && !itemsEnum.atEnd(); itemsEnum.moveNext())
    {
      var item = itemsEnum.item();
      if (item.Attributes & 16) 
        ConvertDirectory(calcOnly, item.Name) // name is directory
      else
        ConvertImage(calcOnly, "", item.Name); // name is file
    }
    
    if (pass == 0)
      Salamander.WaitWindow.Hide();
    else
      Salamander.ProgressDialog.Hide();
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

// ====================
// Validation functions
// ====================

function ValidateTargetPath(path)
{
  if (!objFSO.FolderExists(path))
  {
    var res = Salamander.MsgBox("Specified target directory \'" + path + "\' doesn't exist.\nDo you want to create it?", 32 + 1, scriptName);
    return res == 1;
  }
  return true;
}

function ValidateFileMask(mask)
{
  try
  {
    Salamander.MatchesMask("dummy.txt", mask); // invalid mask would throw exception
  }
  catch (err)
  {
    Salamander.MsgBox("Specifed file mask \'" + mask + "\' is invalid.\nError: " + err.description, 16, scriptName);
    return false;
  }
  return true;
}

// =====================
// Show Run Confirmation
// =====================

function ShowRunConfirmation(items)
{
  var fm = Salamander.Forms.Form(scriptName);
  fm.dstLbl = Salamander.Forms.Label("&Convert " + GetItemsDescription(items) + " to:");
  fm.dst = Salamander.Forms.TextBox(GetTargetPath());
  fm.maskLbl = Salamander.Forms.Label("Convert files &named:");
  fm.mask = Salamander.Forms.TextBox(imagesMask);
  fm.overwrite = Salamander.Forms.CheckBox("&Overwrite existing files without asking");
  fm.overwrite.Checked = overwriteFilesWithoutAsking;
  fm.showlog = Salamander.Forms.CheckBox("&Show output log when finished");
  fm.showlog.Checked = logToFile;
  fm.okbtn = Salamander.Forms.Button("Ok", 1);
  fm.cancelbtn = Salamander.Forms.Button("Cancel", 2);
  
  while (true)
  {
    if (fm.Execute() == 1)
    {
      try
      {
        var fullPath = Salamander.GetFullPath(fm.dst.Text, Salamander.SourcePanel);
      }
      catch (err)
      {
        Salamander.MsgBox("The specified name \'" + fm.dst.Text + "\' is invalid.\n\n" + err.description, 16, scriptName);
        continue;
      }
      if (!ValidateTargetPath(fullPath))
        continue;
      if (!ValidateFileMask(fm.mask.Text))
      {
        objWSH.SendKeys("{TAB}"); // Select invalid mask
        continue;
      }
      targetPathOverride = fullPath;
      imagesMask = fm.mask.Text;
      overwriteFilesWithoutAsking = fm.overwrite.Checked;
      logToFile = fm.showlog.Checked;
      return true;
    }
    return false;
  }
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
    if (ShowRunConfirmation(items))
    {
      LogOpen();
      LogWrite("Converting " + "(" + imagesMask + ")" + ":\n"); 
      ConvertSelectedItems(items);
      var summary = "\nConversion summary: " + convertingErrors + " error(s)";
      if (skippedFiles > 0)
        summary += ", " + skippedFiles + " skipped";
      if (conversionCancelled > 0)
        summary += ", aborted!";
      LogWrite(summary); 
      LogClose(); 
      Salamander.SourcePanel.StoreSelection();
      Salamander.SourcePanel.DeselectAll(true);
      SaveOptions();
    }
  }
  else
    Salamander.MsgBox("Please select images or directories with images to convert.", 64, scriptName);
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

// =======================
// Check Image Magick path
// =======================

function CheckImageMagickPath()
{
  var fullPath = imageMagickPath.AppendPath("convert.exe");
  Salamander.TraceI(fullPath);
  
  if (!objFSO.FileExists(fullPath))
  {
    var fm = Salamander.Forms.Form(scriptName);
    fm.infLbl = Salamander.Forms.Label("Script is unable to find your ImageMagick home directory.");
    fm.dstLbl = Salamander.Forms.Label("ImageMagick directory:");
    fm.dst = Salamander.Forms.TextBox(imageMagickPath);
    fm.okbtn = Salamander.Forms.Button("Ok", 1);
    fm.cancelbtn = Salamander.Forms.Button("Cancel", 2);
    
    while (true)
    {
      if (fm.Execute() == 1)
      {
        if (!objFSO.FileExists(fm.dst.Text.AppendPath("convert.exe")))
          continue;
        imageMagickPath = fm.dst.Text;
        return true;
      }
      return false;
    }
  }
  return true;
}

// ============
// Save Options
// ============

function SaveOptions()
{
  Salamander.SetPersistentVal("ImageMagickPath", imageMagickPath); // shared namespace, could be useful for other scripts
  Salamander.SetPersistentVal("ConvertImages.Mask", imagesMask);   // our "private" namespace
  Salamander.SetPersistentVal("ConvertImages.Overwrite", overwriteFilesWithoutAsking); // our "private" namespace
  Salamander.SetPersistentVal("ConvertImages.Log", logToFile);     // our "private" namespace
}

// ============
// Load Options
// ============

function MyGetPersistentVal(key, def)
{
  var tmp = Salamander.GetPersistentVal(key);
  return tmp == null ? def : tmp;
}

function LoadOptions()
{
  imageMagickPath = MyGetPersistentVal("ImageMagickPath", imageMagickPath);
  imagesMask = MyGetPersistentVal("ConvertImages.Mask", imagesMask);
  overwriteFilesWithoutAsking = MyGetPersistentVal("ConvertImages.Overwrite", overwriteFilesWithoutAsking);
  logToFile = MyGetPersistentVal("ConvertImages.Log", logToFile);
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
  if (Salamander.AutomationVersion < 11)
  {
    Salamander.MsgBox("Automation plugin version 1.1 or later is required by this script.", 16, scriptName);
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

var objWSH; // WScript.Shell
var objFSO; // Scripting.FileSystemObject

var logFilePath;
var logFile;

var convertingErrors = 0;
var skippedFiles = 0;

var convertedFilesSize;
var conversionCancelled = false;

var clickedSkipAll = false;

// debugger; // break in debugger; must be enabled in Automation plugin configuration

if (CheckVersions())
{
  objWSH = new ActiveXObject("WScript.Shell");
  objFSO = new ActiveXObject("Scripting.FileSystemObject");

  LoadOptions();
  if (CheckImageMagickPath())
  {
    Main();
  }
}

