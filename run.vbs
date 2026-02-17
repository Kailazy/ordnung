Set WshShell = CreateObject("WScript.Shell")
WshShell.CurrentDirectory = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
WshShell.Run "python app.py", 0, False
WScript.Sleep 1500
WshShell.Run "http://localhost:5000", 0, False
