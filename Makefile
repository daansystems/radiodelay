MACHINE=$(shell $(CC) -dumpmachine)

FLTKFLAGS=
EXT=

ifneq (, $(findstring mingw, $(MACHINE)))
	FLTKFLAGS += -D"FOO -O2 -static -static-libgcc -static-libstdc++ radiodelay.res.o"
	EXT += .exe
else ifneq (, $(findstring linux, $(MACHINE)))
	FLTKFLAGS += -D"FOO -O2 -static-libgcc -static-libstdc++"
else
	FLTKFLAGS += -D"FOO -O2"
endif


all: build
	./radiodelay$(EXT)

build:
	fltk-config -g --compile radiodelay.cpp $(FLTKFLAGS)

icon:
	windres radiodelay.rc radiodelay.res.o

strip-windows:
	strip radiodelay$(EXT)

strip-macos:
	strip radiodelay.app/Contents/Macos/radiodelay

zip:
	zip RadioDelay-Windows-x64.zip radiodelay.exe README.md COPYING

dmg:
	mkdir -p radiodelay.app/Contents/Resources
	cp radiodelay.icns radiodelay.app/Contents/Resources
	/usr/libexec/PlistBuddy -c 'Add :NSMicrophoneUsageDescription string This app requires microphone access' radiodelay.app/Contents/Info.plist
	/usr/libexec/PlistBuddy -c 'Add :CFBundleIconFile string radiodelay' radiodelay.app/Contents/Info.plist
	hdiutil create -fs HFS+ -srcfolder "radiodelay.app" -volname "radiodelay" "RadioDelay-MacOS.dmg"

setup:
	"/c/Program Files (x86)/NSIS/makensis.exe" radiodelay.nsi

sign-radiodelay-windows:
	"/c/Program Files (x86)/Windows Kits/10/App Certification Kit/signtool.exe" sign /f daansystems.pfx /p ${CODE_SIGN_CERTIFICATE_PASSWORD} radiodelay.exe

sign-setup-windows:
	"/c/Program Files (x86)/Windows Kits/10/App Certification Kit/signtool.exe" sign /f daansystems.pfx /p ${CODE_SIGN_CERTIFICATE_PASSWORD} RadioDelay-Windows-x64-Setup.exe

windows: icon build strip-windows zip setup

windows-signed: icon build strip-windows sign-radiodelay-windows zip setup sign-setup-windows

macos: build strip-macos dmg
