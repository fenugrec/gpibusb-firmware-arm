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
OBJS = ring.o gpib.o cmd_parser.o hw_backend.o

OPENCM3_DIR=../libopencm3
LDSCRIPT = ../stm32f0-discovery.ld

include ../libopencm3.target.mk

all: $(BINARY).bin

cmd_hashtable.c: cmd_hashtable.gen
	gperf -m 4 --output-file cmd_hashtable.c cmd_hashtable.gen

#	st-flash write $(BINARY).bin 0x8000000

