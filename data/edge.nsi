; EDGE NSIS Installer Script

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
;Version Information

  !define VERSION_SUFFIX "RC4"
  !define VERSION "1.29"
  !define FULLNAME "EDGE ${VERSION} Release Candidate #4"

  !define INSTALLDIR_KEYNAME "InstallDirectory"
  
;--------------------------------
;General

  ;Name and file
  Name "${FULLNAME}"
  OutFile "edge-${VERSION}${VERSION_SUFFIX}-win32.exe"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\${FULLNAME}"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\${FULLNAME}" "${INSTALLDIR_KEYNAME}"

;--------------------------------
;Variables

  Var MUI_TEMP
  Var STARTMENU_FOLDER

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  ; Show the start page
  !insertmacro MUI_PAGE_WELCOME

  ; Show the license
  ; FIXME: Insert GPL
  ;!insertmacro MUI_PAGE_LICENSE "${NSISDIR}\Docs\Modern UI\License.txt"
  
  ; Select the components 
  !insertmacro MUI_PAGE_COMPONENTS
  
  ; Select the install directory
  !insertmacro MUI_PAGE_DIRECTORY
  
  ; Select the start menu folder
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\${FULLNAME}" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "StartMenuFolder"
  
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER
    
  ; Do the install
  !insertmacro MUI_PAGE_INSTFILES
  
  ; Show the finish page
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

; Engine (Required)
Section "Engine" SecEngine
  ; This is a required section
  SectionIn RO
  
  ;Set output directory to install directory
  SetOutPath "$INSTDIR"
  
  ;Files
  File "gledge32.exe"
  File "ogg.dll"
  File "vorbis.dll"
  File "vorbisfile.dll"
  File "changelog.txt"
  File "readme.txt"
  
  ;Store installation folder
  WriteRegStr HKCU "Software\${FULLNAME}" "${INSTALLDIR_KEYNAME}" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  ;Create shortcuts
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\${FULLNAME}.lnk" "$INSTDIR\gledge32.exe"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_END  
  
SectionEnd

; Game Development Tools.
Section "Game Development Tools" SecGameDevTools
  SetOutPath "$INSTDIR"
  
  ;Files
  File "edge-ddf-4.6.zip"
SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_SecEngine ${LANG_ENGLISH} "The engine executable."
  LangString DESC_SecGameDevTools ${LANG_ENGLISH} "Tools used for developed games with EDGE."

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecEngine} $(DESC_SecEngine)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecGameDevTools} $(DESC_SecGameDevTools)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ;  Remove the shortcuts
  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP
    
  Delete "$SMPROGRAMS\$MUI_TEMP\Uninstall.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\${FULLNAME}.lnk"
 
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"
  startMenuDeleteLoop:
	ClearErrors
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."
    
    IfErrors startMenuDeleteLoopDone
  
    StrCmp $MUI_TEMP $SMPROGRAMS startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:
  
  ;  Remove the engine files
  Delete "$INSTDIR\gledge32.exe"
  Delete "$INSTDIR\ogg.dll"
  Delete "$INSTDIR\vorbis.dll"
  Delete "$INSTDIR\vorbisfile.dll"
  Delete "$INSTDIR\changelog.txt"
  Delete "$INSTDIR\readme.txt"

  ; Remove any game development files
  IfFileExists "$INSTDIR\edge-ddf-4.6.zip" 0 +2
	Delete "$INSTDIR\edge-ddf-4.6.zip"
  
  ;  Remove the uninstaller itself
  Delete "$INSTDIR\Uninstall.exe"

  ;  Remove the install directory
  RMDir "$INSTDIR"

  ; Delete any registration keys
  DeleteRegKey /ifempty HKCU "Software\${FULLNAME}"

SectionEnd