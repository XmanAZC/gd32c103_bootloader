#ifndef ONCHIP_FLASH_PORT_H
#define ONCHIP_FLASH_PORT_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 0x400U

static inline size_t size_to_pages(size_t size)
{
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

void fmc_erase_pages(uint32_t page_address, uint32_t pages);

int fmc_erase_pages_check(uint32_t page_address, uint32_t pages);

void fmc_program_word(uint32_t address, void *data, uint32_t size);

uint32_t fmc_program_data(uint32_t address, void *data, uint32_t size);

#endif // !ONCHIP_FLASH_PORT_H
