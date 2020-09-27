;NSIS Modern User Interface version 1.67
;Welcome/Finish Page Example Script
SetCompressor /solid lzma
Unicode true
;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
;Configuration

  ;General
  Name "Radiodelay"
  OutFile "Radiodelay_Setup.exe"

  ;Folder selection page
  InstallDir "$PROGRAMFILES64\Radiodelay"
  
  ;Get install folder from registry if available
;  InstallDirRegKey HKCU "Software\Radiodelay" ""

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "COPYING"
 ; !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.md"
  !define MUI_FINISHPAGE_RUN "$INSTDIR\Radiodelay.exe"
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Function .onInit
  FindWindow $0 "" "Radiodelay"
  StrCmp $0 0 continueinstall
  SendMessage $0 16 0 0
continueinstall:
FunctionEnd

Section "Radiodelay.exe" SecRadiodelay
  SetOutPath "$INSTDIR"
  
  ;ADD YOUR OWN STUFF HERE!
 
  
 ; add files / whatever that need to be installed here.
  File "Radiodelay.exe"
  File "README.md"

  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\DaanSystems\Radiodelay" "" "$INSTDIR"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Radiodelay" "DisplayName" "Radiodelay (remove only)"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Radiodelay" "UninstallString" '"$INSTDIR\uninst.exe"'
  ; write out uninstaller
  WriteUninstaller "$INSTDIR\uninst.exe"

SectionEnd

;--------------------------------
;Descriptions

 
;--------------------------------
;Uninstaller Section


; optional section
Section "Start Menu Shortcuts"
  CreateDirectory "$SMPROGRAMS\Radiodelay"
  CreateShortCut "$SMPROGRAMS\Radiodelay\Radiodelay.lnk" "$INSTDIR\Radiodelay.exe" "" "$INSTDIR\Radiodelay.exe" 0
  CreateShortCut "$SMPROGRAMS\Radiodelay\README.md.lnk" "$INSTDIR\README.md" "" "$INSTDIR\README.md" 0
  CreateShortCut "$SMPROGRAMS\Radiodelay\Uninstall.lnk" "$INSTDIR\uninst.exe" "" "$INSTDIR\uninst.exe" 0
  CreateShortCut "$DESKTOP\Radiodelay.lnk" "$INSTDIR\Radiodelay.exe" "" "$INSTDIR\Radiodelay.exe"
SectionEnd

Section "Uninstall"

  ;ADD YOUR OWN STUFF HERE!


; add delete commands to delete whatever files/registry keys/etc you installed here.
Delete "$INSTDIR\uninst.exe"
Delete "$INSTDIR\Radiodelay.exe"
Delete "$INSTDIR\README.md"

DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\DaanSystems\Radiodelay"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Radiodelay"
; remove shortcuts, if any.
Delete "$DESKTOP\Radiodelay.lnk"
Delete "$SMPROGRAMS\Radiodelay\*.*"
; remove directories used.
RMDir "$SMPROGRAMS\Radiodelay"
RMDir "$INSTDIR"

SectionEnd  