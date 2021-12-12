##
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

BINARY = firmware
OBJS = ring.o gpib.o cmd_parser.o hw_backend.o host_comms.o libc_stubs.o \
	cmd_hashtable.o

OPENCM3_DIR=../libopencm3
DEVICE=stm32f070cb
#LDSCRIPT = ../stm32f0-discovery.ld

include ../libopencm3.target.mk

#all: $(BINARY).bin
#	@$(SIZE) $(BINARY).elf

cmd_hashtable.c: cmd_hashtable.gen
	gperf -m 4 --output-file cmd_hashtable.c cmd_hashtable.gen

#qemu target doesn't work unless we compile for  lm3s811evb, or create a new qemu machine.
qemu: $(BINARY).elf
	qemu-system-arm -machine lm3s811evb -cpu cortex-m3 -nographic -monitor null -serial null -s -S -kernel $(BINARY).elf
#	st-flash write $(BINARY).bin 0x8000000

debug:
	@$(GDB) $(BINARY).elf -x gdb_oocd.cmds
