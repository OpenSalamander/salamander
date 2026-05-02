On Error Resume Next
Err.Clear
rem FullPath = Salamander.GetFullPath("relative_path")       ' relative path, no panel is specified
FullPath = Salamander.GetFullPath("c:\relative_path")    ' absolute path, no panel is specified
FullPath = Salamander.GetFullPath("relative_path", Salamander.SourcePanel) ' relative path for source/active panel
FullPath = Salamander.GetFullPath("relative_path", Salamander.TargetPanel) ' relative path for target/inactive panel
if  Err.Number <> 0  then   
Salamander.MsgBox "Error description: " + Err.Description, 0, "Error"
Else
Salamander.MsgBox "Full path: " + FullPath, 0, "Info"
End If
