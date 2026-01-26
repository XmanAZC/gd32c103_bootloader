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

set(CMAKE_EXE_LINKER_FLAGS " -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -Wl,--gc-sections,-Map=bootloader.map,-cref,-u,Reset_Handler -T ${CMAKE_SOURCE_DIR}/link.ld")

set(CMAKE_CXX_STANDARD 14)
project(bootloader C CXX ASM)
include_directories(
	inc
	xlink_generator
)

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
	src/syscall.c
	src/drv_simple_uart.c
	src/freertos_mpool.c
	src/onchip_flash_port.c
)

set(APP_SOURCES
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

add_library(bootloader_objects OBJECT ${BOOTLOADER_SOURCES})

add_executable(bootloader.elf $<TARGET_OBJECTS:bootloader_objects>)
set_target_properties(bootloader.elf PROPERTIES OUTPUT_NAME "bootloader.elf")
target_link_options(bootloader.elf PRIVATE
    -Wl,--defsym,ROM_ORIGIN=0x08000000
    -Wl,--defsym,ROM_LENGTH=16k
)
add_custom_command(TARGET bootloader.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary bootloader.elf bootloader.bin
    COMMAND ${CMAKE_SIZE} bootloader.elf
)

add_library(app_objects OBJECT ${APP_SOURCES})

add_executable(appa.elf $<TARGET_OBJECTS:app_objects>)
set_target_properties(appa.elf PROPERTIES OUTPUT_NAME "appa.elf")
target_link_options(appa.elf PRIVATE
    -Wl,--defsym,ROM_ORIGIN=0x08005000
    -Wl,--defsym,ROM_LENGTH=48k
)
add_custom_command(TARGET appa.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary appa.elf appa.bin
    COMMAND ${CMAKE_SIZE} appa.elf
)
add_executable(appb.elf $<TARGET_OBJECTS:app_objects>)
set_target_properties(appb.elf PROPERTIES OUTPUT_NAME "appb.elf")
target_link_options(appb.elf PRIVATE
    -Wl,--defsym,ROM_ORIGIN=0x08011800
    -Wl,--defsym,ROM_LENGTH=48k
)
add_custom_command(TARGET appb.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary appb.elf appb.bin
    COMMAND ${CMAKE_SIZE} appb.elf
)
