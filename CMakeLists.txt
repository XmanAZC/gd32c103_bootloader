cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)
#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TOOLCHAIN_ROOT "$ENV{GCC_ARM_NONE_EABI_TOOLCHAIN_PATH}")

set(CMAKE_C_COMPILER "${TOOLCHAIN_ROOT}/arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_ROOT}/arm-none-eabi-gcc")
set(CMAKE_C_FLAGS " -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -Dgcc -ffreestanding -nostdlib")
set(CMAKE_ASM_FLAGS " -c -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -x assembler-with-cpp -Wa,-mimplicit-it=thumb ")
set(CMAKE_C_COMPILER_WORKS TRUE)

set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/arm-none-eabi-g++")
set(CMAKE_CXX_FLAGS " -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -Dgcc -ffreestanding -nostdlib -fno-exceptions -fno-rtti")
set(CMAKE_CXX_COMPILER_WORKS TRUE)

set(CMAKE_OBJCOPY "${TOOLCHAIN_ROOT}/arm-none-eabi-objcopy")
set(CMAKE_SIZE "${TOOLCHAIN_ROOT}/arm-none-eabi-size")

set(CMAKE_EXE_LINKER_FLAGS " -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -Wl,--gc-sections,-cref,-u,Reset_Handler -T ${CMAKE_SOURCE_DIR}/link.ld")

set(CMAKE_CXX_STANDARD 14)
project(gd32c103_ab C CXX ASM)
include_directories(
	inc
    xlink
    xlink/xlink_generator
)

function(set_target_map_file target map_name)
    target_link_options(${target} PRIVATE
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${map_name}
    )
endfunction()

if(DEFINED ENV{DEBUG})
    add_definitions(-DDEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -gdwarf-2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -gdwarf-2")
else()
    add_definitions(-DNDEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

set(BOOTLOADER_SOURCES
	src/boot_loader_main.c
)

set(APP_SOURCES
    src/upgrade.c
	src/main.c
	src/syscall.c
	src/drv_simple_uart.c
	src/freertos_mpool.c
	src/onchip_flash_port.c
)

set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "FreeRTOS port to use")
set(FREERTOS_HEAP 4 CACHE STRING "FreeRTOS heap implementation to use")

add_subdirectory(GD32C10x_Firmware_Library INTERFACE)
add_library(freertos_config INTERFACE)

target_include_directories(freertos_config SYSTEM
INTERFACE
    inc
)

target_compile_definitions(freertos_config
  INTERFACE
    projCOVERAGE_TEST=0
)

add_subdirectory(freeRTOS)

link_directories(
)

link_libraries(
	m
	freertos_kernel
	freertos_config
	gd32c103
)
if(DEFINED ENV{DEBUG})
    message("Debug build")
    add_library(app_objects OBJECT ${APP_SOURCES})
    add_executable(${CMAKE_PROJECT_NAME}.elf $<TARGET_OBJECTS:app_objects>)
    set_target_properties(${CMAKE_PROJECT_NAME}.elf PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}.elf")
    set_target_map_file(${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.map)
    target_link_options(${CMAKE_PROJECT_NAME}.elf PRIVATE
        -Wl,--defsym,ROM_ORIGIN=0x08000000
        -Wl,--defsym,ROM_LENGTH=128k
    )
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
        COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf
    )
else()
    message("Release build")
    find_program(HOST_CXX_COMPILER NAMES g++ c++)
    if(NOT HOST_CXX_COMPILER)
        message(FATAL_ERROR "Host C++ compiler not found, required to build app_padding")
    endif()

    add_library(bootloader_objects OBJECT ${BOOTLOADER_SOURCES})

    add_executable(bootloader.elf $<TARGET_OBJECTS:bootloader_objects>)
    set_target_properties(bootloader.elf PROPERTIES OUTPUT_NAME "bootloader.elf")
    set_target_map_file(bootloader.elf bootloader.map)
    target_link_options(bootloader.elf PRIVATE
        -Wl,--defsym,ROM_ORIGIN=0x08000000
        -Wl,--defsym,ROM_LENGTH=4k
    )
    add_custom_command(TARGET bootloader.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary bootloader.elf bootloader.bin
        COMMAND ${CMAKE_SIZE} bootloader.elf
    )

    add_library(app_objects OBJECT ${APP_SOURCES})

    add_executable(appa.elf $<TARGET_OBJECTS:app_objects>)
    set_target_properties(appa.elf PROPERTIES OUTPUT_NAME "appa.elf")
    set_target_map_file(appa.elf appa.map)
    target_link_options(appa.elf PRIVATE
        -Wl,--defsym,ROM_ORIGIN=0x08002000
        -Wl,--defsym,ROM_LENGTH=50k
    )
    add_custom_command(TARGET appa.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary appa.elf appa.bin
        COMMAND ${CMAKE_SIZE} appa.elf
    )

    add_executable(appb.elf $<TARGET_OBJECTS:app_objects>)
    set_target_properties(appb.elf PROPERTIES OUTPUT_NAME "appb.elf")
    set_target_map_file(appb.elf appb.map)
    target_link_options(appb.elf PRIVATE
        -Wl,--defsym,ROM_ORIGIN=0x0800F000
        -Wl,--defsym,ROM_LENGTH=50k
    )
    add_custom_command(TARGET appb.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary appb.elf appb.bin
        COMMAND ${CMAKE_SIZE} appb.elf
    )

    add_custom_command(
        OUTPUT app_padding_tool
        COMMAND ${HOST_CXX_COMPILER} ${CMAKE_SOURCE_DIR}/app_padding.cpp -o app_padding_tool
        DEPENDS ${CMAKE_SOURCE_DIR}/app_padding.cpp ${CMAKE_SOURCE_DIR}/inc/partition.h
        COMMENT "Building host app_padding tool"
        VERBATIM
    )

    add_custom_command(
        OUTPUT upgrade
        COMMAND ${HOST_CXX_COMPILER} ${CMAKE_SOURCE_DIR}/upgrade.cpp -o upgrade
            -I${CMAKE_SOURCE_DIR}/xlink
            -I${CMAKE_SOURCE_DIR}/xlink/xlink_generator
            -I${CMAKE_SOURCE_DIR}/inc
        DEPENDS ${CMAKE_SOURCE_DIR}/upgrade.cpp
                 ${CMAKE_SOURCE_DIR}/xlink/xlink_generator/xlink_upgrade.h
                 ${CMAKE_SOURCE_DIR}/xlink/xlink.h
                 ${CMAKE_SOURCE_DIR}/inc/partition.h
        COMMENT "Building host upgrade tool"
        VERBATIM
    )

    add_custom_target(upgrade_tool ALL DEPENDS upgrade)

    add_custom_target(app_padding ALL DEPENDS app_padding_tool)

endif()
