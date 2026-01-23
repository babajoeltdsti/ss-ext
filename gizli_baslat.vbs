' SS-EXT V2.1 - Arka planda calistir (pencere acilmaz)
' Yapimci: OMERBABACO
Set WshShell = CreateObject("WScript.Shell")
WshShell.CurrentDirectory = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
WshShell.Run "venv\Scripts\pythonw.exe main.py", 0, False
