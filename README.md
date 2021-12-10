USB-GPIB interface

This is a fork of gpib-usb-firmware-arm, initially published by Galvant (/ S. Casagrande).
Most of the code was restructured and modified.

## status : haha

## hardware
hw 1.00 : buggy PCB based on stm32F070 , with two GPIB driver ICs (SN75160 + SN75161/75162). Not released.

This should run on pretty much any ARM mcu (note) that is supported by libopencm3. I'm working with an STM32F072 discovery board.
(note): the one hard requirement is to have 8 consecutive GPIO pins that are 5V-tolerant. See firmware/hw_conf.h .

## getting & compiling
* clone this repo

* retrieve libopencm3 submodule :

`git submodule update --init`

compile locm3 :

* `cd libopencm3 && make TARGETS=stm32/f0`

compile the firmware and debug/flash :

* `cd firmware && make `
* `make debug`

There is also a rudimentary .gdbinit file included to help with debugging, particular to view peripheral register contents "easily".
Uses python, cmsis-svd (available on pip), and cmdebug ( https://github.com/bnahill/PyCortexMDebug )

It allows one to do things like 
"svd USART2" to view register contents.

## tests
there are is a separate Makefile in tests/ , meant to be compiled and run on the host system, that targets certain areas of code
(hash table for command matching, string processing, etc)