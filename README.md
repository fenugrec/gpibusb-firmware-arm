USB-GPIB interface

This is a fork of gpib-usb-firmware-arm, initially published by Galvant (/ S. Casagrande).
Most of the code was restructured and modified.

## status : haha

## hardware
Not sure of the details yet. Needs the two GPIB driver ICs (SN75160 + SN75161/75162).

This should run on pretty much any ARM mcu that is supported by libopencm3. I'm working with an STM32F072 discovery board.

## getting & compiling
* clone this repo

* retrieve libopencm3 submodule :

`git submodule update --init`

compile locm3 :

* `cd libopencm3 && make TARGETS=stm32/f0`

compile the firmware and debug/flash :

* `cd firmware && make `
* `make debug`