' SS-EXT V2.0 - Arka planda calistir (pencere acilmaz)
' Yapimci: OMERBABACO
Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
appDir = fso.GetParentFolderName(WScript.ScriptFullName)
WshShell.CurrentDirectory = appDir

If fso.FileExists(appDir & "\ss-ext.exe") Then
	WshShell.Run """" & appDir & "\ss-ext.exe"" --tray", 0, False
Else
	WshShell.Run "venv\Scripts\pythonw.exe main.py --tray", 0, False
End If
