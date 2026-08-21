!macro NSIS_HOOK_POSTINSTALL
  Delete "$SMPROGRAMS\ClipBridge.lnk"
  CreateShortCut "$SMPROGRAMS\ClipBridge.lnk" "$INSTDIR\ClipBridge.exe" "" "$INSTDIR\ClipBridge.exe" 0

  Delete "$DESKTOP\ClipBridge.lnk"
  CreateShortCut "$DESKTOP\ClipBridge.lnk" "$INSTDIR\ClipBridge.exe" "" "$INSTDIR\ClipBridge.exe" 0

  System::Call 'shell32.dll::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
!macroend

!macro NSIS_HOOK_PREUNINSTALL
  Delete "$SMPROGRAMS\ClipBridge.lnk"
  Delete "$DESKTOP\ClipBridge.lnk"
!macroend
