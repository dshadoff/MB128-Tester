# MB128-Tester
This project contains both a PC board to control (Backup, Restore, Test) PC Engine
Memory Base 128 devices, as well as the control software for that PC board.

The PC board is based on an 'Adafruit Feather M0 Adalogger' microcontroller board, which
directly interfaces to PC Engine Memory Base 128 devices.


This repository holds the software which runs on that board, as well as the board's design files.

The sketch was built within the Arduino IDE, but I found that the digitalWrite() and digitalRead()
functions were too slow: ~1uS per call, when I needed 3 to 4 times that speed.

Moreover, they could only manipulate one pin at a time, when I generally needed to write 2, or read 4.

As a result, you will see direct port reads/writes to the Cortex M0 ports... see the comments at the
top of the sketch if this is confusing.

(Note: Older versions of this sketch were written for an older version of the board, which didn't have
status LEDs or pushbutton switches, and automatically started to read the MB128 at boot-up time.)
