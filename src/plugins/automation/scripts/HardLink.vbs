' Hardlinks.vbs
' Demonstrates hard links on NTFS volumes 
' --------------------------------------------------------

Option Explicit

' Some constants
Const L_NoHardLinkCreated = "Unable to create hard link"
Const L_EnterTarget = "Enter the file name to hard-link to"
Const L_HardLinks = "Creating hard link"
Const L_EnterHardLink = "Name of the hard link you want to create"
Const L_CannotCreate = "Make sure that both files are on the same volume and the volume is NTFS"
Const L_NotExist = "Sorry, the file doesn't exist"
Const L_SameName = "Target file and hard link cannot have the same name"

' Determine the existing file to (hard) link to
dim sTargetFile 
if WScript.Arguments.Count >0 then
   sTargetFile = WScript.Arguments(0)
else
   sTargetFile = InputBox(L_EnterTarget, L_HardLinks, "")
   if sTargetFile = "" then WScript.Quit
end if

' Does the file exist?
dim fso
set fso = CreateObject("Scripting.FileSystemObject")   
if Not fso.FileExists(sTargetFile) then
   MsgBox L_NotExist
   WScript.Quit
end if

' Main loop
while true
   QueryForHardLink sTargetFile
wend


' Close up
WScript.Quit






' /////////////////////////////////////////////////////////////
' // Helper Functions



' Create the hard link
'------------------------------------------------------------
function QueryForHardLink(sTargetFile)
   ' Extract the hard link name if specified on the command line
   dim sHardLinkName
   if WScript.Arguments.Count >1 then
      sHardLinkName = WScript.Arguments(1)
   else
      dim buf
      buf = L_EnterHardLink & " for" & vbCrLf & sTargetFile
      sHardLinkName = InputBox(buf, L_HardLinks, sTargetFile)
      if sHardLinkName = "" then WScript.Quit   
      if sHardLinkName = sTargetFile then 
         MsgBox L_SameName
         exit function
      end if
   end if 

   ' Verify that both files are on the same volume and the 
   ' volume is NTFS
   if Not CanCreateHardLinks(sTargetFile, sHardLinkName) then 
      MsgBox L_CannotCreate
      exit function
   end if
   
   ' Creates the hard link
   dim oHL
   set oHL = CreateObject("HardLink.Object.1")
   oHL.CreateNewHardLink sHardLinkName, sTargetFile
end function


' Verify that both files are on the same NTFS disk
'------------------------------------------------------------
function CanCreateHardLinks(sTargetFile, sHardLinkName)
   CanCreateHardLinks = false
   
   dim fso
   set fso = CreateObject("Scripting.FileSystemObject")
   
   ' same drive?
   dim d1, d2
   d1 = fso.GetDriveName(sTargetFile)
   d2 = fso.GetDriveName(sHardLinkName)
   if d1 <> d2 then exit function

   ' NTFS drive?
   CanCreateHardLinks = IsNTFS(sTargetFile)
end function


' IsNTFS() - Verifies whether the file's volume is NTFS
' --------------------------------------------------------
function IsNTFS(sFileName)
   dim fso, drv
   
   IsNTFS = False
   set fso = CreateObject("Scripting.FileSystemObject")   
   set drv = fso.GetDrive(fso.GetDriveName(sFileName)) 
   set fso = Nothing
   
   if drv.FileSystem = "NTFS" then IsNTFS = True
end function