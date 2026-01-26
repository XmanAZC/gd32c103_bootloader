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
	GD32C10x_Firmware_Library/CMSIS/GD/GD32C10x/Include
	GD32C10x_Firmware_Library/CMSIS
	GD32C10x_Firmware_Library/GD32C10x_standard_peripheral/Include
	freeRTOS/include
	freeRTOS/portable/GCC/ARM_CM4F
)

add_definitions(
	-DUSE_STDPERIPH_DRIVER
	-D_POSIX_C_SOURCE=1
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

set(PROJECT_SOURCES
	src/main.c
	src/syscall.c
	src/drv_simple_uart.c
	src/freertos_mpool.c
	src/ringbuffer.c
	GD32C10x_Firmware_Library/GD32C10x_standard_peripheral/Source/gd32c10x_dma.c
	GD32C10x_Firmware_Library/CMSIS/GD/GD32C10x/Source/system_gd32c10x.c
	GD32C10x_Firmware_Library/GD32C10x_standard_peripheral/Source/gd32c10x_usart.c
	GD32C10x_Firmware_Library/CMSIS/GD/GD32C10x/Source/GCC/startup_gd32c10x.s
	GD32C10x_Firmware_Library/GD32C10x_standard_peripheral/Source/gd32c10x_rcu.c
	GD32C10x_Firmware_Library/GD32C10x_standard_peripheral/Source/gd32c10x_gpio.c
	GD32C10x_Firmware_Library/GD32C10x_standard_peripheral/Source/gd32c10x_fmc.c
	GD32C10x_Firmware_Library/GD32C10x_standard_peripheral/Source/gd32c10x_misc.c
)

set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "FreeRTOS port to use")
set(FREERTOS_HEAP 4 CACHE STRING "FreeRTOS heap implementation to use")

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
)

add_library(bootloader_objects OBJECT ${PROJECT_SOURCES})

add_executable(bootloader.elf $<TARGET_OBJECTS:bootloader_objects>)
set_target_properties(bootloader.elf PROPERTIES OUTPUT_NAME "bootloader.elf")
target_link_options(bootloader.elf PRIVATE
    -Wl,--defsym,ROM_ORIGIN=0x08000000
    -Wl,--defsym,ROM_LENGTH=20k
)
add_custom_command(TARGET bootloader.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary bootloader.elf bootloader.bin
    COMMAND ${CMAKE_SIZE} bootloader.elf
)
