MACHINE=$(shell $(CC) -dumpmachine)

FLTKFLAGS=
EXT=
SETUP=

ifneq (, $(findstring mingw, $(MACHINE)))
	FLTKFLAGS += -D"FOO -static -static-libgcc -static-libstdc++ radiodelay.res.o"
	EXT += .exe
	SETUP = "/c/Program Files (x86)/NSIS/makensis.exe" radiodelay.nsi
else ifneq (, $(findstring linux, $(MACHINE)))
	FLTKFLAGS += -D"FOO -static-libgcc -static-libstdc++"
else
	FLTKFLAGS += -D"FOO"
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

setup:
	$(SETUP)

release: icon build strip zip setup
