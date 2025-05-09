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

set (TGTNAME firmware)


## Generate command parser hashtable with gperf.
# cmd_hashtable.c is part of the repo so gperf is not a hard requirement

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/cmd_hashtable.c
	COMMENT "Generating command hashtable"
	COMMAND gperf -T -m 4 --output-file cmd_hashtable.c cmd_hashtable.gen
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cmd_hashtable.gen
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

### build source file lists

# files that make sense to indent with uncrustify etc
set (FORMATTED_SRCS
	gpib.c
	cmd_parser.c
	hw_backend.c
	host_comms.c
	libc_stubs.c
	usb_cdc.c
	firmware.c
)

set (FW_SRCS
	${CMAKE_SOURCE_DIR}/printf/src/printf/printf.c
	${CMAKE_SOURCE_DIR}/etools/ecbuff.c
	cmd_hashtable.c
	${FORMATTED_SRCS}
)

## make sure printf.c uses our printf_config.h
set_source_files_properties(${CMAKE_SOURCE_DIR}/printf/src/printf/printf.c
	PROPERTIES COMPILE_OPTIONS "-DPRINTF_INCLUDE_CONFIG_H")


#and GLOB all the headers. This is *only* so that the headers end up in
#the file list of IDE projects (at least Code::blocks)
#but could be removed without impacting the build process
file (GLOB FW_HEADERS *.h)


### Now define targets.

add_custom_target(format
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/firmware
	COMMAND uncrustify --replace --no-backup
	-c ${CMAKE_SOURCE_DIR}/uncrustify.cfg
	${FORMATTED_SRCS} ${FW_HEADERS}
)

add_executable(${TGTNAME} ${FW_SRCS} ${FW_HEADERS})

#make sure libopencm3 builds first
add_dependencies(${TGTNAME} libopencm3)

target_compile_definitions(${TGTNAME} PRIVATE STM32F0)
target_link_options(${TGTNAME} PRIVATE -T ${CMAKE_SOURCE_DIR}/ldscripts/stm32f070.ld)
#target_link_options(${TGTNAME} PRIVATE -T ${CMAKE_SOURCE_DIR}/firmware/generated.stm32f070cb.ld)
target_link_options(${TGTNAME} PRIVATE -Wl,-Map=${TGTNAME}.map)

target_include_directories(${TGTNAME} PRIVATE
	${CMAKE_SOURCE_DIR}/firmware
	${CMAKE_SOURCE_DIR}/libopencm3/include
	${CMAKE_SOURCE_DIR}/libopencmsis/include
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/cmsis
	${CMAKE_SOURCE_DIR}/printf/src
	${CMAKE_SOURCE_DIR}/etools
)
target_link_directories(${TGTNAME} PRIVATE
	${CMAKE_SOURCE_DIR}/libopencm3/lib
)

target_link_libraries(${TGTNAME} opencm3_stm32f0)

#report size post-build
string(REPLACE "objcopy" "size" CMAKE_OBJSIZE "${CMAKE_OBJCOPY}")
add_custom_target(tgt_size ALL
	DEPENDS ${TGTNAME}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND ${CMAKE_OBJSIZE} ${TGTNAME}
)

#produce .bin file
add_custom_command(
	TARGET ${TGTNAME} POST_BUILD
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	BYPRODUCTS ${TGTNAME}.bin
	COMMAND ${CMAKE_OBJCOPY} -O binary ${TGTNAME} ${TGTNAME}.bin
)

#add debugging target. Fugly
string(REPLACE "objcopy" "gdb" GDB_CMD "${CMAKE_OBJCOPY}")
add_custom_target(debug
	COMMAND ${GDB_CMD} $<TARGET_FILE:${TGTNAME}> -x gdb_oocd.cmds
	DEPENDS ${TGTNAME}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
