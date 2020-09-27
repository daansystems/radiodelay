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

release: icon build strip
