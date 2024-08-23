# Radiodelay
## Audio input/output delay for Windows, MacOS, Linux

![Screenshot](radiodelay.gif)

#### (C)2006-2024 DaanSystems
#### License: GPLv3
#### Homepage: https://daansystems.com/radiodelay
#### Github: https://github.com/daansystems/radiodelay

# Introduction
Radiodelay is a tool to delay audio from an internal or external
input source. It is particulary useful for listening to sports commentary
on the radio when watching TV. Radio commentary is often a few
seconds earlier than TV.

# Commandline Options
-driver x : Set the input driver by driver index number<br/>
-delay x.x : Set the delay in x.x seconds.<br/>
-in x : Preselect the input device by device index number.<br/>
-out x : Preselect the output device by device index number.<br/>
-play : Start playing automatically.<br/>
-skipfile : Set skipfile.<br/>

## Example:
```
RadioDelay.exe -delay 5.6 -in 0 -out 1 -play
```

# Dependencies
Radiodelay is based in part on the work of the
FLTK project (http://www.fltk.org).

Radiodelay uses miniaudio for input/output (https://github.com/mackron/miniaudio).

# Compiling

A simple Makefile is available for Windows/Linux/MacOS. On windows you can use MinGW/MSYS to compile. Visual Studio should be easy too.
On linux/Mac use g++ or clang++.

##### Example Windows 64 bit MSYS:
```
$ pacman -S mingw-w64-x86_64-fltk
$ make
```
##### Example Arch Linux:
```
$ pacman -S fltk
$ make
```
##### Example MacOS:
```
$ brew install fltk
$ make
```
# Revision history

Please look at git commit log:

https://github.com/daansystems/radiodelay/commits/master
