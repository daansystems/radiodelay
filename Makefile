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

strip:
	strip radiodelay$(EXT)

zip:
	zip RadioDelay-Windows-x64.zip radiodelay.exe README.md COPYING

dmg:
	mkdir -p radiodelay.app/Contents/Resources
	cp radiodelay.icns radiodelay.app/Contents/Resources
	/usr/libexec/PlistBuddy -c 'Add :NSMicrophoneUsageDescription string This app requires microphone access' radiodelay.app/Contents/Info.plist
	/usr/libexec/PlistBuddy -c 'Add :CFBundleIconFile string radiodelay' radiodelay.app/Contents/Info.plist
	hdiutil create -fs HFS+ -srcfolder "radiodelay.app" -volname "radiodelay" "radiodelay.dmg"

setup:
	"/c/Program Files (x86)/NSIS/makensis.exe" radiodelay.nsi

windows: icon build strip zip setup

macos: build strip dmg


