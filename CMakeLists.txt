cmake_minimum_required(VERSION 3.13)
project(gpibusb_cdc C ASM)

# based on CMake buildsys of candlelight-fw project

add_compile_options(
    -std=gnu11
    -mcpu=cortex-m0 -mthumb -msoft-float
    -Wall -Wextra -Werror
	-Wextra -Wshadow -Wimplicit-function-declaration
	-Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
#    -fsigned-char
    -ffunction-sections -fdata-sections
#    -ffreestanding
#    -fno-move-loop-invariants
    -Os -ggdb3
#    -flto -ffat-lto-objects
)

add_link_options(
	-mcpu=cortex-m0 -mthumb
    -Wall -Wextra -g3
	--static
	-nostartfiles
#	-lnosys
	-Wl,--gc-sections
    -specs=nano.specs
)
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake )


# Add a custom target that produces version.h, plus
# a dummy output that's not actually produced, in order
# to force version.hmake to always be re-run before the build


add_custom_target(version_h BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/version.h"
	COMMAND ${CMAKE_COMMAND}
		-D SRCDIR="${CMAKE_CURRENT_SOURCE_DIR}"
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gitversion.cmake
)

# add a custom target for libopencm3

set (OPENCM3_DIR ${CMAKE_SOURCE_DIR}/libopencm3)

add_custom_target(libopencm3
	COMMAND make TARGETS=stm32/f0 V=1
	COMMENT building libopencm3
	WORKING_DIRECTORY ${OPENCM3_DIR}
)



#add_subdirectory(libs/STM32_HAL)
#add_subdirectory(libs/STM32_USB_Device_Library)
#add_subdirectory(cdc_test)
add_subdirectory(firmware)


set( SOURCE_FILES
	src/startup.c
	src/main.c
	src/interrupts.c

	${CMAKE_CURRENT_BINARY_DIR}/version.h
)


####### some helpers to generate targets

## objcopy to produce .bin file
function(make_bin_file target)
    add_custom_command(
        TARGET ${target} POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        BYPRODUCTS ${target}.bin
        COMMAND ${CMAKE_OBJCOPY} -O binary ${target} ${target}.bin
    )
endfunction()

## report size
function(show_object_size target)
    string(REPLACE "objcopy" "size" CMAKE_OBJSIZE "${CMAKE_OBJCOPY}")
    add_custom_command(
        TARGET ${target} POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_OBJSIZE} ${target}
    )
endfunction()

find_package(DFUSuffix)
## run dfu-suffix to append DFU stuff and signature; generate relevant flash-* target
# TODO ? : run with execute_proces(... OUTPUT_QUIET ) instead of '... 1>/dev/null'

function(dfu_flash target)
	if (DFU_SUFFIX_EXECUTABLE)
		add_custom_command( TARGET ${target}
			BYPRODUCTS ${target}.dfu
			COMMAND ${CMAKE_OBJCOPY} -O binary ${target} ${target}.dfu
			COMMAND ${DFU_SUFFIX_EXECUTABLE} --add ${target}.dfu --vid 1d50 --pid 606f 1>/dev/null
			COMMENT "create and sign dfu bin file: ${TGTNAME}_fw"
		)
		add_custom_target( flash-${target}
			dfu-util -a 0 -R -s 0x08000000 -D ${target}.dfu
		)
	else()
		add_custom_target( flash-${target}
			dfu-util -d 1d50:606f -a 0 -R -s 0x08000000 -D ${target}.bin
			)
	endif()
endfunction()


######### commands for adding each target have a lot in common: make helper func.
# 

function(add_f070_target TGTNAME)
	add_executable(${TGTNAME}_fw ${SOURCE_FILES})
	add_dependencies(${TGTNAME}_fw version_h)
    target_include_directories(${TGTNAME}_fw PRIVATE include/ ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(${TGTNAME}_fw PRIVATE BOARD=BOARD_${TGTNAME})
	target_link_options(${TGTNAME}_fw PRIVATE -T ${CMAKE_SOURCE_DIR}/ldscripts/STM32F042X6.ld)
    target_link_libraries(${TGTNAME}_fw PRIVATE STM32_HAL_STM32F042x6 STM32_USB_Device_Library_STM32F042x6)
    make_bin_file(${TGTNAME}_fw)
    dfu_flash(${TGTNAME}_fw)
    show_object_size(${TGTNAME}_fw)
endfunction()



########## generate list of targets.
# the "_fw" part is appended automatically
#set(TGT070_LIST "gpibcdc_f070")


foreach (TGTNAME IN LISTS TGT070_LIST)
	option(BUILD_${TGTNAME} "Build firmware for \"${TGTNAME}\" (default=yes)" ON)
	if (BUILD_${TGTNAME})
		add_f070_target(${TGTNAME})
	endif()
endforeach()


message("*******************")
message("You may now:\n\t-compile all targets ('make')\n\t-compile a single target (e.g. 'make cantact_fw'")
message("\t-flash a device (e.g. 'make flash-cantact_fw'")
