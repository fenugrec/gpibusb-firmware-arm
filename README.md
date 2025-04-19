USB-GPIB interface

This is a fork of gpib-usb-firmware-arm, initially published by Galvant  S. Casagrande), but has diverged quite a bit.
This implements a USB-CDC serial port on a single mcu.

## status
Most of the functionality appears to work :

- HP 3478A with lmester's software ( https://mesterhome.com/gpibsw/hp3478a/index.html )
- TDS 744A with KE5FX's 7470 plotter emulator ( http://www.ke5fx.com/gpib/7470.htm ), needs some tweaking of timeouts though.


## hardware
hw 1.00 : buggy PCB based on stm32F070 and two GPIB driver ICs (SN75160 + SN75161/75162). Not released.
hw 1.01 : fixed dumb 1.00 bugs; WIP schematics in hw/ but PCB layout not done.
As of 2021, the IC shortage is such that there's no point in making a schematic nor layout - nothing is available.
When things become more reasonable, I would probably go with an stm32f042 that can do crystal-less USB.


This should run on many ARM mcus supported by libopencm3 with :
 - basic timers
 - USB device driver
 - 5V-tolerant pins (at least 8 consecutive pins for DIO signals, and a few control signals as well. See firmware/hw_conf.h )

In addition, I've tried to write the code in a portable way as much as possible ; this is not perfect but most of the device-specific code is split out in different files.

My first batch of PCBs use an STM32F070 which is fairly limited and should be good lowest common denominator. Beware of low pin-count devices that may lack the 5V-tolerant pins.


## getting & compiling
* clone this repo

* retrieve submodules :

`git submodule update --init`

* compile using cmake : (find a sample gcctoolchain file in cmake/) 
` mkdir build && cd build && cmake-gui ..`

compile the firmware , flash and debug :

* `make`
* `dfu-util -d 0483:df11 -a 0 -R -s 0x08000000:leave -D firmware/firmware.bin`
* `make debug`

There is a rudimentary .gdbinit file included to help with debugging, particular to view peripheral register contents "easily".
Uses python, cmsis-svd (available on pip), and cmdebug ( https://github.com/bnahill/PyCortexMDebug )

It allows one to do things like 
"svd USART2" to view register contents.


## Using
Refer to the Prologix manual. http://prologix.biz/downloads/PrologixGpibUsbManual-6.0.pdf
Do *not* contact Prologix in case of problems using this firmware !

Additional custom commands:
- `++debug [0|1]`, enable debugging output. The extra messages may interfere with some software.
- `++dfu` , reset into DFU mode for reflashing.
- `++help`, list available commands, and print some system stats.


## tests
there are is a separate Makefile in tests/ , meant to be compiled and run on the host system, that targets certain areas of code
(hash table for command matching, string processing, etc). Not always up to date.
