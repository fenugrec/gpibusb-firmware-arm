USB-GPIB interface

This is a fork of gpib-usb-firmware-arm, initially published by Galvant  S. Casagrande).
In the current state, only the GPIB layer and the command implementations were kept, but the rest was restructured and rewritten to run as a USB-CDC device on a single mcu.

## status
testing in progress.
- USB enumerates
- command processor responds to commands (++ver , ++srq etc)


## hardware
hw 1.00 : buggy PCB based on stm32F070 , with two GPIB driver ICs (SN75160 + SN75161/75162). Not released.
hw 1.01 : fixed dumb 1.00 bugs; WIP schematics in hw/ but PCB layout not done.

This should run on many ARM mcus supported by libopencm3 with :
 - basic timers
 - USB driver
 - 5V-tolerant pins (at least 8 consecutive pins for DIO signals, and a few control signals as well. See firmware/hw_conf.h )

My first batch of PCBs use an STM32F070 which is fairly limited and should be good lowest common denominator. Beware of low pin-count devices that may lack the 5V-tolerant pins.


## getting & compiling
* clone this repo

* retrieve submodules :

`git submodule update --init`

* compile using cmake : (find a sample gcctoolchain file in cmake/) 
` mkdir build && cd build && cmake-gui ..`

compile the firmware and debug :

* `make`
* `make debug`

There is also a rudimentary .gdbinit file included to help with debugging, particular to view peripheral register contents "easily".
Uses python, cmsis-svd (available on pip), and cmdebug ( https://github.com/bnahill/PyCortexMDebug )

It allows one to do things like 
"svd USART2" to view register contents.

## tests
there are is a separate Makefile in tests/ , meant to be compiled and run on the host system, that targets certain areas of code
(hash table for command matching, string processing, etc)
