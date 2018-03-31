USB-GPIB interface

## status : haha

## hardware
Not sure of the details yet. Needs the two GPIB driver ICs (SN75160 + SN75161/75162).

This should run on pretty much any ARM mcu that is supported by libopencm3. I'm working with an STM32F072 discovery board.

## getting & compiling
* clone this repo

* retrieve libopencm3 submodule :

`git submodule init && git submodule update`

compile locm3 :

* `cd libopencm3 && make TARGETS=stm32/f0`

