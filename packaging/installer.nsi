; Per-user installer for PoetryDesktop.
; Built by the CMake `installer` target, which stages a windeployqt tree and
; passes STAGE_DIR, OUT_FILE, APP_VERSION, APP_VERSION_QUAD and ICON_FILE.
; It installs under %LOCALAPPDATA%\Programs so no elevation is needed, matching
; the HKCU Run entry the app itself writes.

Unicode true

!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef APP_VERSION
  !define APP_VERSION "0.0.0"
!endif
!ifndef APP_VERSION_QUAD
  !define APP_VERSION_QUAD "0.0.0.0"
!endif
!ifndef STAGE_DIR
  !error "STAGE_DIR is required"
!endif
!ifndef OUT_FILE
  !error "OUT_FILE is required"
!endif
!ifndef ICON_FILE
  !define ICON_FILE "app_icon.ico"
!endif

!define APP_NAME "诗词壁纸"
!define APP_EXE "poetry_desktop.exe"
!define APP_PUBLISHER "PoetryDesktop"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\PoetryDesktop"
!define INSTALL_KEY "Software\PoetryDesktop\PoetryDesktop"

Name "${APP_NAME}"
OutFile "${OUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\PoetryDesktop"
InstallDirRegKey HKCU "${INSTALL_KEY}" ""
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "${APP_NAME} ${APP_VERSION}"

VIProductVersion "${APP_VERSION_QUAD}"
VIAddVersionKey /LANG=2052 "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=2052 "FileDescription" "${APP_NAME} 安装程序"
VIAddVersionKey /LANG=2052 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=2052 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=2052 "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey /LANG=2052 "LegalCopyright" "Copyright (C) 2026 ${APP_PUBLISHER}"

!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "运行诗词壁纸"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "安装" SecInstall
  SetOutPath "$INSTDIR"
  File /r "${STAGE_DIR}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}\使用说明.lnk" "$INSTDIR\README.txt"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}\卸载诗词壁纸.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  WriteRegStr HKCU "${INSTALL_KEY}" "" "$INSTDIR"
  WriteRegStr HKCU "${INSTALL_KEY}" "Start Menu Folder" "$SMPROGRAMS\${APP_NAME}"

  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" "$\"$INSTDIR\Uninstall.exe$\" /S"
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\使用说明.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\卸载诗词壁纸.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"

  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "${INSTALL_KEY}"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR"
SectionEnd
