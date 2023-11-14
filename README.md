Overview and Warning: 
=====
** NEEDS EDITING WIP MAY NEVER GET MUCH FARTHER ***

Files and Classes contained within this directory
========================


USB To Serial Adapter
-----
USBHostSerialDevice.cpp
USBHostSerialDevice.h
FasterSafeRingBuffer.h

GamePad
------
USBHostGamepadDevice.cpp
USBHostGamepadDevice.h

USB HID Parser
---
Reads in HID Descriptor and then parses Inputs using the descriptor

USBHostHIDParser.cpp
USBHostHIDParser.h


Keyboard - Uses HID
---
USBHostKeyboardEx. cpp
USBHostKeyboardEx.h

Mouse - Uses HID
===
USBHostMouseEx. cpp
USBHostMouseEx.h

External Files
========================
Currently the library is using code from other libraries.  Some of them
are avaialable through the Arduino libary manager and others through
Github, some of these can be removed over time, but for example prefer
to use printf versus several print or println statements.


From Library Manager
-----------------
#include <Arduino_USBHostMbed5.h>
#include <LibPrintf.h>

Currently from Github:
----------------------

MemoryHexDump - my library for dumping memory
https://github.com/KurtE/MemoryHexDump

Timer library 
This may be optional, without it may not have TX buffering

Has GIGA R1 support
https://github.com/KurtE/Portenta_H7_TimerInterrupt

The library was forked from the archived library 
https://github.com/khoih-prog/Portenta_H7_TimerInterrupt


Again WIP
=====
