// =======================================================
//
// Count Lines of Text Files 
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
// 02/29/2010 v0.1  Initial version
//
// =======================================================
var scriptVersion = "v0.1"; // version of this script

// =======================================================
// Initialize & Configure Global Variables
// =======================================================
var scriptName = "Count Lines " + scriptVersion;               // script name
var filesMask = "*.txt";                                       // only files with name matching this mask will be counted
var countWords = false;
// =======================================================
// =======================================================

//
// Script description:
//   Counts lines of selected files (and files in selected directories).
//   Features: recursive directory walking, file masks matching, progress dialog box, 
//   error states handling with Retry, Skip, Skip All, and Cancel error dialog box,
//   regular expressions, output log with opearation summary.
//
// Installation and Requirements:
//   Install Open Salamander: https://www.altap.cz/
//
// Usage:
//   In Open Salamander panel select files (or directories) you want to count
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

// =============
// Get file size
// =============

function GetFileSize(file)
{
  var f = objFSO.GetFile(file);
  return f.size;
}

// ===========
// Make plural
// ===========

function MyNumber(number, text)
{
  return number + " " + text + (number == 1 ? "" : "s");
}

// ====================
// Get file lines count
// ====================

function GetFileLinesCount(fileName)
{
  while (true)
  {
    try
    {
      var file = objFSO.GetFile(fileName); 
      var textStream = file.OpenAsTextStream(1, 0);
      var lines = 0;
      var words = 0;
      while (!textStream.AtEndOfStream)
      {
        var textLine = textStream.ReadLine();
        if (countWords)
        {
          var whiteSpace = /[\s]+/;
          var splitLine = textLine.split(whiteSpace);
          words += splitLine.length;
        }
        lines++;
      }
      textStream.Close();
      LogWrite(fileName + " - " + MyNumber(lines, "line"));
      totalWordsCount += words;
      totalLinesCount += lines;
      return true;
    }
    catch (err)
    {
      if (!clickedSkipAll)
      {
        var ret = Salamander.ErrorDialog(fileName, err.description, 3, "Error reading file");
        switch (ret)
        {
          case 2: operationCancelled = true; break;
          case 4: continue;
          case 16: break;
          case 17: clickedSkipAll = true; break;
        }
      }
      LogWrite(fileName + " - " + "Error reading file: " + err.description);
      return false;
    }
  }
}

// =======
// Do file
// =======

function DoFile(calcOnly, subPath, itemName)
{
  if (Salamander.MatchesMask(itemName, filesMask))
  {
    var sourceFile = Salamander.SourcePanel.Path.AppendPath(subPath).AppendPath(itemName);
    totalFilesSize += GetFileSize(sourceFile);
    totalFilesCount++;
    if (!calcOnly)
    {
      Salamander.ProgressDialog.AddText("Counting file " + subPath.AppendPath(itemName));
      var linesCounted = GetFileLinesCount(sourceFile);
      if (linesCounted)
      {
        Salamander.ProgressDialog.Position = totalFilesSize;
        if (Salamander.ProgressDialog.IsCancelled)
          operationCancelled = true;
      }
      else
      {
        totalErrors++;
      }
      if (operationCancelled)
          LogWrite("Aborting operation...");
    }
  }
  return true;
}

// ==============
// Enum directory
// ==============

function EnumDirectory(calcOnly, subPath)
{
  var fullPath = Salamander.SourcePanel.Path + "\\" + subPath;
  var folder = objFSO.GetFolder(fullPath);

  // enum all subdirectories
  for (var dirsEnum = new Enumerator(folder.SubFolders); !operationCancelled && !dirsEnum.atEnd(); dirsEnum.moveNext()) 
  {
    var strDirName = dirsEnum.item();
    EnumDirectory(calcOnly, subPath.AppendPath(strDirName.Name));
  }

  // extract all files
  for (var filesEnum = new Enumerator(folder.Files); !operationCancelled && !filesEnum.atEnd(); filesEnum.moveNext()) 
  {
    var strFileName = filesEnum.item();
    DoFile(calcOnly, subPath, strFileName.Name);
  }
}

// ===================
// Enum selected items
// ===================

function EnumSelectedItems(items)
{
  // Iterate through the selected items in two passes (0: calculate total size for progress dialog box; 1: do operation)
  for (var pass = 0; !operationCancelled && pass < 2; pass++)
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
      Salamander.ProgressDialog.Maximum = totalFilesSize;
      Salamander.ProgressDialog.Show();
    }
    
    totalFilesSize = 0;
    totalFilesCount = 0;
    
    for (var itemsEnum = new Enumerator(items); !operationCancelled && !itemsEnum.atEnd(); itemsEnum.moveNext())
    {
      var item = itemsEnum.item();
      if (item.Attributes & 16) 
        EnumDirectory(calcOnly, item.Name) // name is directory
      else
        DoFile(calcOnly, "", item.Name); // name is file
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

function ValidateFileMask(mask)
{
  try
  {
    Salamander.MatchesMask("dummy.txt", mask); // invalid mask would trhow exception
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
  fm.maskLbl = Salamander.Forms.Label("Count files &named:");
  fm.mask = Salamander.Forms.TextBox(filesMask);
  fm.words = Salamander.Forms.CheckBox("&Count words (slower)");
  fm.words.Checked = countWords;
  fm.okbtn = Salamander.Forms.Button("Ok", 1);
  fm.cancelbtn = Salamander.Forms.Button("Cancel", 2);
  
  while (true)
  {
    if (fm.Execute() == 1)
    {
      if (!ValidateFileMask(fm.mask.Text))
        continue;
      filesMask = fm.mask.Text;
      countWords = fm.words.Checked;
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
      LogWrite("Counting " + "(" + filesMask + ")" + ":\n"); 
      EnumSelectedItems(items);
      var summary = "\nOperation summary: " + MyNumber(totalErrors, "error");
      if (operationCancelled > 0)
      {
        summary += ", operation aborted!";
      }
      else
      {
        summary += "\nTotal files count: " + MyNumber(totalFilesCount, "file");
        summary += "\nTotal files size: " + MyNumber(totalFilesSize, "byte");
        summary += "\nTotal lines count: " + MyNumber(totalLinesCount, "line");
        if (countWords)
          summary += "\nTotal words count: " + MyNumber(totalWordsCount, "word");
      }
      LogWrite(summary); 
      LogClose(); 
      Salamander.SourcePanel.StoreSelection();
      Salamander.SourcePanel.DeselectAll(true);
      SaveOptions();
    }
  }
  else
    Salamander.MsgBox("Please select files or directories with files to count.", 64, scriptName);
}

// =====================================
// Logging to text file and trace server
// =====================================

function LogOpen()
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

function LogWrite(text)
{
  if (typeof(logFile) != "undefined")
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

// ============
// Save Options
// ============

function SaveOptions()
{
  Salamander.SetPersistentVal("CountLines.Mask", filesMask);   // our "private" namespace
  Salamander.SetPersistentVal("CountLines.Words", countWords); // our "private" namespace
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
  filesMask = MyGetPersistentVal("CountLines.Mask", filesMask);
  countWords = MyGetPersistentVal("CountLines.Words", countWords);
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

var totalErrors = 0;
var totalLinesCount = 0;
var totalWordsCount = 0;

var totalFilesSize;
var totalFilesCount;

var clickedSkipAll = false;
var operationCancelled = false;

if (CheckVersions())
{
  objWSH = new ActiveXObject("WScript.Shell");
  objFSO = new ActiveXObject("Scripting.FileSystemObject");

  LoadOptions();
  Main();
}

