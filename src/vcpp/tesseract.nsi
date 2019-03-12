Name "Tesseract"

OutFile "tesseract_20YY_MM_DD_foo_edition_windows.exe"

InstallDir $PROGRAMFILES\Tesseract

InstallDirRegKey HKLM "Software\Tesseract" "Install_Dir"

SetCompressor /SOLID lzma
XPStyle on

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Tesseract (required)"

  SectionIn RO
  
  SetOutPath $INSTDIR
  
  File /r "..\..\*.*"
  
  WriteRegStr HKLM SOFTWARE\Tesseract "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tesseract" "DisplayName" "Tesseract"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tesseract" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tesseract" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tesseract" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

  IfFileExists "$DOCUMENTS\My Games\Tesseract\config\saved.cfg" ConfigFound NoConfig  
  ConfigFound:
     Delete "$DOCUMENTS\My Games\Tesseract\config\old-saved.cfg"
     Rename "$DOCUMENTS\My Games\Tesseract\config\saved.cfg" "$DOCUMENTS\My Games\Tesseract\config\old-saved.cfg"
  NoConfig:

SectionEnd

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Tesseract"

  CreateDirectory "$DOCUMENTS\My Games\Tesseract"
 
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$INSTDIR\Tesseract.lnk"                "$INSTDIR\tesseract.bat" "" "$INSTDIR\bin\tesseract.exe" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\Tesseract\Tesseract.lnk"   "$INSTDIR\tesseract.bat" "" "$INSTDIR\bin\tesseract.exe" 0 SW_SHOWMINIMIZED
;  CreateShortCut "$SMPROGRAMS\Tesseract\README.lnk"      "$INSTDIR\README.html"   "" "$INSTDIR\README.html" 0

  CreateShortCut "$INSTDIR\User Data.lnk"                "$DOCUMENTS\My Games\Tesseract"
  CreateShortCut "$SMPROGRAMS\Tesseract\User Data.lnk"   "$DOCUMENTS\My Games\Tesseract"  

  CreateShortCut "$SMPROGRAMS\Tesseract\Uninstall.lnk"   "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tesseract"
  DeleteRegKey HKLM SOFTWARE\Tesseract

  RMDir /r "$SMPROGRAMS\Tesseract"
  RMDir /r "$INSTDIR"

SectionEnd
